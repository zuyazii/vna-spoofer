"""
VNA Settings Management

This module implements VNA settings configuration with validation
and optimization for different test scenarios.
"""

import numpy as np
from typing import List, Optional, Dict, Any, Union, Tuple
from dataclasses import dataclass, field
import logging

logger = logging.getLogger(__name__)


@dataclass
class VNASettings:
    """
    VNA Settings Configuration
    
    This class defines all VNA settings parameters with validation
    and optimization capabilities for different measurement scenarios.
    """
    
    # Frequency configuration
    start_frequency: float = 1e6  # Hz
    stop_frequency: float = 6e9   # Hz
    point_count: int = 201
    
    # Measurement parameters
    ifbw: int = 1000  # Hz
    power_level: float = -10.0  # dBm
    averaging: int = 1
    
    # Port configuration
    excited_ports: List[int] = field(default_factory=lambda: [1])
    measured_ports: List[int] = field(default_factory=lambda: [1])
    
    # Advanced settings
    sweep_mode: str = "linear"  # linear, log, list
    sweep_direction: str = "forward"  # forward, reverse, both
    trigger_source: str = "internal"  # internal, external, manual
    
    # Calibration settings
    calibration_enabled: bool = True
    calibration_type: str = "SOLT"  # SOLT, SOL, TRL, etc.
    
    # Output settings
    data_format: str = "complex"  # complex, magnitude_phase, real_imag
    output_format: str = "s_parameters"  # s_parameters, raw_data
    
    def __post_init__(self):
        """Validate settings after initialization"""
        self._validate_settings()
    
    def _validate_settings(self):
        """Validate VNA settings parameters"""
        if self.start_frequency >= self.stop_frequency:
            raise ValueError("Start frequency must be less than stop frequency")
        
        if self.point_count < 2:
            raise ValueError("Point count must be at least 2")
        
        if self.ifbw <= 0:
            raise ValueError("IFBW must be positive")
        
        if self.averaging < 1:
            raise ValueError("Averaging must be at least 1")
        
        if not self.excited_ports:
            raise ValueError("At least one excited port must be specified")
        
        if not self.measured_ports:
            raise ValueError("At least one measured port must be specified")
        
        if self.sweep_mode not in ["linear", "log", "list"]:
            raise ValueError("Sweep mode must be 'linear', 'log', or 'list'")
        
        if self.sweep_direction not in ["forward", "reverse", "both"]:
            raise ValueError("Sweep direction must be 'forward', 'reverse', or 'both'")
        
        if self.trigger_source not in ["internal", "external", "manual"]:
            raise ValueError("Trigger source must be 'internal', 'external', or 'manual'")
        
        if self.data_format not in ["complex", "magnitude_phase", "real_imag"]:
            raise ValueError("Data format must be 'complex', 'magnitude_phase', or 'real_imag'")
    
    def get_frequency_array(self) -> np.ndarray:
        """Get frequency array based on sweep mode"""
        if self.sweep_mode == "linear":
            return np.linspace(self.start_frequency, self.stop_frequency, self.point_count)
        elif self.sweep_mode == "log":
            return np.logspace(
                np.log10(self.start_frequency), 
                np.log10(self.stop_frequency), 
                self.point_count
            )
        else:
            raise ValueError("List mode requires explicit frequency list")
    
    def get_sweep_time_estimate(self) -> float:
        """
        Estimate sweep time based on settings
        
        Returns:
            Estimated sweep time in seconds
        """
        # Base time per point (rough estimate)
        base_time_per_point = 0.001  # 1 ms per point
        
        # Adjust for averaging
        time_per_point = base_time_per_point * self.averaging
        
        # Adjust for IFBW (lower IFBW = longer settling time)
        ifbw_factor = max(0.1, 1000.0 / self.ifbw)
        time_per_point *= ifbw_factor
        
        # Total sweep time
        total_time = time_per_point * self.point_count
        
        # Add overhead for sweep setup
        overhead = 0.5  # 500 ms overhead
        
        return total_time + overhead
    
    def optimize_for_speed(self) -> 'VNASettings':
        """
        Optimize settings for maximum speed
        
        Returns:
            New VNASettings optimized for speed
        """
        optimized = VNASettings(
            start_frequency=self.start_frequency,
            stop_frequency=self.stop_frequency,
            point_count=min(self.point_count, 101),  # Reduce points
            ifbw=max(self.ifbw, 10000),  # Increase IFBW
            power_level=self.power_level,
            averaging=1,  # No averaging
            excited_ports=self.excited_ports,
            measured_ports=self.measured_ports,
            sweep_mode=self.sweep_mode,
            sweep_direction="forward",  # Single direction
            trigger_source="internal",
            calibration_enabled=self.calibration_enabled,
            calibration_type=self.calibration_type,
            data_format=self.data_format,
            output_format=self.output_format
        )
        
        return optimized
    
    def optimize_for_accuracy(self) -> 'VNASettings':
        """
        Optimize settings for maximum accuracy
        
        Returns:
            New VNASettings optimized for accuracy
        """
        optimized = VNASettings(
            start_frequency=self.start_frequency,
            stop_frequency=self.stop_frequency,
            point_count=max(self.point_count, 401),  # Increase points
            ifbw=min(self.ifbw, 1000),  # Decrease IFBW
            power_level=self.power_level,
            averaging=max(self.averaging, 4),  # Increase averaging
            excited_ports=self.excited_ports,
            measured_ports=self.measured_ports,
            sweep_mode=self.sweep_mode,
            sweep_direction="both",  # Both directions
            trigger_source="internal",
            calibration_enabled=True,  # Always enable calibration
            calibration_type=self.calibration_type,
            data_format=self.data_format,
            output_format=self.output_format
        )
        
        return optimized
    
    def optimize_for_s11_only(self) -> 'VNASettings':
        """
        Optimize settings for S11-only measurements
        
        Returns:
            New VNASettings optimized for S11 measurements
        """
        optimized = VNASettings(
            start_frequency=self.start_frequency,
            stop_frequency=self.stop_frequency,
            point_count=self.point_count,
            ifbw=self.ifbw,
            power_level=self.power_level,
            averaging=self.averaging,
            excited_ports=[1],  # Only port 1
            measured_ports=[1],  # Only port 1
            sweep_mode=self.sweep_mode,
            sweep_direction=self.sweep_direction,
            trigger_source=self.trigger_source,
            calibration_enabled=self.calibration_enabled,
            calibration_type="SOL",  # S11 only needs SOL
            data_format=self.data_format,
            output_format=self.output_format
        )
        
        return optimized
    
    def get_measurement_matrix_size(self) -> Tuple[int, int]:
        """
        Get size of measurement matrix
        
        Returns:
            Tuple of (rows, columns) for measurement matrix
        """
        rows = len(self.measured_ports)
        cols = len(self.excited_ports)
        return (rows, cols)
    
    def get_required_s_parameters(self) -> List[str]:
        """
        Get list of required S-parameters for this configuration
        
        Returns:
            List of S-parameter names (e.g., ['S11', 'S21'])
        """
        s_params = []
        
        for measured_port in self.measured_ports:
            for excited_port in self.excited_ports:
                s_param = f"S{measured_port}{excited_port}"
                s_params.append(s_param)
        
        return s_params
    
    def to_dict(self) -> Dict[str, Any]:
        """Convert settings to dictionary"""
        return {
            'start_frequency': self.start_frequency,
            'stop_frequency': self.stop_frequency,
            'point_count': self.point_count,
            'ifbw': self.ifbw,
            'power_level': self.power_level,
            'averaging': self.averaging,
            'excited_ports': self.excited_ports,
            'measured_ports': self.measured_ports,
            'sweep_mode': self.sweep_mode,
            'sweep_direction': self.sweep_direction,
            'trigger_source': self.trigger_source,
            'calibration_enabled': self.calibration_enabled,
            'calibration_type': self.calibration_type,
            'data_format': self.data_format,
            'output_format': self.output_format
        }
    
    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> 'VNASettings':
        """Create VNASettings from dictionary"""
        return cls(**data)
    
    def copy(self) -> 'VNASettings':
        """Create a copy of the settings"""
        return VNASettings(
            start_frequency=self.start_frequency,
            stop_frequency=self.stop_frequency,
            point_count=self.point_count,
            ifbw=self.ifbw,
            power_level=self.power_level,
            averaging=self.averaging,
            excited_ports=self.excited_ports.copy(),
            measured_ports=self.measured_ports.copy(),
            sweep_mode=self.sweep_mode,
            sweep_direction=self.sweep_direction,
            trigger_source=self.trigger_source,
            calibration_enabled=self.calibration_enabled,
            calibration_type=self.calibration_type,
            data_format=self.data_format,
            output_format=self.output_format
        )
    
    def __str__(self) -> str:
        """String representation of settings"""
        return (f"VNASettings({self.start_frequency/1e6:.1f}-{self.stop_frequency/1e6:.1f} MHz, "
                f"{self.point_count} pts, IFBW={self.ifbw} Hz, Power={self.power_level} dBm)")
    
    def __repr__(self) -> str:
        """Detailed string representation"""
        return (f"VNASettings(start_freq={self.start_frequency}, stop_freq={self.stop_frequency}, "
                f"points={self.point_count}, ifbw={self.ifbw}, power={self.power_level}, "
                f"avg={self.averaging}, excited={self.excited_ports}, measured={self.measured_ports})")


