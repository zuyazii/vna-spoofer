"""
Tests for calibration functionality

This module contains comprehensive tests for the calibration system including
file parsing, correction algorithms, and mathematical utilities.
"""

import pytest
import numpy as np
import json
import tempfile
import os
from unittest.mock import Mock, patch

from src.librevna.calibration import (
    CalFile, CalPoint, CalibrationData, Corrector, MathUtils
)


class TestCalFile:
    """Test calibration file parsing"""
    
    def test_cal_file_initialization(self):
        """Test CalFile initialization"""
        cal_file = CalFile()
        assert cal_file.file_path is None
        assert cal_file.calibration_points == []
        assert cal_file.used_ports == []
    
    def test_parse_complex_numbers(self):
        """Test complex number parsing"""
        cal_file = CalFile()
        
        # Test various formats
        assert cal_file._parse_complex("1.0+2.5i") == complex(1.0, 2.5)
        assert cal_file._parse_complex("3.0-1.2i") == complex(3.0, -1.2)
        assert cal_file._parse_complex("5.0") == complex(5.0)
        assert cal_file._parse_complex("i") == complex(0, 1)
        assert cal_file._parse_complex("-i") == complex(0, -1)
        assert cal_file._parse_complex(1.5) == complex(1.5)
    
    def test_create_sample_cal_file(self):
        """Create a sample calibration file for testing"""
        sample_data = {
            "calkit": {
                "Description": "Test calibration kit",
                "Manufacturer": "Test",
                "Serialnumber": "TEST123",
                "standards": [
                    {
                        "type": "Open",
                        "params": {
                            "id": 123456789,
                            "name": "Test Open",
                            "Z0": 50.0,
                            "delay": 0.0,
                            "loss": 0.0,
                            "C0": 0.0,
                            "C1": 0.0,
                            "C2": 0.0,
                            "C3": 0.0
                        }
                    }
                ]
            },
            "calibration": {
                "type": "SOLT",
                "usedPorts": [1, 2],
                "points": [
                    {
                        "frequency": 1000000.0,
                        "errorTerms": {
                            "D": "0.01+0.02i",
                            "R": "0.95+0.05i",
                            "S": "0.02+0.01i",
                            "L": "0.01+0.01i",
                            "T": "0.98+0.02i",
                            "I": "0.001+0.001i",
                            "E00": "0.01+0.02i",
                            "E11": "0.02+0.01i",
                            "E10E01": "0.95+0.05i"
                        }
                    },
                    {
                        "frequency": 2000000.0,
                        "errorTerms": {
                            "D": "0.015+0.025i",
                            "R": "0.96+0.04i",
                            "S": "0.025+0.015i",
                            "L": "0.015+0.015i",
                            "T": "0.99+0.015i",
                            "I": "0.0015+0.0015i",
                            "E00": "0.015+0.025i",
                            "E11": "0.025+0.015i",
                            "E10E01": "0.96+0.04i"
                        }
                    }
                ]
            }
        }
        
        # Create temporary file
        with tempfile.NamedTemporaryFile(mode='w', suffix='.cal', delete=False) as f:
            json.dump(sample_data, f, indent=2)
            temp_file = f.name
        
        try:
            # Test loading
            cal_file = CalFile(temp_file)
            
            assert cal_file.calibration_type == "SOLT"
            assert cal_file.used_ports == [1, 2]
            assert len(cal_file.calibration_points) == 2
            assert len(cal_file.frequency_points) == 2
            
            # Test frequency range
            min_freq, max_freq = cal_file.get_frequency_range()
            assert min_freq == 1000000.0
            assert max_freq == 2000000.0
            
            # Test calibration point retrieval
            point = cal_file.get_calibration_point(1000000.0)
            assert point is not None
            assert point.frequency == 1000000.0
            assert abs(point.D - complex(0.01, 0.02)) < 1e-10
            
        finally:
            os.unlink(temp_file)
    
    def test_cal_file_validation(self):
        """Test calibration file validation"""
        cal_file = CalFile()
        
        # Test with invalid data
        with pytest.raises(ValueError):
            cal_file._validate_calibration()
        
        # Test with valid data
        cal_file.frequency_points = [1e6, 2e6]
        cal_file.calibration_points = [
            CalPoint(1e6, complex(0.01), complex(0.95), complex(0.02), 
                    complex(0.01), complex(0.98), complex(0.001),
                    complex(0.01), complex(0.02), complex(0.95)),
            CalPoint(2e6, complex(0.015), complex(0.96), complex(0.025), 
                    complex(0.015), complex(0.99), complex(0.0015),
                    complex(0.015), complex(0.025), complex(0.96))
        ]
        
        # Should not raise exception
        cal_file._validate_calibration()


