"""
Calibration Data Structures

This module defines data structures for calibration data handling,
including numpy array operations and frequency interpolation utilities.
"""

import numpy as np
from typing import List, Tuple, Optional, Dict, Any
from dataclasses import dataclass, field
import logging

logger = logging.getLogger(__name__)


@dataclass
class CalibrationData:
    """
    Container for calibration data with numpy array support
    
    This class provides efficient storage and manipulation of calibration
    data using numpy arrays for mathematical operations.
    """
    
    frequencies: np.ndarray = field(default_factory=lambda: np.array([]))
    error_terms: Dict[str, np.ndarray] = field(default_factory=dict)
    calibration_type: str = "Unknown"
    used_ports: List[int] = field(default_factory=list)
    
    def __post_init__(self):
        """Initialize error terms arrays if not provided"""
        if not self.error_terms:
            self.error_terms = {
                'D': np.array([]),  # Directivity
                'R': np.array([]),  # Reflection tracking
                'S': np.array([]),  # Source match
                'L': np.array([]),  # Load match
                'T': np.array([]),  # Transmission tracking
                'I': np.array([]),  # Isolation
                'E00': np.array([]),  # 1-port directivity
                'E11': np.array([]),  # 1-port source match
                'E10E01': np.array([])  # 1-port reflection tracking
            }
        
        # Ensure all error terms are initialized
        required_terms = ['D', 'R', 'S', 'L', 'T', 'I', 'E00', 'E11', 'E10E01']
        for term in required_terms:
            if term not in self.error_terms:
                self.error_terms[term] = np.array([])
    
    @classmethod
    def from_cal_points(cls, cal_points: List, calibration_type: str = "Unknown", 
                       used_ports: List[int] = None) -> 'CalibrationData':
        """
        Create CalibrationData from list of CalPoint objects
        
        Args:
            cal_points: List of CalPoint objects
            calibration_type: Type of calibration
            used_ports: List of used port numbers
            
        Returns:
            CalibrationData instance
        """
        if not cal_points:
            return cls()
        
        # Extract frequencies
        frequencies = np.array([point.frequency for point in cal_points])
        
        # Extract error terms
        error_terms = {
            'D': np.array([point.D for point in cal_points], dtype=complex),
            'R': np.array([point.R for point in cal_points], dtype=complex),
            'S': np.array([point.S for point in cal_points], dtype=complex),
            'L': np.array([point.L for point in cal_points], dtype=complex),
            'T': np.array([point.T for point in cal_points], dtype=complex),
            'I': np.array([point.I for point in cal_points], dtype=complex),
            'E00': np.array([point.E00 for point in cal_points], dtype=complex),
            'E11': np.array([point.E11 for point in cal_points], dtype=complex),
            'E10E01': np.array([point.E10E01 for point in cal_points], dtype=complex)
        }
        
        return cls(
            frequencies=frequencies,
            error_terms=error_terms,
            calibration_type=calibration_type,
            used_ports=used_ports or []
        )
    
    def get_frequency_range(self) -> Tuple[float, float]:
        """
        Get frequency range of calibration data
        
        Returns:
            Tuple of (min_frequency, max_frequency) in Hz
        """
        if len(self.frequencies) == 0:
            return (0.0, 0.0)
        
        return (float(np.min(self.frequencies)), float(np.max(self.frequencies)))
    
    def is_frequency_in_range(self, frequency: float) -> bool:
        """
        Check if frequency is within calibration range
        
        Args:
            frequency: Frequency to check in Hz
            
        Returns:
            True if frequency is within range
        """
        if len(self.frequencies) == 0:
            return False
        
        min_freq, max_freq = self.get_frequency_range()
        return min_freq <= frequency <= max_freq
    
    def get_error_term_at_frequency(self, term_name: str, frequency: float) -> Optional[complex]:
        """
        Get error term value at specific frequency (exact match)
        
        Args:
            term_name: Name of error term ('D', 'R', 'S', etc.)
            frequency: Frequency in Hz
            
        Returns:
            Error term value if exact match found, None otherwise
        """
        if term_name not in self.error_terms:
            return None
        
        # Find exact frequency match
        for i, freq in enumerate(self.frequencies):
            if abs(freq - frequency) < 1e-6:
                return self.error_terms[term_name][i]
        
        return None
    
    def get_interpolated_error_term(self, term_name: str, frequency: float, 
                                  method: str = 'linear') -> Optional[complex]:
        """
        Get interpolated error term value at specific frequency
        
        Args:
            term_name: Name of error term ('D', 'R', 'S', etc.)
            frequency: Frequency in Hz
            method: Interpolation method ('linear', 'nearest')
            
        Returns:
            Interpolated error term value
        """
        if term_name not in self.error_terms or len(self.frequencies) == 0:
            return None
        
        if method == 'nearest':
            # Find nearest frequency
            idx = np.argmin(np.abs(self.frequencies - frequency))
            return self.error_terms[term_name][idx]
        
        elif method == 'linear':
            # Linear interpolation
            if len(self.frequencies) < 2:
                return self.error_terms[term_name][0] if len(self.error_terms[term_name]) > 0 else None
            
            # Check if error term array is empty
            if len(self.error_terms[term_name]) == 0:
                return None
            
            # Check if frequency is within range
            if not self.is_frequency_in_range(frequency):
                logger.warning(f"Frequency {frequency} outside calibration range")
                return None
            
            # Find surrounding points
            idx = np.searchsorted(self.frequencies, frequency)
            
            if idx == 0:
                return self.error_terms[term_name][0]
            elif idx == len(self.frequencies):
                return self.error_terms[term_name][-1]
            else:
                # Linear interpolation
                f1, f2 = self.frequencies[idx-1], self.frequencies[idx]
                v1, v2 = self.error_terms[term_name][idx-1], self.error_terms[term_name][idx]
                
                # Interpolate real and imaginary parts separately
                weight = (frequency - f1) / (f2 - f1)
                interpolated = v1 + weight * (v2 - v1)
                return interpolated
        
        else:
            raise ValueError(f"Unknown interpolation method: {method}")
    
    def get_all_error_terms_at_frequency(self, frequency: float, 
                                       method: str = 'linear') -> Dict[str, complex]:
        """
        Get all error terms at specific frequency
        
        Args:
            frequency: Frequency in Hz
            method: Interpolation method
            
        Returns:
            Dictionary of error terms
        """
        terms = {}
        for term_name in self.error_terms:
            terms[term_name] = self.get_interpolated_error_term(term_name, frequency, method)
        return terms
    
    def validate_calibration(self) -> Dict[str, Any]:
        """
        Validate calibration data quality
        
        Returns:
            Dictionary with validation results
        """
        validation = {
            'is_valid': True,
            'warnings': [],
            'errors': [],
            'statistics': {}
        }
        
        # Check if we have data
        if len(self.frequencies) == 0:
            validation['is_valid'] = False
            validation['errors'].append("No frequency points")
            return validation
        
        # Check frequency ordering
        if not np.all(np.diff(self.frequencies) > 0):
            validation['warnings'].append("Frequencies not in ascending order")
        
        # Check for duplicate frequencies
        if len(np.unique(self.frequencies)) != len(self.frequencies):
            validation['warnings'].append("Duplicate frequencies found")
        
        # Check error terms consistency (only for terms that have been set)
        for term_name, values in self.error_terms.items():
            if len(values) > 0 and len(values) != len(self.frequencies):
                validation['errors'].append(f"Error term {term_name} length mismatch")
                validation['is_valid'] = False
        
        # Check if error terms are all zero (potential issue)
        all_zero_terms = []
        for term_name, values in self.error_terms.items():
            if np.allclose(values, 0, atol=1e-10):
                all_zero_terms.append(term_name)
        
        if len(all_zero_terms) == len(self.error_terms):
            validation['warnings'].append("All error terms are zero - calibration may be invalid")
        
        # Calculate statistics
        validation['statistics'] = {
            'frequency_points': len(self.frequencies),
            'frequency_range': self.get_frequency_range(),
            'all_zero_terms': all_zero_terms,
            'calibration_type': self.calibration_type,
            'used_ports': self.used_ports
        }
        
        return validation
    
    def subset_by_frequency_range(self, min_freq: float, max_freq: float) -> 'CalibrationData':
        """
        Create subset of calibration data within frequency range
        
        Args:
            min_freq: Minimum frequency in Hz
            max_freq: Maximum frequency in Hz
            
        Returns:
            New CalibrationData instance with subset
        """
        # Find indices within range
        mask = (self.frequencies >= min_freq) & (self.frequencies <= max_freq)
        
        if not np.any(mask):
            return CalibrationData()
        
        # Create subset
        subset_frequencies = self.frequencies[mask]
        subset_error_terms = {}
        
        for term_name, values in self.error_terms.items():
            subset_error_terms[term_name] = values[mask]
        
        return CalibrationData(
            frequencies=subset_frequencies,
            error_terms=subset_error_terms,
            calibration_type=self.calibration_type,
            used_ports=self.used_ports.copy()
        )
    
    def to_dict(self) -> Dict[str, Any]:
        """
        Convert calibration data to dictionary format
        
        Returns:
            Dictionary representation
        """
        return {
            'frequencies': self.frequencies.tolist(),
            'error_terms': {name: values.tolist() for name, values in self.error_terms.items()},
            'calibration_type': self.calibration_type,
            'used_ports': self.used_ports
        }
    
    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> 'CalibrationData':
        """
        Create CalibrationData from dictionary
        
        Args:
            data: Dictionary with calibration data
            
        Returns:
            CalibrationData instance
        """
        frequencies = np.array(data.get('frequencies', []))
        error_terms = {}
        
        for term_name, values in data.get('error_terms', {}).items():
            error_terms[term_name] = np.array(values, dtype=complex)
        
        return cls(
            frequencies=frequencies,
            error_terms=error_terms,
            calibration_type=data.get('calibration_type', 'Unknown'),
            used_ports=data.get('used_ports', [])
        )
    
    def get_calibration_info(self) -> Dict[str, Any]:
        """
        Get summary information about the calibration
        
        Returns:
            Dictionary with calibration information
        """
        return {
            'calibration_type': self.calibration_type,
            'used_ports': self.used_ports,
            'frequency_points': len(self.frequencies),
            'frequency_range': self.get_frequency_range(),
            'has_valid_terms': any(
                len(values) > 0 and not np.allclose(values, 0, atol=1e-10)
                for values in self.error_terms.values()
            )
        }