class VNASettingsPreset:
    """Predefined VNA settings for common test scenarios"""
    
    @staticmethod
    def fast_sweep() -> VNASettings:
        """Fast sweep settings for quick measurements"""
        return VNASettings(
            start_frequency=1e6,
            stop_frequency=6e9,
            point_count=101,
            ifbw=10000,
            power_level=-10.0,
            averaging=1,
            excited_ports=[1],
            measured_ports=[1]
        )
    
    @staticmethod
    def high_accuracy() -> VNASettings:
        """High accuracy settings for precise measurements"""
        return VNASettings(
            start_frequency=1e6,
            stop_frequency=6e9,
            point_count=401,
            ifbw=1000,
            power_level=-10.0,
            averaging=8,
            excited_ports=[1],
            measured_ports=[1],
            sweep_direction="both"
        )
    
    @staticmethod
    def antenna_test() -> VNASettings:
        """Settings optimized for antenna testing"""
        return VNASettings(
            start_frequency=100e6,
            stop_frequency=3e9,
            point_count=201,
            ifbw=3000,
            power_level=-5.0,
            averaging=4,
            excited_ports=[1],
            measured_ports=[1],
            calibration_type="SOL"
        )
    
    @staticmethod
    def filter_test() -> VNASettings:
        """Settings optimized for filter testing"""
        return VNASettings(
            start_frequency=1e6,
            stop_frequency=6e9,
            point_count=301,
            ifbw=2000,
            power_level=-10.0,
            averaging=2,
            excited_ports=[1],
            measured_ports=[1, 2],
            calibration_type="SOLT"
        )
    
    @staticmethod
    def low_frequency() -> VNASettings:
        """Settings for low frequency measurements"""
        return VNASettings(
            start_frequency=1e3,
            stop_frequency=100e6,
            point_count=201,
            ifbw=1000,
            power_level=-10.0,
            averaging=4,
            excited_ports=[1],
            measured_ports=[1]
        )
    
    @staticmethod
    def high_frequency() -> VNASettings:
        """Settings for high frequency measurements"""
        return VNASettings(
            start_frequency=1e9,
            stop_frequency=18e9,
            point_count=201,
            ifbw=5000,
            power_level=-5.0,
            averaging=2,
            excited_ports=[1],
            measured_ports=[1]
        )