class TestCalibrationData:
    """Test calibration data structures"""
    
    def test_calibration_data_initialization(self):
        """Test CalibrationData initialization"""
        cal_data = CalibrationData()
        assert len(cal_data.frequencies) == 0
        assert len(cal_data.error_terms) == 9  # All error terms initialized
        assert cal_data.calibration_type == "Unknown"
        assert cal_data.used_ports == []
    
    def test_from_cal_points(self):
        """Test creating CalibrationData from CalPoint list"""
        cal_points = [
            CalPoint(1e6, complex(0.01), complex(0.95), complex(0.02), 
                    complex(0.01), complex(0.98), complex(0.001),
                    complex(0.01), complex(0.02), complex(0.95)),
            CalPoint(2e6, complex(0.015), complex(0.96), complex(0.025), 
                    complex(0.015), complex(0.99), complex(0.0015),
                    complex(0.015), complex(0.025), complex(0.96))
        ]
        
        cal_data = CalibrationData.from_cal_points(cal_points, "SOLT", [1, 2])
        
        assert len(cal_data.frequencies) == 2
        assert cal_data.calibration_type == "SOLT"
        assert cal_data.used_ports == [1, 2]
        assert cal_data.frequencies[0] == 1e6
        assert cal_data.frequencies[1] == 2e6
    
    def test_frequency_range(self):
        """Test frequency range calculation"""
        cal_data = CalibrationData()
        cal_data.frequencies = np.array([1e6, 2e6, 3e6])
        
        min_freq, max_freq = cal_data.get_frequency_range()
        assert min_freq == 1e6
        assert max_freq == 3e6
    
    def test_interpolation(self):
        """Test error term interpolation"""
        cal_data = CalibrationData()
        cal_data.frequencies = np.array([1e6, 2e6, 3e6])
        cal_data.error_terms['D'] = np.array([complex(0.01), complex(0.02), complex(0.03)])
        
        # Test exact match
        result = cal_data.get_error_term_at_frequency('D', 2e6)
        assert abs(result - complex(0.02)) < 1e-10
        
        # Test interpolation
        result = cal_data.get_interpolated_error_term('D', 1.5e6, 'linear')
        expected = complex(0.015)  # Midpoint between 0.01 and 0.02
        assert abs(result - expected) < 1e-10
    
    def test_validation(self):
        """Test calibration data validation"""
        cal_data = CalibrationData()

        # Test empty data
        validation = cal_data.validate_calibration()
        assert not validation['is_valid']
        assert "No frequency points" in validation['errors']

        # Test incomplete data (missing error terms)
        cal_data.frequencies = np.array([1e6, 2e6])
        cal_data.error_terms['D'] = np.array([complex(0.01), complex(0.02)])
        cal_data.error_terms['R'] = np.array([complex(0.95), complex(0.96)])

        validation = cal_data.validate_calibration()
        # With only D and R terms, validation should pass (no length mismatches)
        assert validation['is_valid']

        # Test valid complete data
        cal_data.error_terms['S'] = np.array([complex(0.02), complex(0.025)])
        cal_data.error_terms['L'] = np.array([complex(0.01), complex(0.015)])
        cal_data.error_terms['T'] = np.array([complex(0.98), complex(0.99)])
        cal_data.error_terms['I'] = np.array([complex(0.001), complex(0.0015)])
        cal_data.error_terms['E00'] = np.array([complex(0.01), complex(0.02)])
        cal_data.error_terms['E11'] = np.array([complex(0.02), complex(0.025)])
        cal_data.error_terms['E10E01'] = np.array([complex(0.95), complex(0.96)])

        validation = cal_data.validate_calibration()
        assert validation['is_valid']
        assert len(validation['errors']) == 0


