"""
Calibration Corrector

This module implements calibration correction algorithms to apply
calibration data to raw VNA measurements.
"""

import numpy as np
from typing import Dict, List, Optional, Tuple, Union, Any
import logging
from .structures import CalibrationData
from .cal_file import CalPoint
from .math_utils import MathUtils

logger = logging.getLogger(__name__)

# Global warning counters for suppression
_warning_counts = {
    'insufficient_error_terms': 0,
    'denominator_near_zero': 0
}

def get_warning_summary() -> str:
    """Get summary of suppressed warnings"""
    summary_parts = []
    if _warning_counts['insufficient_error_terms'] > 0:
        summary_parts.append(f"insufficient error terms: {_warning_counts['insufficient_error_terms']}")
    if _warning_counts['denominator_near_zero'] > 0:
        summary_parts.append(f"denominator near zero: {_warning_counts['denominator_near_zero']}")
    
    if summary_parts:
        return f"Calibration warnings suppressed: {', '.join(summary_parts)}"
    return ""

def reset_warning_counts():
    """Reset warning counters"""
    global _warning_counts
    _warning_counts = {
        'insufficient_error_terms': 0,
        'denominator_near_zero': 0
    }


class Corrector:
    """
    Calibration correction engine
    
    This class applies calibration corrections to raw VNA measurements
    using error terms from calibration files.
    """
    
    def __init__(self, calibration_data: Optional[CalibrationData] = None):
        """
        Initialize corrector with calibration data
        
        Args:
            calibration_data: CalibrationData instance with error terms
        """
        self.calibration_data = calibration_data
        self.interpolation_method = 'linear'
        self.extrapolation_enabled = False
        
    def load_calibration(self, calibration_data: CalibrationData) -> None:
        """
        Load calibration data
        
        Args:
            calibration_data: CalibrationData instance
        """
        self.calibration_data = calibration_data
        
        # Validate calibration data
        validation = calibration_data.validate_calibration()
        if not validation['is_valid']:
            raise ValueError(f"Invalid calibration data: {validation['errors']}")
        
        if validation['warnings']:
            for warning in validation['warnings']:
                logger.warning(f"Calibration warning: {warning}")
        
        logger.info(f"Loaded calibration data: {len(calibration_data.frequencies)} points")
        logger.info(f"Frequency range: {calibration_data.get_frequency_range()}")
    
    def correct_s11_measurement(self, frequency: float, raw_s11: complex, 
                               method: str = 'auto') -> complex:
        """
        Apply calibration correction to S11 measurement
        
        Args:
            frequency: Measurement frequency in Hz
            raw_s11: Raw S11 measurement
            method: Correction method ('auto', '1port', '2port')
            
        Returns:
            Corrected S11 value
        """
        if not self.calibration_data:
            logger.warning("No calibration data loaded, returning raw measurement")
            return raw_s11
        
        # Check if frequency is in calibration range
        if not self.calibration_data.is_frequency_in_range(frequency):
            if not self.extrapolation_enabled:
                logger.warning(f"Frequency {frequency} outside calibration range")
                return raw_s11
        
        # Get error terms for this frequency
        error_terms = self.calibration_data.get_all_error_terms_at_frequency(
            frequency, self.interpolation_method
        )
        
        # Determine correction method
        if method == 'auto':
            method = self._determine_correction_method(error_terms)
        
        # Apply correction
        if method == '1port':
            return self._correct_s11_1port(raw_s11, error_terms)
        elif method == '2port':
            return self._correct_s11_2port(raw_s11, error_terms)
        else:
            logger.warning(f"Unknown correction method: {method}")
            return raw_s11
    
    def correct_s21_measurement(self, frequency: float, raw_s21: complex,
                               raw_s11: complex, raw_s22: complex) -> complex:
        """
        Apply calibration correction to S21 measurement
        
        Args:
            frequency: Measurement frequency in Hz
            raw_s21: Raw S21 measurement
            raw_s11: Raw S11 measurement
            raw_s22: Raw S22 measurement
            
        Returns:
            Corrected S21 value
        """
        if not self.calibration_data:
            logger.warning("No calibration data loaded, returning raw measurement")
            return raw_s21
        
        # Check if frequency is in calibration range
        if not self.calibration_data.is_frequency_in_range(frequency):
            if not self.extrapolation_enabled:
                logger.warning(f"Frequency {frequency} outside calibration range")
                return raw_s21
        
        # Get error terms for this frequency
        error_terms = self.calibration_data.get_all_error_terms_at_frequency(
            frequency, self.interpolation_method
        )
        
        # Apply 2-port S21 correction
        return self._correct_s21_2port(raw_s21, raw_s11, raw_s22, error_terms)
    
    def correct_measurement_array(self, frequencies: np.ndarray, 
                                 raw_measurements: np.ndarray,
                                 measurement_type: str = 's11',
                                 **kwargs) -> np.ndarray:
        """
        Apply calibration correction to array of measurements
        
        Args:
            frequencies: Array of frequencies
            raw_measurements: Array of raw measurements
            measurement_type: Type of measurement ('s11', 's21')
            **kwargs: Additional arguments for specific measurement types
            
        Returns:
            Array of corrected measurements
        """
        if not self.calibration_data:
            logger.warning("No calibration data loaded, returning raw measurements")
            return raw_measurements
        
        corrected = np.zeros_like(raw_measurements, dtype=complex)
        
        for i, (freq, raw_meas) in enumerate(zip(frequencies, raw_measurements)):
            if measurement_type == 's11':
                corrected[i] = self.correct_s11_measurement(freq, raw_meas)
            elif measurement_type == 's21':
                raw_s11 = kwargs.get('raw_s11', np.zeros_like(raw_measurements))[i]
                raw_s22 = kwargs.get('raw_s22', np.zeros_like(raw_measurements))[i]
                corrected[i] = self.correct_s21_measurement(freq, raw_meas, raw_s11, raw_s22)
            else:
                logger.warning(f"Unknown measurement type: {measurement_type}")
                corrected[i] = raw_meas
        
        return corrected
    
    def _determine_correction_method(self, error_terms: Dict[str, complex]) -> str:
        """
        Determine best correction method based on available error terms
        
        Args:
            error_terms: Dictionary of error terms
            
        Returns:
            Correction method ('1port' or '2port')
        """
        # Check if 1-port terms are available and non-zero
        e00_available = error_terms.get('E00') is not None and abs(error_terms['E00']) > 1e-10
        e11_available = error_terms.get('E11') is not None and abs(error_terms['E11']) > 1e-10
        e10e01_available = error_terms.get('E10E01') is not None and abs(error_terms['E10E01']) > 1e-10
        
        # Check if 2-port terms are available and non-zero
        d_available = error_terms.get('D') is not None and abs(error_terms['D']) > 1e-10
        r_available = error_terms.get('R') is not None and abs(error_terms['R']) > 1e-10
        s_available = error_terms.get('S') is not None and abs(error_terms['S']) > 1e-10
        
        # Also check for L, T, I terms for 2-port
        l_available = error_terms.get('L') is not None and abs(error_terms['L']) > 1e-10
        t_available = error_terms.get('T') is not None and abs(error_terms['T']) > 1e-10
        i_available = error_terms.get('I') is not None and abs(error_terms['I']) > 1e-10
        
        # Log available terms for debugging
        available_terms = []
        if e00_available: available_terms.append('E00')
        if e11_available: available_terms.append('E11')
        if e10e01_available: available_terms.append('E10E01')
        if d_available: available_terms.append('D')
        if r_available: available_terms.append('R')
        if s_available: available_terms.append('S')
        if l_available: available_terms.append('L')
        if t_available: available_terms.append('T')
        if i_available: available_terms.append('I')
        
        logger.debug(f"Available error terms: {available_terms}")
        
        # Prefer 1-port if available and complete
        if e00_available and e11_available and e10e01_available:
            logger.debug("Using 1-port correction (E00, E11, E10E01 available)")
            return '1port'
        # Use 2-port if available and complete
        elif d_available and r_available and s_available and l_available and t_available:
            logger.debug("Using 2-port correction (D, R, S, L, T available)")
            return '2port'
        # Fallback to 1-port with available terms
        elif e00_available or e11_available or e10e01_available:
            logger.warning("Using 1-port correction with incomplete error terms")
            return '1port'
        # Last resort - use 2-port with available terms
        elif d_available or r_available or s_available:
            logger.warning("Using 2-port correction with incomplete error terms")
            return '2port'
        else:
            _warning_counts['insufficient_error_terms'] += 1
            logger.error("No valid error terms available - using fallback 1-port correction")
            return '1port'  # Default fallback
    
    def _correct_s11_1port(self, raw_s11: complex, error_terms: Dict[str, complex]) -> complex:
        """
        Apply 1-port calibration correction to S11
        
        Args:
            raw_s11: Raw S11 measurement
            error_terms: Dictionary of error terms
            
        Returns:
            Corrected S11 value
        """
        e00 = error_terms.get('E00', complex(0))
        e11 = error_terms.get('E11', complex(0))
        e10e01 = error_terms.get('E10E01', complex(0))
        
        return MathUtils.s11_correction_1port(raw_s11, e00, e11, e10e01)
    
    def _correct_s11_2port(self, raw_s11: complex, error_terms: Dict[str, complex]) -> complex:
        """
        Apply 2-port calibration correction to S11
        
        Args:
            raw_s11: Raw S11 measurement
            error_terms: Dictionary of error terms
            
        Returns:
            Corrected S11 value
        """
        d = error_terms.get('D', complex(0))
        r = error_terms.get('R', complex(0))
        s = error_terms.get('S', complex(0))
        
        return MathUtils.s11_correction_2port(raw_s11, d, r, s)
    
    def _correct_s21_2port(self, raw_s21: complex, raw_s11: complex, 
                          raw_s22: complex, error_terms: Dict[str, complex]) -> complex:
        """
        Apply 2-port calibration correction to S21
        
        Args:
            raw_s21: Raw S21 measurement
            raw_s11: Raw S11 measurement
            raw_s22: Raw S22 measurement
            error_terms: Dictionary of error terms
            
        Returns:
            Corrected S21 value
        """
        t = error_terms.get('T', complex(0))
        l = error_terms.get('L', complex(0))
        s = error_terms.get('S', complex(0))
        
        return MathUtils.s21_correction_2port(raw_s21, t, l, s, raw_s11, raw_s22)
    
    def _correct_measurement_matrix(self, raw_measurements: Dict[str, complex], 
                                  error_terms: Dict[str, complex]) -> Dict[str, complex]:
        """
        Apply matrix-based calibration correction matching LibreVNA GUI
        
        This implements the correction algorithm from calibration.cpp:correctMeasurement()
        lines 322-396
        
        Args:
            raw_measurements: Dictionary of raw measurements (S11, S21, S12, S22)
            error_terms: Dictionary of error terms
            
        Returns:
            Dictionary of corrected measurements
        """
        corrected = {}
        
        try:
            # Extract error terms
            d = error_terms.get('D', complex(0))
            r = error_terms.get('R', complex(0))
            s = error_terms.get('S', complex(0))
            l = error_terms.get('L', complex(0))
            t = error_terms.get('T', complex(0))
            i = error_terms.get('I', complex(0))
            
            # Get raw measurements
            s11_raw = raw_measurements.get('S11', complex(0))
            s21_raw = raw_measurements.get('S21', complex(0))
            s12_raw = raw_measurements.get('S12', complex(0))
            s22_raw = raw_measurements.get('S22', complex(0))
            
            # Create S-parameter matrix
            s_matrix = np.array([
                [s11_raw, s12_raw],
                [s21_raw, s22_raw]
            ], dtype=complex)
            
            # Create error term matrices
            # a matrix (incident waves) - line 371-383
            a_matrix = np.zeros((2, 2), dtype=complex)
            b_matrix = np.zeros((2, 2), dtype=complex)
            
            # Port 1 (exciting port)
            a_matrix[0, 0] = 1.0 + s/r * (s11_raw - d * 1.0)
            b_matrix[0, 0] = (1.0 / r) * (s11_raw - d * 1.0)
            
            # Port 2 (receiving port)
            a_matrix[1, 0] = l * s21_raw / t
            b_matrix[1, 0] = s21_raw / t
            
            # Port 2 (exciting port)
            a_matrix[1, 1] = 1.0 + s/r * (s22_raw - d * 1.0)
            b_matrix[1, 1] = (1.0 / r) * (s22_raw - d * 1.0)
            
            # Port 1 (receiving port)
            a_matrix[0, 1] = l * s12_raw / t
            b_matrix[0, 1] = s12_raw / t
            
            # Apply isolation removal (line 362-368)
            s_matrix[0, 1] -= i  # S12 - isolation
            s_matrix[1, 0] -= i  # S21 - isolation
            
            # Compute corrected S-parameters (line 385)
            try:
                s_corrected = b_matrix @ np.linalg.inv(a_matrix)
                
                # Extract corrected values
                corrected['S11'] = s_corrected[0, 0]
                corrected['S12'] = s_corrected[0, 1]
                corrected['S21'] = s_corrected[1, 0]
                corrected['S22'] = s_corrected[1, 1]
                
                logger.debug(f"Matrix correction applied: S11={corrected['S11']:.6f}, S21={corrected['S21']:.6f}")
                
            except np.linalg.LinAlgError:
                logger.warning("Matrix inversion failed in correction, using raw measurements")
                corrected = raw_measurements.copy()
                
        except Exception as e:
            logger.error(f"Matrix correction failed: {e}")
            corrected = raw_measurements.copy()
        
        return corrected
    
    def validate_correction_accuracy(self, test_frequencies: List[float],
                                   known_measurements: List[complex],
                                   corrected_measurements: List[complex]) -> Dict[str, Any]:
        """
        Validate correction accuracy against known measurements
        
        Args:
            test_frequencies: List of test frequencies
            known_measurements: List of known correct measurements
            corrected_measurements: List of corrected measurements
            
        Returns:
            Dictionary with validation results
        """
        if len(known_measurements) != len(corrected_measurements):
            raise ValueError("Measurement arrays must have same length")
        
        # Calculate errors
        errors = []
        for known, corrected in zip(known_measurements, corrected_measurements):
            error = abs(corrected - known)
            errors.append(error)
        
        errors = np.array(errors)
        
        # Calculate statistics
        max_error = np.max(errors)
        mean_error = np.mean(errors)
        rms_error = np.sqrt(np.mean(errors**2))
        
        # Convert to dB
        max_error_db = 20 * np.log10(1 + max_error)
        mean_error_db = 20 * np.log10(1 + mean_error)
        rms_error_db = 20 * np.log10(1 + rms_error)
        
        validation = {
            'max_error': max_error,
            'mean_error': mean_error,
            'rms_error': rms_error,
            'max_error_db': max_error_db,
            'mean_error_db': mean_error_db,
            'rms_error_db': rms_error_db,
            'is_accurate': max_error_db < 0.2,  # ±0.2 dB accuracy target
            'test_frequencies': test_frequencies,
            'error_array': errors.tolist()
        }
        
        return validation
    
    def get_calibration_info(self) -> Dict[str, Any]:
        """
        Get information about loaded calibration
        
        Returns:
            Dictionary with calibration information
        """
        if not self.calibration_data:
            return {'loaded': False}
        
        info = self.calibration_data.get_calibration_info()
        info['loaded'] = True
        info['interpolation_method'] = self.interpolation_method
        info['extrapolation_enabled'] = self.extrapolation_enabled
        
        return info
    
    def set_interpolation_method(self, method: str) -> None:
        """
        Set interpolation method for error term interpolation
        
        Args:
            method: Interpolation method ('linear', 'cubic', 'nearest')
        """
        if method not in ['linear', 'cubic', 'nearest']:
            raise ValueError(f"Unknown interpolation method: {method}")
        
        self.interpolation_method = method
        logger.info(f"Interpolation method set to: {method}")
    
    def enable_extrapolation(self, enabled: bool = True) -> None:
        """
        Enable or disable extrapolation outside calibration frequency range
        
        Args:
            enabled: Whether to enable extrapolation
        """
        self.extrapolation_enabled = enabled
        logger.info(f"Extrapolation {'enabled' if enabled else 'disabled'}")
    
    def create_correction_report(self, test_measurements: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
        """
        Create a comprehensive correction report
        
        Args:
            test_measurements: Optional test measurements for validation
            
        Returns:
            Dictionary with correction report
        """
        report = {
            'calibration_info': self.get_calibration_info(),
            'correction_settings': {
                'interpolation_method': self.interpolation_method,
                'extrapolation_enabled': self.extrapolation_enabled
            }
        }
        
        if test_measurements:
            # Add validation results if test measurements provided
            validation = self.validate_correction_accuracy(
                test_measurements.get('frequencies', []),
                test_measurements.get('known_measurements', []),
                test_measurements.get('corrected_measurements', [])
            )
            report['validation'] = validation
        
        return report
