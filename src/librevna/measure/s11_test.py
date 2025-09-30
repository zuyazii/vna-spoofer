"""
S11 Test Implementation

This module implements complete S11 testing automation with pass/fail evaluation,
configurable thresholds, and comprehensive result analysis.
"""

import numpy as np
from typing import Dict, List, Optional, Tuple, Any, Union
from dataclasses import dataclass, field
import logging
from enum import Enum
import time

from ..calibration import CalibrationData, Corrector
from ..device.vna_settings import VNASettings

logger = logging.getLogger(__name__)


class TestStatus(Enum):
    """Test execution status"""
    PENDING = "pending"
    RUNNING = "running"
    COMPLETED = "completed"
    FAILED = "failed"
    CANCELLED = "cancelled"


class PassFailResult(Enum):
    """Pass/fail evaluation result"""
    PASS = "pass"
    FAIL = "fail"
    MARGINAL = "marginal"
    ERROR = "error"


@dataclass
class S11TestConfig:
    """
    Configuration for S11 test execution
    
    This class defines all parameters needed for S11 testing including
    frequency ranges, thresholds, and evaluation criteria.
    """
    
    # Frequency configuration
    start_frequency: float = 1e6  # Hz
    stop_frequency: float = 6e9   # Hz
    point_count: int = 201
    
    # Test evaluation
    pass_threshold_db: float = -10.0  # dB
    fail_threshold_db: float = -5.0   # dB
    evaluation_band_start: Optional[float] = None  # Hz, None = use start_frequency
    evaluation_band_stop: Optional[float] = None   # Hz, None = use stop_frequency
    
    # Measurement parameters
    ifbw: int = 1000  # Hz
    power_level: float = -10.0  # dBm
    averaging: int = 1
    
    # Calibration
    calibration_file: Optional[str] = None
    apply_calibration: bool = True
    
    # Output configuration
    save_raw_data: bool = True
    save_corrected_data: bool = True
    output_format: List[str] = field(default_factory=lambda: ['json', 'csv'])
    
    # Test execution
    timeout_seconds: int = 300  # 5 minutes
    retry_count: int = 3
    retry_delay: float = 1.0  # seconds
    
    def __post_init__(self):
        """Validate configuration after initialization"""
        self._validate_config()
    
    def _validate_config(self):
        """Validate configuration parameters"""
        if self.start_frequency >= self.stop_frequency:
            raise ValueError("Start frequency must be less than stop frequency")
        
        if self.point_count < 2:
            raise ValueError("Point count must be at least 2")
        
        if self.pass_threshold_db >= self.fail_threshold_db:
            raise ValueError("Pass threshold must be less than fail threshold")
        
        if self.ifbw <= 0:
            raise ValueError("IFBW must be positive")
        
        if self.averaging < 1:
            raise ValueError("Averaging must be at least 1")
        
        if self.timeout_seconds <= 0:
            raise ValueError("Timeout must be positive")
        
        if self.retry_count < 0:
            raise ValueError("Retry count cannot be negative")
        
        # Set evaluation band defaults
        if self.evaluation_band_start is None:
            self.evaluation_band_start = self.start_frequency
        if self.evaluation_band_stop is None:
            self.evaluation_band_stop = self.stop_frequency
        
        # Validate evaluation band
        if (self.evaluation_band_start < self.start_frequency or 
            self.evaluation_band_stop > self.stop_frequency):
            raise ValueError("Evaluation band must be within measurement frequency range")
    
    def get_frequency_array(self) -> np.ndarray:
        """Get frequency array for measurement"""
        return np.linspace(self.start_frequency, self.stop_frequency, self.point_count)
    
    def get_evaluation_mask(self, frequencies: np.ndarray) -> np.ndarray:
        """Get boolean mask for evaluation band"""
        # Handle None values by using start/stop frequencies as defaults
        start_freq = self.evaluation_band_start if self.evaluation_band_start is not None else self.start_frequency
        stop_freq = self.evaluation_band_stop if self.evaluation_band_stop is not None else self.stop_frequency
        
        return ((frequencies >= start_freq) & 
                (frequencies <= stop_freq))
    
    def to_vna_settings(self) -> VNASettings:
        """Convert to VNASettings for device configuration"""
        return VNASettings(
            start_frequency=self.start_frequency,
            stop_frequency=self.stop_frequency,
            point_count=self.point_count,
            ifbw=self.ifbw,
            power_level=self.power_level,
            averaging=self.averaging,
            excited_ports=[1]  # S11 measurement uses port 1
        )