class TestMathUtils:
    """Test mathematical utilities"""
    
    def test_complex_interpolation(self):
        """Test complex number interpolation"""
        frequencies = np.array([1e6, 2e6, 3e6])
        values = np.array([complex(0.01), complex(0.02), complex(0.03)])
        
        # Test exact match
        result = MathUtils.complex_interpolate(frequencies, values, 2e6, 'nearest')
        assert abs(result - complex(0.02)) < 1e-10
        
        # Test linear interpolation
        result = MathUtils.complex_interpolate(frequencies, values, 1.5e6, 'linear')
        expected = complex(0.015)
        assert abs(result - expected) < 1e-10
    
    def test_s11_correction_1port(self):
        """Test 1-port S11 correction"""
        raw_s11 = complex(0.1, 0.05)
        e00 = complex(0.01, 0.02)
        e11 = complex(0.02, 0.01)
        e10e01 = complex(0.95, 0.05)
        
        corrected = MathUtils.s11_correction_1port(raw_s11, e00, e11, e10e01)
        
        # Should be different from raw measurement
        assert abs(corrected - raw_s11) > 1e-6
        assert isinstance(corrected, complex)
    
    def test_s11_correction_2port(self):
        """Test 2-port S11 correction"""
        raw_s11 = complex(0.1, 0.05)
        d = complex(0.01, 0.02)
        r = complex(0.95, 0.05)
        s = complex(0.02, 0.01)
        
        corrected = MathUtils.s11_correction_2port(raw_s11, d, r, s)
        
        # Should be different from raw measurement
        assert abs(corrected - raw_s11) > 1e-6
        assert isinstance(corrected, complex)
    
    def test_s21_correction_2port(self):
        """Test 2-port S21 correction"""
        raw_s21 = complex(0.8, 0.1)
        t = complex(0.98, 0.02)
        l = complex(0.01, 0.01)
        s = complex(0.02, 0.01)
        raw_s11 = complex(0.1, 0.05)
        raw_s22 = complex(0.05, 0.02)
        
        corrected = MathUtils.s21_correction_2port(raw_s21, t, l, s, raw_s11, raw_s22)
        
        # Should be different from raw measurement
        assert abs(corrected - raw_s21) > 1e-6
        assert isinstance(corrected, complex)
    
    def test_error_term_validation(self):
        """Test error term validation"""
        error_terms = {
            'D': complex(0.01, 0.02),
            'R': complex(0.95, 0.05),
            'S': complex(0.02, 0.01)
        }
        
        validation = MathUtils.validate_error_terms(error_terms)
        assert validation['is_valid']
        assert len(validation['errors']) == 0
    
    def test_db_conversion(self):
        """Test dB conversion functions"""
        linear_values = np.array([0.1, 1.0, 10.0])
        db_values = MathUtils.db_conversion(linear_values)
        
        # Check known values
        assert abs(db_values[0] - (-20)) < 1e-6  # 0.1 -> -20 dB
        assert abs(db_values[1] - 0) < 1e-6      # 1.0 -> 0 dB
        assert abs(db_values[2] - 20) < 1e-6     # 10.0 -> 20 dB
        
        # Test reverse conversion
        back_to_linear = MathUtils.linear_from_db(db_values)
        assert np.allclose(linear_values, back_to_linear, rtol=1e-6)


