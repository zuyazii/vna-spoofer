"""
Mathematical Utilities for Calibration

This module provides mathematical functions for calibration calculations,
including complex matrix operations and interpolation algorithms.
"""

import numpy as np
from typing import Tuple, List, Optional, Union
import logging
from scipy import interpolate
import warnings

logger = logging.getLogger(__name__)

# Import warning counter from corrector
try:
    from .corrector import _warning_counts
except ImportError:
    # Fallback if import fails
    _warning_counts = {'denominator_near_zero': 0}


class MathUtils:
    """
    Mathematical utilities for calibration calculations
    
    This class provides methods for:
    - Complex matrix operations
    - Interpolation algorithms
    - Calibration correction mathematics
    - Validation functions
    """
    
    @staticmethod
    def complex_interpolate(frequencies: np.ndarray, values: np.ndarray, 
                          target_freq: float, method: str = 'linear') -> complex:
        """
        Interpolate complex values at target frequency
        
        Args:
            frequencies: Array of frequencies
            values: Array of complex values
            target_freq: Target frequency for interpolation
            method: Interpolation method ('linear', 'cubic', 'nearest')
            
        Returns:
            Interpolated complex value
        """
        if len(frequencies) == 0 or len(values) == 0:
            return complex(0)
        
        if len(frequencies) == 1:
            return values[0]
        
        # Handle edge cases
        if target_freq <= frequencies[0]:
            return values[0]
        if target_freq >= frequencies[-1]:
            return values[-1]
        
        if method == 'nearest':
            idx = np.argmin(np.abs(frequencies - target_freq))
            return values[idx]
        
        elif method == 'linear':
            # Linear interpolation
            idx = np.searchsorted(frequencies, target_freq)
            
            if idx == 0:
                return values[0]
            elif idx == len(frequencies):
                return values[-1]
            else:
                f1, f2 = frequencies[idx-1], frequencies[idx]
                v1, v2 = values[idx-1], values[idx]
                
                weight = (target_freq - f1) / (f2 - f1)
                return v1 + weight * (v2 - v1)
        
        elif method == 'cubic':
            # Cubic spline interpolation
            try:
                # Interpolate real and imaginary parts separately
                real_parts = np.real(values)
                imag_parts = np.imag(values)
                
                real_interp = interpolate.interp1d(frequencies, real_parts, 
                                                 kind='cubic', bounds_error=False, 
                                                 fill_value='extrapolate')
                imag_interp = interpolate.interp1d(frequencies, imag_parts, 
                                                 kind='cubic', bounds_error=False, 
                                                 fill_value='extrapolate')
                
                real_val = real_interp(target_freq)
                imag_val = imag_interp(target_freq)
                
                return complex(real_val, imag_val)
            
            except Exception as e:
                logger.warning(f"Cubic interpolation failed, falling back to linear: {e}")
                return MathUtils.complex_interpolate(frequencies, values, target_freq, 'linear')
        
        else:
            raise ValueError(f"Unknown interpolation method: {method}")
    
    @staticmethod
    def interpolate_error_terms(frequencies: np.ndarray, error_terms: dict, 
                              target_freq: float, method: str = 'linear') -> dict:
        """
        Interpolate all error terms at target frequency
        
        Args:
            frequencies: Array of frequencies
            error_terms: Dictionary of error term arrays
            target_freq: Target frequency
            method: Interpolation method
            
        Returns:
            Dictionary of interpolated error terms
        """
        interpolated = {}
        
        for term_name, values in error_terms.items():
            interpolated[term_name] = MathUtils.complex_interpolate(
                frequencies, values, target_freq, method
            )
        
        return interpolated
    
    @staticmethod
    def s11_correction_1port(raw_s11: complex, e00: complex, e11: complex, 
                           e10e01: complex) -> complex:
        """
        Apply 1-port calibration correction to S11 measurement
        
        Args:
            raw_s11: Raw S11 measurement
            e00: Directivity error term
            e11: Source match error term
            e10e01: Reflection tracking error term
            
        Returns:
            Corrected S11 value
        """
        # 1-port correction formula: S11_corrected = (S11_raw - E00) / (E10E01 + E11 * (S11_raw - E00))
        numerator = raw_s11 - e00
        denominator = e10e01 + e11 * numerator
        
        if abs(denominator) < 1e-12:
            _warning_counts['denominator_near_zero'] += 1
            return raw_s11
        
        return numerator / denominator
    
    @staticmethod
    def s11_correction_2port(raw_s11: complex, d: complex, r: complex, 
                           s: complex) -> complex:
        """
        Apply 2-port calibration correction to S11 measurement
        
        Args:
            raw_s11: Raw S11 measurement
            d: Directivity error term
            r: Reflection tracking error term
            s: Source match error term
            
        Returns:
            Corrected S11 value
        """
        # 2-port S11 correction: S11_corrected = (S11_raw - D) / (R + S * (S11_raw - D))
        numerator = raw_s11 - d
        denominator = r + s * numerator
        
        if abs(denominator) < 1e-12:
            _warning_counts['denominator_near_zero'] += 1
            return raw_s11
        
        return numerator / denominator
    
    @staticmethod
    def s21_correction_2port(raw_s21: complex, t: complex, l: complex, 
                           s: complex, raw_s11: complex, raw_s22: complex) -> complex:
        """
        Apply 2-port calibration correction to S21 measurement
        
        Args:
            raw_s21: Raw S21 measurement
            t: Transmission tracking error term
            l: Load match error term
            s: Source match error term
            raw_s11: Raw S11 measurement
            raw_s22: Raw S22 measurement
            
        Returns:
            Corrected S21 value
        """
        # 2-port S21 correction: S21_corrected = S21_raw / (T * (1 + L * S22_raw) * (1 + S * S11_raw))
        denominator = t * (1 + l * raw_s22) * (1 + s * raw_s11)
        
        if abs(denominator) < 1e-12:
            logger.warning("Denominator near zero in S21 correction")
            return raw_s21
        
        return raw_s21 / denominator
    
    @staticmethod
    def validate_error_terms(error_terms: dict) -> dict:
        """
        Validate error terms for reasonableness
        
        Args:
            error_terms: Dictionary of error terms
            
        Returns:
            Dictionary with validation results
        """
        validation = {
            'is_valid': True,
            'warnings': [],
            'errors': []
        }
        
        # Check for reasonable magnitudes
        for term_name, value in error_terms.items():
            if isinstance(value, complex):
                magnitude = abs(value)
                
                # Check for extremely large values
                if magnitude > 10:
                    validation['warnings'].append(f"Error term {term_name} has large magnitude: {magnitude}")
                
                # Check for NaN or infinite values
                if np.isnan(magnitude) or np.isinf(magnitude):
                    validation['errors'].append(f"Error term {term_name} is NaN or infinite")
                    validation['is_valid'] = False
        
        return validation
    
    @staticmethod
    def calculate_uncertainty(error_terms: dict, measurement: complex) -> float:
        """
        Calculate measurement uncertainty based on error terms
        
        Args:
            error_terms: Dictionary of error terms
            measurement: Measurement value
            
        Returns:
            Estimated uncertainty in dB
        """
        # Simple uncertainty estimation based on error term magnitudes
        uncertainty = 0.0
        
        for term_name, value in error_terms.items():
            if isinstance(value, complex):
                magnitude = abs(value)
                # Convert to dB and add to uncertainty
                uncertainty += 20 * np.log10(1 + magnitude)
        
        return uncertainty
    
    @staticmethod
    def frequency_response_smoothing(frequencies: np.ndarray, values: np.ndarray, 
                                   window_size: int = 5) -> np.ndarray:
        """
        Apply smoothing to frequency response data
        
        Args:
            frequencies: Array of frequencies
            values: Array of complex values
            window_size: Size of smoothing window
            
        Returns:
            Smoothed values
        """
        if len(values) < window_size:
            return values
        
        # Apply moving average smoothing
        smoothed = np.zeros_like(values, dtype=complex)
        
        for i in range(len(values)):
            start_idx = max(0, i - window_size // 2)
            end_idx = min(len(values), i + window_size // 2 + 1)
            
            smoothed[i] = np.mean(values[start_idx:end_idx])
        
        return smoothed
    
    @staticmethod
    def group_delay_calculation(frequencies: np.ndarray, phase: np.ndarray) -> np.ndarray:
        """
        Calculate group delay from phase data
        
        Args:
            frequencies: Array of frequencies
            phase: Array of phase values in radians
            
        Returns:
            Group delay in seconds
        """
        if len(frequencies) < 2:
            return np.array([])
        
        # Calculate phase derivative
        phase_diff = np.diff(phase)
        freq_diff = np.diff(frequencies)
        
        # Handle phase unwrapping
        phase_diff = np.unwrap(phase_diff)
        
        # Calculate group delay: -d(phase)/d(frequency) / (2*pi)
        group_delay = -phase_diff / (2 * np.pi * freq_diff)
        
        # Pad to match original length
        group_delay = np.concatenate([[group_delay[0]], group_delay])
        
        return group_delay
    
    @staticmethod
    def magnitude_phase_conversion(values: np.ndarray) -> Tuple[np.ndarray, np.ndarray]:
        """
        Convert complex values to magnitude and phase
        
        Args:
            values: Array of complex values
            
        Returns:
            Tuple of (magnitude, phase) arrays
        """
        magnitude = np.abs(values)
        phase = np.angle(values)
        
        return magnitude, phase
    
    @staticmethod
    def db_conversion(values: np.ndarray, reference: float = 1.0) -> np.ndarray:
        """
        Convert linear values to dB
        
        Args:
            values: Array of linear values
            reference: Reference value for dB calculation
            
        Returns:
            Array of dB values
        """
        # Avoid log of zero
        values = np.where(values == 0, 1e-12, values)
        return 20 * np.log10(np.abs(values) / reference)
    
    @staticmethod
    def linear_from_db(db_values: np.ndarray, reference: float = 1.0) -> np.ndarray:
        """
        Convert dB values to linear
        
        Args:
            db_values: Array of dB values
            reference: Reference value for linear calculation
            
        Returns:
            Array of linear values
        """
        return reference * 10**(db_values / 20)