@dataclass
class S11TestResult:
    """
    Results from S11 test execution
    
    This class contains all results from S11 testing including
    measurements, pass/fail evaluation, and analysis data.
    """
    
    # Test configuration
    config: S11TestConfig
    
    # Test execution
    status: TestStatus = TestStatus.PENDING
    start_time: Optional[float] = None
    end_time: Optional[float] = None
    duration: Optional[float] = None
    
    # Measurement data
    frequencies: np.ndarray = field(default_factory=lambda: np.array([]))
    raw_s11: np.ndarray = field(default_factory=lambda: np.array([]))
    corrected_s11: np.ndarray = field(default_factory=lambda: np.array([]))
    
    # Evaluation results
    pass_fail_result: PassFailResult = PassFailResult.ERROR
    worst_case_db: Optional[float] = None
    worst_case_frequency: Optional[float] = None
    evaluation_band_db: Optional[float] = None
    
    # Statistics
    min_db: Optional[float] = None
    max_db: Optional[float] = None
    mean_db: Optional[float] = None
    std_db: Optional[float] = None
    
    # Error information
    error_message: Optional[str] = None
    retry_count: int = 0
    
    # Calibration information
    calibration_applied: bool = False
    calibration_file: Optional[str] = None
    
    def __post_init__(self):
        """Calculate derived values after initialization"""
        if len(self.corrected_s11) > 0:
            self._calculate_statistics()
    
    def _calculate_statistics(self):
        """Calculate statistical measures from corrected S11 data"""
        if len(self.corrected_s11) == 0:
            return
        
        # Convert to dB
        s11_db = 20 * np.log10(np.abs(self.corrected_s11))
        
        # Calculate statistics
        self.min_db = float(np.min(s11_db))
        self.max_db = float(np.max(s11_db))
        self.mean_db = float(np.mean(s11_db))
        self.std_db = float(np.std(s11_db))
        
        # Find worst case in evaluation band
        eval_mask = self.config.get_evaluation_mask(self.frequencies)
        if np.any(eval_mask):
            eval_s11_db = s11_db[eval_mask]
            eval_freqs = self.frequencies[eval_mask]
            
            worst_idx = np.argmax(eval_s11_db)  # Highest (worst) value
            self.worst_case_db = float(eval_s11_db[worst_idx])
            self.worst_case_frequency = float(eval_freqs[worst_idx])
            self.evaluation_band_db = self.worst_case_db
    
    def evaluate_pass_fail(self) -> PassFailResult:
        """
        Evaluate pass/fail based on worst case in evaluation band
        
        Returns:
            PassFailResult indicating test outcome
        """
        if self.worst_case_db is None:
            return PassFailResult.ERROR
        
        if self.worst_case_db <= self.config.pass_threshold_db:
            return PassFailResult.PASS
        elif self.worst_case_db <= self.config.fail_threshold_db:
            return PassFailResult.MARGINAL
        else:
            return PassFailResult.FAIL
    
    def get_summary(self) -> Dict[str, Any]:
        """Get summary of test results"""
        return {
            'status': self.status.value,
            'pass_fail': self.pass_fail_result.value,
            'duration': self.duration,
            'worst_case_db': self.worst_case_db,
            'worst_case_frequency': self.worst_case_frequency,
            'evaluation_band_db': self.evaluation_band_db,
            'statistics': {
                'min_db': self.min_db,
                'max_db': self.max_db,
                'mean_db': self.mean_db,
                'std_db': self.std_db
            },
            'calibration_applied': self.calibration_applied,
            'calibration_file': self.calibration_file,
            'error_message': self.error_message,
            'retry_count': self.retry_count
        }