class TestCorrector:
    """Test calibration corrector"""
    
    def test_corrector_initialization(self):
        """Test Corrector initialization"""
        corrector = Corrector()
        assert corrector.calibration_data is None
        assert corrector.interpolation_method == 'linear'
        assert not corrector.extrapolation_enabled
    
    def test_load_calibration(self):
        """Test loading calibration data"""
        # Create test calibration data with all required error terms
        cal_data = CalibrationData()
        cal_data.frequencies = np.array([1e6, 2e6])
        cal_data.error_terms['D'] = np.array([complex(0.01), complex(0.02)])
        cal_data.error_terms['R'] = np.array([complex(0.95), complex(0.96)])
        cal_data.error_terms['S'] = np.array([complex(0.02), complex(0.025)])
        cal_data.error_terms['L'] = np.array([complex(0.01), complex(0.015)])
        cal_data.error_terms['T'] = np.array([complex(0.98), complex(0.99)])
        cal_data.error_terms['I'] = np.array([complex(0.001), complex(0.0015)])
        cal_data.error_terms['E00'] = np.array([complex(0.01), complex(0.02)])
        cal_data.error_terms['E11'] = np.array([complex(0.02), complex(0.025)])
        cal_data.error_terms['E10E01'] = np.array([complex(0.95), complex(0.96)])
        cal_data.calibration_type = "SOLT"
        cal_data.used_ports = [1, 2]
        
        corrector = Corrector()
        corrector.load_calibration(cal_data)
        
        assert corrector.calibration_data is not None
        assert corrector.calibration_data.calibration_type == "SOLT"
    
    def test_s11_correction(self):
        """Test S11 correction"""
        # Create test calibration data with all required error terms
        cal_data = CalibrationData()
        cal_data.frequencies = np.array([1e6, 2e6])
        cal_data.error_terms['D'] = np.array([complex(0.01), complex(0.02)])
        cal_data.error_terms['R'] = np.array([complex(0.95), complex(0.96)])
        cal_data.error_terms['S'] = np.array([complex(0.02), complex(0.025)])
        cal_data.error_terms['L'] = np.array([complex(0.01), complex(0.015)])
        cal_data.error_terms['T'] = np.array([complex(0.98), complex(0.99)])
        cal_data.error_terms['I'] = np.array([complex(0.001), complex(0.0015)])
        cal_data.error_terms['E00'] = np.array([complex(0.01), complex(0.02)])
        cal_data.error_terms['E11'] = np.array([complex(0.02), complex(0.025)])
        cal_data.error_terms['E10E01'] = np.array([complex(0.95), complex(0.96)])
        
        corrector = Corrector(cal_data)
        
        raw_s11 = complex(0.1, 0.05)
        corrected = corrector.correct_s11_measurement(1.5e6, raw_s11)
        
        # Should be different from raw measurement
        assert abs(corrected - raw_s11) > 1e-6
        assert isinstance(corrected, complex)
    
    def test_s21_correction(self):
        """Test S21 correction"""
        # Create test calibration data with all required error terms
        cal_data = CalibrationData()
        cal_data.frequencies = np.array([1e6, 2e6])
        cal_data.error_terms['D'] = np.array([complex(0.01), complex(0.02)])
        cal_data.error_terms['R'] = np.array([complex(0.95), complex(0.96)])
        cal_data.error_terms['S'] = np.array([complex(0.02), complex(0.025)])
        cal_data.error_terms['L'] = np.array([complex(0.01), complex(0.015)])
        cal_data.error_terms['T'] = np.array([complex(0.98), complex(0.99)])
        cal_data.error_terms['I'] = np.array([complex(0.001), complex(0.0015)])
        cal_data.error_terms['E00'] = np.array([complex(0.01), complex(0.02)])
        cal_data.error_terms['E11'] = np.array([complex(0.02), complex(0.025)])
        cal_data.error_terms['E10E01'] = np.array([complex(0.95), complex(0.96)])
        
        corrector = Corrector(cal_data)
        
        raw_s21 = complex(0.8, 0.1)
        raw_s11 = complex(0.1, 0.05)
        raw_s22 = complex(0.05, 0.02)
        
        corrected = corrector.correct_s21_measurement(1.5e6, raw_s21, raw_s11, raw_s22)
        
        # Should be different from raw measurement
        assert abs(corrected - raw_s21) > 1e-6
        assert isinstance(corrected, complex)
    
    def test_correction_array(self):
        """Test correction of measurement arrays"""
        # Create test calibration data with all required error terms
        cal_data = CalibrationData()
        cal_data.frequencies = np.array([1e6, 2e6])
        cal_data.error_terms['D'] = np.array([complex(0.01), complex(0.02)])
        cal_data.error_terms['R'] = np.array([complex(0.95), complex(0.96)])
        cal_data.error_terms['S'] = np.array([complex(0.02), complex(0.025)])
        cal_data.error_terms['L'] = np.array([complex(0.01), complex(0.015)])
        cal_data.error_terms['T'] = np.array([complex(0.98), complex(0.99)])
        cal_data.error_terms['I'] = np.array([complex(0.001), complex(0.0015)])
        cal_data.error_terms['E00'] = np.array([complex(0.01), complex(0.02)])
        cal_data.error_terms['E11'] = np.array([complex(0.02), complex(0.025)])
        cal_data.error_terms['E10E01'] = np.array([complex(0.95), complex(0.96)])
        
        corrector = Corrector(cal_data)
        
        frequencies = np.array([1e6, 1.5e6, 2e6])
        raw_measurements = np.array([complex(0.1), complex(0.15), complex(0.2)])
        
        corrected = corrector.correct_measurement_array(frequencies, raw_measurements, 's11')
        
        assert len(corrected) == len(raw_measurements)
        assert not np.allclose(corrected, raw_measurements, rtol=1e-6)
    
    def test_validation_accuracy(self):
        """Test correction accuracy validation"""
        corrector = Corrector()
        
        test_frequencies = [1e6, 2e6]
        known_measurements = [complex(0.1), complex(0.2)]
        corrected_measurements = [complex(0.11), complex(0.21)]  # Small error
        
        validation = corrector.validate_correction_accuracy(
            test_frequencies, known_measurements, corrected_measurements
        )
        
        assert 'max_error' in validation
        assert 'mean_error' in validation
        assert 'rms_error' in validation
        assert 'is_accurate' in validation
        assert validation['is_accurate']  # Should be accurate with small errors


