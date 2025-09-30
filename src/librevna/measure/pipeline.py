"""
Measurement Pipeline

This module implements the complete measurement collection workflow with
real-time data processing, correction, and quality validation.
"""

import numpy as np
from typing import Dict, List, Optional, Tuple, Any, Callable, Union
from dataclasses import dataclass, field
import logging
import time
from enum import Enum
import threading
from queue import Queue, Empty

from .s11_test import S11TestResult, TestStatus
from ..device.vna_settings import VNASettings
from ..device.realtime_receiver import RealtimeReceiver
from ..calibration import CalibrationData, Corrector

logger = logging.getLogger(__name__)


class PipelineStatus(Enum):
    """Pipeline execution status"""
    IDLE = "idle"
    RUNNING = "running"
    PAUSED = "paused"
    COMPLETED = "completed"
    FAILED = "failed"
    CANCELLED = "cancelled"


class DataQuality(Enum):
    """Data quality assessment"""
    EXCELLENT = "excellent"
    GOOD = "good"
    FAIR = "fair"
    POOR = "poor"
    INVALID = "invalid"


@dataclass
class MeasurementPoint:
    """Single measurement point with metadata"""
    frequency: float
    s_parameters: Dict[str, complex]
    timestamp: float
    quality: DataQuality = DataQuality.GOOD
    raw_data: Optional[Dict[str, Any]] = None


@dataclass
class MeasurementResult:
    """
    Complete measurement result with processing information
    
    This class contains all measurement data along with processing
    metadata and quality assessment.
    """
    
    # Configuration
    settings: VNASettings
    
    # Measurement data
    frequencies: np.ndarray = field(default_factory=lambda: np.array([]))
    s_parameters: Dict[str, np.ndarray] = field(default_factory=dict)
    raw_data: Dict[str, Any] = field(default_factory=dict)
    
    # Processing information
    calibration_applied: bool = False
    calibration_file: Optional[str] = None
    processing_time: float = 0.0
    
    # Quality assessment
    overall_quality: DataQuality = DataQuality.GOOD
    quality_metrics: Dict[str, float] = field(default_factory=dict)
    
    # Metadata
    start_time: Optional[float] = None
    end_time: Optional[float] = None
    duration: Optional[float] = None
    point_count: int = 0
    valid_points: int = 0
    
    # Error information
    errors: List[str] = field(default_factory=list)
    warnings: List[str] = field(default_factory=list)
    
    def __post_init__(self):
        """Calculate derived values after initialization"""
        if self.start_time and self.end_time:
            self.duration = self.end_time - self.start_time
        
        if len(self.frequencies) > 0:
            self.point_count = len(self.frequencies)
            self.valid_points = self._count_valid_points()
        else:
            self.point_count = 0
            self.valid_points = 0
    
    def _count_valid_points(self) -> int:
        """Count valid measurement points"""
        if not self.s_parameters:
            return 0
        
        # Check first S-parameter for validity
        first_param = list(self.s_parameters.values())[0]
        if len(first_param) == 0:
            return 0
        
        # Count non-NaN, non-infinite values
        valid_count = 0
        for param_name, values in self.s_parameters.items():
            valid_mask = np.isfinite(values)
            valid_count = max(valid_count, np.sum(valid_mask))
        
        return valid_count
    
    def get_quality_summary(self) -> Dict[str, Any]:
        """Get summary of data quality assessment"""
        return {
            'overall_quality': self.overall_quality.value,
            'quality_metrics': self.quality_metrics,
            'point_count': self.point_count,
            'valid_points': self.valid_points,
            'validity_ratio': self.valid_points / max(1, self.point_count),
            'errors': self.errors,
            'warnings': self.warnings
        }
    
    def is_valid(self) -> bool:
        """Check if measurement result is valid"""
        return (self.overall_quality != DataQuality.INVALID and 
                self.valid_points > 0 and
                len(self.errors) == 0)