class S11Test:
    """
    S11 Test Automation Engine
    
    This class orchestrates complete S11 testing including measurement
    collection, calibration application, and pass/fail evaluation.
    """
    
    def __init__(self, device_manager=None, calibration_data: Optional[CalibrationData] = None):
        """
        Initialize S11 test engine
        
        Args:
            device_manager: Device manager for VNA communication
            calibration_data: Pre-loaded calibration data
        """
        self.device_manager = device_manager
        self.calibration_data = calibration_data
        self.corrector = None
        
        if calibration_data:
            self.corrector = Corrector(calibration_data)
    
    def load_calibration(self, calibration_file: str) -> None:
        """
        Load calibration data from file
        
        Args:
            calibration_file: Path to calibration file
        """
        from ..calibration import CalFile
        
        cal_file = CalFile(calibration_file)
        self.calibration_data = CalibrationData.from_cal_points(cal_file.calibration_points)
        self.corrector = Corrector(self.calibration_data)
        
        logger.info(f"Loaded calibration from: {calibration_file}")
    
    def run_test(self, config: S11TestConfig) -> S11TestResult:
        """
        Run complete S11 test with given configuration
        
        Args:
            config: S11 test configuration
            
        Returns:
            S11TestResult with test results
        """
        result = S11TestResult(config=config)
        result.start_time = time.time()
        result.status = TestStatus.RUNNING
        
        try:
            logger.info(f"Starting S11 test: {config.start_frequency/1e6:.1f}-{config.stop_frequency/1e6:.1f} MHz, {config.point_count} points")
            
            # Load calibration if specified
            if config.calibration_file and not self.corrector:
                self.load_calibration(config.calibration_file)
                result.calibration_file = config.calibration_file
            
            # Configure device
            vna_settings = config.to_vna_settings()
            if self.device_manager:
                self._configure_device(vna_settings)
            
            # Perform measurement
            frequencies, raw_s11 = self._perform_measurement(config)
            result.frequencies = frequencies
            result.raw_s11 = raw_s11
            
            # Apply calibration if available
            if self.corrector and config.apply_calibration:
                corrected_s11 = self._apply_calibration(frequencies, raw_s11)
                result.corrected_s11 = corrected_s11
                result.calibration_applied = True
            else:
                result.corrected_s11 = raw_s11
                result.calibration_applied = False
            
            # Calculate statistics and evaluate
            result._calculate_statistics()
            result.pass_fail_result = result.evaluate_pass_fail()
            
            result.status = TestStatus.COMPLETED
            if result.worst_case_db is not None and result.worst_case_frequency is not None:
                logger.info(f"S11 test completed: {result.pass_fail_result.value}, worst case: {result.worst_case_db:.2f} dB @ {result.worst_case_frequency/1e6:.1f} MHz")
            else:
                logger.info(f"S11 test completed: {result.pass_fail_result.value}")
            
        except Exception as e:
            result.status = TestStatus.FAILED
            result.error_message = str(e)
            logger.error(f"S11 test failed: {e}")
        
        finally:
            result.end_time = time.time()
            result.duration = result.end_time - result.start_time
        
        return result
    
    def _configure_device(self, settings: VNASettings) -> None:
        """
        Configure VNA device with given settings
        
        Args:
            settings: VNA settings to apply
        """
        if not self.device_manager:
            logger.warning("No device manager available, skipping device configuration")
            return
        
        try:
            # This would interface with the actual device
            # For now, we'll log the configuration
            logger.info(f"Configuring VNA: {settings.start_frequency/1e6:.1f}-{settings.stop_frequency/1e6:.1f} MHz, {settings.point_count} points")
            logger.info(f"IFBW: {settings.ifbw} Hz, Power: {settings.power_level} dBm, Averaging: {settings.averaging}")
            
        except Exception as e:
            logger.error(f"Failed to configure device: {e}")
            raise
    
    def _perform_measurement(self, config: S11TestConfig) -> Tuple[np.ndarray, np.ndarray]:
        """
        Perform S11 measurement sweep
        
        Args:
            config: Test configuration
            
        Returns:
            Tuple of (frequencies, raw_s11_measurements)
        """
        frequencies = config.get_frequency_array()
        
        if not self.device_manager:
            raise ConnectionError("No device manager available. Cannot perform measurement without VNA device.")
        
        try:
            # Interface with the actual device
            logger.info(f"Performing S11 measurement sweep: {len(frequencies)} points")
            
            # Configure device for measurement
            settings = config.to_vna_settings()
            self._configure_device(settings)
            
            # Perform actual measurement sweep
            raw_s11 = self._measure_s11_sweep(frequencies, settings)
            
            return frequencies, raw_s11
            
        except Exception as e:
            logger.error(f"Measurement failed: {e}")
            raise
    
    def _measure_s11_sweep(self, frequencies: np.ndarray, settings: VNASettings) -> np.ndarray:
        """
        Perform S11 measurement sweep using the device
        
        Args:
            frequencies: Frequency array
            settings: VNA settings
            
        Returns:
            Raw S11 measurements
        """
        if not self.device_manager:
            raise ConnectionError("No device manager available. Cannot perform measurement without VNA device.")
        
        try:
            device = self.device_manager
            
            # Configure the device for measurement
            logger.info("Configuring device for S11 measurement")
            device.configure_vna(
                start_freq=settings.start_frequency,
                stop_freq=settings.stop_frequency,
                points=settings.point_count,
                ifbw=settings.ifbw,
                power=settings.power_level,
                averaging=settings.averaging
            )
            
            # Perform the measurement sweep
            logger.info(f"Performing real S11 measurement sweep: {len(frequencies)} points")
            frequencies_list = frequencies.tolist()
            s11_measurements = device.measure_s11_sweep(frequencies_list)
            
            # Log progress for long sweeps
            if len(frequencies) > 10:
                for i in range(0, len(frequencies), max(1, len(frequencies) // 10)):
                    progress = (i + 1) / len(frequencies) * 100
                    logger.info(f"Measurement progress: {progress:.0f}% ({i+1}/{len(frequencies)})")
            
            logger.info("Real S11 measurement completed successfully")
            return np.array(s11_measurements)
            
        except Exception as e:
            logger.error(f"Real measurement failed: {e}")
            raise
    
    def _apply_calibration(self, frequencies: np.ndarray, raw_s11: np.ndarray) -> np.ndarray:
        """
        Apply calibration correction to raw measurements
        
        Args:
            frequencies: Frequency array
            raw_s11: Raw S11 measurements
            
        Returns:
            Corrected S11 measurements
        """
        if not self.corrector:
            logger.warning("No corrector available, returning raw measurements")
            return raw_s11
        
        try:
            corrected = self.corrector.correct_measurement_array(
                frequencies, raw_s11, 's11'
            )
            return corrected
            
        except Exception as e:
            logger.error(f"Calibration application failed: {e}")
            return raw_s11
    
    
    def run_batch_tests(self, configs: List[S11TestConfig]) -> List[S11TestResult]:
        """
        Run multiple S11 tests in batch
        
        Args:
            configs: List of test configurations
            
        Returns:
            List of test results
        """
        results = []
        
        for i, config in enumerate(configs):
            logger.info(f"Running batch test {i+1}/{len(configs)}")
            result = self.run_test(config)
            results.append(result)
            
            # Add delay between tests if needed
            if i < len(configs) - 1:
                time.sleep(0.5)
        
        return results
    
    def validate_config(self, config: S11TestConfig) -> Dict[str, Any]:
        """
        Validate test configuration
        
        Args:
            config: Test configuration to validate
            
        Returns:
            Dictionary with validation results
        """
        validation = {
            'is_valid': True,
            'warnings': [],
            'errors': []
        }
        
        try:
            # This will raise exceptions if invalid
            config._validate_config()
        except ValueError as e:
            validation['is_valid'] = False
            validation['errors'].append(str(e))
        
        # Check calibration file if specified
        if config.calibration_file:
            import os
            if not os.path.exists(config.calibration_file):
                validation['warnings'].append(f"Calibration file not found: {config.calibration_file}")
        
        # Check frequency range against calibration
        if self.calibration_data:
            cal_min, cal_max = self.calibration_data.get_frequency_range()
            if (config.start_frequency < cal_min or config.stop_frequency > cal_max):
                validation['warnings'].append(
                    f"Test frequency range ({config.start_frequency/1e6:.1f}-{config.stop_frequency/1e6:.1f} MHz) "
                    f"extends beyond calibration range ({cal_min/1e6:.1f}-{cal_max/1e6:.1f} MHz)"
                )
        
        return validation