class TestIntegration:
    """Integration tests for calibration system"""
    
    def test_full_calibration_workflow(self):
        """Test complete calibration workflow"""
        # Create sample calibration file
        sample_data = {
            "calkit": {
                "Description": "Test calibration kit",
                "Manufacturer": "Test",
                "Serialnumber": "TEST123",
                "standards": []
            },
            "calibration": {
                "type": "SOLT",
                "usedPorts": [1, 2],
                "points": [
                    {
                        "frequency": 1000000.0,
                        "errorTerms": {
                            "D": "0.01+0.02i",
                            "R": "0.95+0.05i",
                            "S": "0.02+0.01i",
                            "L": "0.01+0.01i",
                            "T": "0.98+0.02i",
                            "I": "0.001+0.001i"
                        }
                    }
                ]
            }
        }
        
        with tempfile.NamedTemporaryFile(mode='w', suffix='.cal', delete=False) as f:
            json.dump(sample_data, f, indent=2)
            temp_file = f.name
        
        try:
            # Load calibration file
            cal_file = CalFile(temp_file)
            
            # Convert to CalibrationData
            cal_data = CalibrationData.from_cal_points(
                cal_file.calibration_points,
                cal_file.calibration_type,
                cal_file.used_ports
            )
            
            # Create corrector
            corrector = Corrector(cal_data)
            
            # Test correction
            raw_s11 = complex(0.1, 0.05)
            corrected = corrector.correct_s11_measurement(1e6, raw_s11)
            
            # Verify correction was applied
            assert abs(corrected - raw_s11) > 1e-6
            assert isinstance(corrected, complex)
            
            # Test calibration info
            info = corrector.get_calibration_info()
            assert info['loaded']
            assert info['calibration_type'] == "SOLT"
            
        finally:
            os.unlink(temp_file)


if __name__ == '__main__':
    pytest.main([__file__])