class MeasurementPipeline:
    """
    Measurement Pipeline Engine
    
    This class orchestrates the complete measurement workflow including
    data collection, real-time processing, and quality validation.
    """
    
    def __init__(self, device_manager=None, corrector: Optional[Corrector] = None):
        """
        Initialize measurement pipeline
        
        Args:
            device_manager: Device manager for VNA communication
            corrector: Calibration corrector for data processing
        """
        self.device_manager = device_manager
        self.corrector = corrector
        
        # Pipeline state
        self.status = PipelineStatus.IDLE
        self.current_result: Optional[MeasurementResult] = None
        
        # Real-time processing
        self.data_queue = Queue()
        self.processing_thread: Optional[threading.Thread] = None
        self.stop_event = threading.Event()
        
        # Real-time receiver
        self.realtime_receiver: Optional[RealtimeReceiver] = None
        self.measurement_data: Dict[str, Any] = {}
        self.measurement_lock = threading.Lock()
        
        # Callbacks
        self.progress_callback: Optional[Callable[[float], None]] = None
        self.data_callback: Optional[Callable[[MeasurementPoint], None]] = None
        self.quality_callback: Optional[Callable[[DataQuality], None]] = None
        
        # Configuration
        self.enable_real_time_processing = True
        self.quality_check_interval = 10  # Check quality every N points
        self.max_queue_size = 1000
    
    def set_progress_callback(self, callback: Callable[[float], None]) -> None:
        """Set callback for progress updates"""
        self.progress_callback = callback
    
    def set_data_callback(self, callback: Callable[[MeasurementPoint], None]) -> None:
        """Set callback for real-time data"""
        self.data_callback = callback
    
    def set_quality_callback(self, callback: Callable[[DataQuality], None]) -> None:
        """Set callback for quality updates"""
        self.quality_callback = callback
    
    def run_measurement(self, settings: VNASettings) -> MeasurementResult:
        """
        Run complete measurement with given settings
        
        Args:
            settings: VNA settings for measurement
            
        Returns:
            MeasurementResult with complete measurement data
        """
        if self.status == PipelineStatus.RUNNING:
            raise RuntimeError("Pipeline is already running")
        
        self.status = PipelineStatus.RUNNING
        self.stop_event.clear()
        
        # Initialize result
        result = MeasurementResult(settings=settings)
        result.start_time = time.time()
        
        try:
            logger.info(f"Starting measurement pipeline: {settings}")
            
            # Configure device
            self._configure_device(settings)
            
            # Start real-time processing if enabled
            if self.enable_real_time_processing:
                self._start_processing_thread()
            
            # Perform measurement
            frequencies, s_parameters = self._collect_measurement_data(settings, result)
            
            # Stop real-time processing
            if self.enable_real_time_processing:
                self._stop_processing_thread()
            
            # Process final data
            result.frequencies = frequencies
            result.s_parameters = s_parameters
            
            # Apply calibration if available
            if self.corrector:
                result = self._apply_calibration(result)
            
            # Assess data quality
            result = self._assess_data_quality(result)
            
            # Update point count after data is set
            result.point_count = len(frequencies)
            result.valid_points = result._count_valid_points()
            
            result.status = PipelineStatus.COMPLETED
            logger.info(f"Measurement completed: {result.point_count} points, quality: {result.overall_quality.value}")
            
        except Exception as e:
            result.status = PipelineStatus.FAILED
            result.errors.append(str(e))
            logger.error(f"Measurement failed: {e}")
        
        finally:
            result.end_time = time.time()
            result.duration = result.end_time - result.start_time
            self.status = PipelineStatus.IDLE
        
        return result
    
    def _configure_device(self, settings: VNASettings) -> None:
        """Configure VNA device with given settings"""
        if not self.device_manager:
            logger.warning("No device manager available, skipping device configuration")
            return
        
        try:
            logger.info(f"Configuring device: {settings}")
            # This would interface with the actual device
            # For now, we'll simulate configuration
            time.sleep(0.1)
            
        except Exception as e:
            logger.error(f"Device configuration failed: {e}")
            raise
    
    def _collect_measurement_data(self, settings: VNASettings, 
                                result: MeasurementResult) -> Tuple[np.ndarray, Dict[str, np.ndarray]]:
        """
        Collect measurement data from device using real-time receiver
        
        Args:
            settings: VNA settings
            result: Measurement result object for progress tracking
            
        Returns:
            Tuple of (frequencies, s_parameters)
        """
        frequencies = settings.get_frequency_array()
        s_parameters = {}
        
        # Initialize S-parameter arrays
        for param_name in settings.get_required_s_parameters():
            s_parameters[param_name] = np.zeros(len(frequencies), dtype=complex)
        
        # Initialize measurement data storage
        with self.measurement_lock:
            self.measurement_data = {
                'frequencies': frequencies,
                's_parameters': s_parameters,
                'points_received': 0,
                'expected_points': len(frequencies)
            }
        
        # Start real-time receiver if not already running
        if not self.realtime_receiver or not self.realtime_receiver.is_running():
            self._start_realtime_receiver()
        
        # Wait for measurement data to be collected
        start_time = time.time()
        timeout = 30.0  # 30 second timeout
        
        while not self.stop_event.is_set():
            with self.measurement_lock:
                points_received = self.measurement_data['points_received']
                expected_points = self.measurement_data['expected_points']
            
            # Check if we have all points
            if points_received >= expected_points:
                break
            
            # Check timeout
            if time.time() - start_time > timeout:
                logger.warning(f"Measurement timeout: received {points_received}/{expected_points} points")
                break
            
            # Update progress
            progress = points_received / expected_points
            if self.progress_callback:
                self.progress_callback(progress)
            
            # Short sleep to prevent busy waiting
            time.sleep(0.01)
        
        # Get final measurement data
        with self.measurement_lock:
            s_parameters = self.measurement_data['s_parameters'].copy()
        
        return frequencies, s_parameters
    
    def _start_realtime_receiver(self) -> None:
        """Start real-time receiver for measurement data"""
        if not self.device_manager or not hasattr(self.device_manager, 'get_transport'):
            raise ConnectionError("Device manager not available or no transport")
        
        transport = self.device_manager.get_transport()
        if not transport:
            raise ConnectionError("No transport available")
        
        # Create real-time receiver with callbacks
        self.realtime_receiver = RealtimeReceiver(
            transport=transport,
            on_measurement=self._on_measurement_received,
            on_error=self._on_receiver_error,
            on_status=self._on_receiver_status
        )
        
        # Start receiver
        self.realtime_receiver.start()
        logger.info("Real-time receiver started for measurement")
    
    def _on_measurement_received(self, measurement: Dict[str, Any]) -> None:
        """Handle measurement data received from real-time receiver"""
        try:
            with self.measurement_lock:
                if 'points_received' not in self.measurement_data:
                    return
                
                points_received = self.measurement_data['points_received']
                expected_points = self.measurement_data['expected_points']
                
                if points_received >= expected_points:
                    return  # Already have all points
                
                # Extract S-parameters from measurement
                s_params = measurement.get('s_parameters', {})
                frequency_or_time = measurement.get('frequency_or_time', 0)
                
                # For now, assume frequency is provided directly
                # In a real implementation, we'd need to map frequency to array index
                if 's11' in s_params:
                    self.measurement_data['s_parameters']['S11'][points_received] = s_params['s11']
                
                if 's21' in s_params:
                    self.measurement_data['s_parameters']['S21'][points_received] = s_params['s21']
                
                # Update point count
                self.measurement_data['points_received'] += 1
                
                # Create measurement point for callbacks
                measurement_point = MeasurementPoint(
                    frequency=frequency_or_time / 1e9 if frequency_or_time > 1e6 else frequency_or_time,
                    s_parameters=s_params,
                    timestamp=time.time()
                )
                
                # Add to real-time processing queue
                if self.enable_real_time_processing:
                    try:
                        self.data_queue.put_nowait(measurement_point)
                    except:
                        pass  # Queue full, skip
                
                # Call data callback
                if self.data_callback:
                    self.data_callback(measurement_point)
                
        except Exception as e:
            logger.error(f"Error processing measurement data: {e}")
    
    def _on_receiver_error(self, error: Exception) -> None:
        """Handle errors from real-time receiver"""
        logger.error(f"Real-time receiver error: {error}")
        
        # Update quality if we have a quality callback
        if self.quality_callback:
            self.quality_callback(DataQuality.INVALID)
    
    def _on_receiver_status(self, status: str) -> None:
        """Handle status updates from real-time receiver"""
        logger.info(f"Real-time receiver status: {status}")
    
    def _measure_single_point(self, frequency: float, settings: VNASettings) -> Dict[str, complex]:
        """
        Measure single frequency point
        
        Args:
            frequency: Frequency to measure
            settings: VNA settings
            
        Returns:
            Dictionary of S-parameter measurements
        """
        if not self.device_manager:
            raise ConnectionError("No device manager available. Cannot perform measurement without VNA device.")
        
        try:
            # Interface with the actual device
            device = self.device_manager
            
            # Measure S11 at this frequency
            s11_measurement = device.measure_single_s11(frequency)
            
            # Return S-parameter measurements
            s_params = {}
            for param_name in settings.get_required_s_parameters():
                if param_name == "S11":
                    s_params[param_name] = s11_measurement
                else:
                    # For other S-parameters, we would need to implement measurement
                    # For now, raise an error if other parameters are requested
                    raise NotImplementedError(f"Measurement of {param_name} not yet implemented")
            
            return s_params
            
        except Exception as e:
            logger.error(f"Single point measurement failed at {frequency/1e6:.1f} MHz: {e}")
            raise
    
    
    def _start_processing_thread(self) -> None:
        """Start real-time data processing thread"""
        if self.processing_thread and self.processing_thread.is_alive():
            return
        
        self.processing_thread = threading.Thread(target=self._processing_worker)
        self.processing_thread.daemon = True
        self.processing_thread.start()
    
    def _stop_processing_thread(self) -> None:
        """Stop real-time data processing thread"""
        self.stop_event.set()
        
        if self.processing_thread and self.processing_thread.is_alive():
            self.processing_thread.join(timeout=1.0)
    
    def _processing_worker(self) -> None:
        """Worker thread for real-time data processing"""
        processed_count = 0
        
        while not self.stop_event.is_set():
            try:
                # Get data from queue with timeout
                measurement_point = self.data_queue.get(timeout=0.1)
                
                # Process the measurement point
                self._process_measurement_point(measurement_point)
                
                # Call data callback
                if self.data_callback:
                    self.data_callback(measurement_point)
                
                processed_count += 1
                
                # Check quality periodically
                if processed_count % self.quality_check_interval == 0:
                    quality = self._assess_realtime_quality()
                    if self.quality_callback:
                        self.quality_callback(quality)
                
            except Empty:
                # No data available, continue
                continue
            except Exception as e:
                logger.error(f"Real-time processing error: {e}")
                break
    
    def _process_measurement_point(self, point: MeasurementPoint) -> None:
        """Process single measurement point"""
        # Apply calibration if available
        if self.corrector:
            for param_name, value in point.s_parameters.items():
                if param_name == "S11":
                    corrected = self.corrector.correct_s11_measurement(point.frequency, value)
                    point.s_parameters[param_name] = corrected
        
        # Assess point quality
        point.quality = self._assess_point_quality(point)
    
    def _assess_point_quality(self, point: MeasurementPoint) -> DataQuality:
        """Assess quality of single measurement point"""
        # Check for invalid values
        for value in point.s_parameters.values():
            if not np.isfinite(value):
                return DataQuality.INVALID
        
        # Check magnitude ranges (basic sanity check)
        for param_name, value in point.s_parameters.items():
            magnitude = abs(value)
            if magnitude > 1.0:  # S-parameters should be <= 1
                return DataQuality.POOR
            elif magnitude > 0.9:
                return DataQuality.FAIR
            elif magnitude > 0.5:
                return DataQuality.GOOD
            else:
                return DataQuality.EXCELLENT
        
        return DataQuality.GOOD
    
    def _assess_realtime_quality(self) -> DataQuality:
        """Assess overall quality from recent measurements"""
        # This is a simplified real-time quality assessment
        # In practice, this would analyze recent data points
        return DataQuality.GOOD
    
    def _apply_calibration(self, result: MeasurementResult) -> MeasurementResult:
        """Apply calibration correction to measurement result"""
        if not self.corrector:
            return result
        
        try:
            logger.info("Applying calibration correction")
            
            # Apply correction to each S-parameter
            for param_name, values in result.s_parameters.items():
                if param_name == "S11":
                    corrected = self.corrector.correct_measurement_array(
                        result.frequencies, values, 's11'
                    )
                    result.s_parameters[param_name] = corrected
            
            result.calibration_applied = True
            result.processing_time += 0.1  # Simulate processing time
            
        except Exception as e:
            logger.error(f"Calibration application failed: {e}")
            result.warnings.append(f"Calibration failed: {e}")
        
        return result
    
    def _assess_data_quality(self, result: MeasurementResult) -> MeasurementResult:
        """Assess overall data quality"""
        if result.point_count == 0:
            result.overall_quality = DataQuality.INVALID
            return result
        
        # Calculate quality metrics
        quality_metrics = {}
        
        # Validity ratio
        validity_ratio = result.valid_points / result.point_count
        quality_metrics['validity_ratio'] = validity_ratio
        
        # Check for invalid data
        if validity_ratio < 0.5:
            result.overall_quality = DataQuality.INVALID
        elif validity_ratio < 0.8:
            result.overall_quality = DataQuality.POOR
        elif validity_ratio < 0.95:
            result.overall_quality = DataQuality.FAIR
        elif validity_ratio < 0.99:
            result.overall_quality = DataQuality.GOOD
        else:
            result.overall_quality = DataQuality.EXCELLENT
        
        # Additional quality checks
        for param_name, values in result.s_parameters.items():
            if len(values) == 0:
                continue
            
            # Check for reasonable S-parameter magnitudes
            magnitudes = np.abs(values)
            max_magnitude = np.max(magnitudes)
            quality_metrics[f'{param_name}_max_magnitude'] = max_magnitude
            
            if max_magnitude > 1.0:
                result.warnings.append(f"{param_name} magnitude exceeds 1.0")
                if result.overall_quality == DataQuality.EXCELLENT:
                    result.overall_quality = DataQuality.GOOD
        
        result.quality_metrics = quality_metrics
        return result
    
    def cancel_measurement(self) -> None:
        """Cancel running measurement"""
        if self.status == PipelineStatus.RUNNING:
            logger.info("Cancelling measurement")
            self.stop_event.set()
            self.status = PipelineStatus.CANCELLED
    
    def pause_measurement(self) -> None:
        """Pause running measurement"""
        if self.status == PipelineStatus.RUNNING:
            self.status = PipelineStatus.PAUSED
            logger.info("Measurement paused")
    
    def resume_measurement(self) -> None:
        """Resume paused measurement"""
        if self.status == PipelineStatus.PAUSED:
            self.status = PipelineStatus.RUNNING
            logger.info("Measurement resumed")
    
    def get_status(self) -> Dict[str, Any]:
        """Get current pipeline status"""
        return {
            'status': self.status.value,
            'queue_size': self.data_queue.qsize(),
            'processing_active': (self.processing_thread and 
                                self.processing_thread.is_alive()),
            'current_result': self.current_result.get_summary() if self.current_result else None
        }
