"""
Calibration File Parser

This module implements parsing of LibreVNA .cal JSON files containing
calibration data and error terms for VNA measurements.
"""

import json
import logging
from typing import Dict, List, Optional, Tuple, Any
from dataclasses import dataclass
import numpy as np

logger = logging.getLogger(__name__)


@dataclass
class CalStandard:
    """Represents a calibration standard (Open, Short, Load, Through)"""
    type: str  # 'Open', 'Short', 'Load', 'Through'
    params: Dict[str, Any]
    id: int


@dataclass
class CalPoint:
    """Represents a single calibration point with error terms"""
    frequency: float
    # Error terms for 2-port calibration
    D: complex  # Directivity
    R: complex  # Reflection tracking
    S: complex  # Source match
    L: complex  # Load match
    T: complex  # Transmission tracking
    I: complex  # Isolation
    # Additional terms for 1-port calibration
    E00: complex  # Directivity
    E11: complex  # Source match
    E10E01: complex  # Reflection tracking


class CalFile:
    """
    Parser for LibreVNA .cal JSON files
    
    This class handles parsing of calibration files containing:
    - Calibration kit definitions
    - Error terms for each frequency point
    - Port configuration
    - Calibration type information
    """
    
    def __init__(self, file_path: Optional[str] = None):
        """
        Initialize CalFile parser
        
        Args:
            file_path: Path to .cal file to load
        """
        self.file_path = file_path
        self.calkit = None
        self.calibration_points = []
        self.used_ports = []
        self.calibration_type = None
        self.frequency_points = []
        self.raw_data = None
        
        if file_path:
            self.load(file_path)
    
    def load(self, file_path: str) -> None:
        """
        Load calibration data from .cal file
        
        Args:
            file_path: Path to .cal file
            
        Raises:
            FileNotFoundError: If file doesn't exist
            json.JSONDecodeError: If file is not valid JSON
            ValueError: If file structure is invalid
        """
        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                self.raw_data = json.load(f)
            
            self.file_path = file_path
            self._parse_calkit()
            self._parse_calibration_data()
            self._validate_calibration()
            
            logger.info(f"Loaded calibration file: {file_path}")
            logger.info(f"Calibration type: {self.calibration_type}")
            logger.info(f"Frequency points: {len(self.frequency_points)}")
            logger.info(f"Used ports: {self.used_ports}")
            
        except FileNotFoundError:
            raise FileNotFoundError(f"Calibration file not found: {file_path}")
        except json.JSONDecodeError as e:
            raise json.JSONDecodeError(f"Invalid JSON in calibration file: {e}")
        except Exception as e:
            raise ValueError(f"Error parsing calibration file: {e}")
    
    def _parse_calkit(self) -> None:
        """Parse calibration kit information"""
        if 'calkit' not in self.raw_data:
            raise ValueError("Missing 'calkit' section in calibration file")
        
        calkit_data = self.raw_data['calkit']
        self.calkit = {
            'description': calkit_data.get('Description', ''),
            'manufacturer': calkit_data.get('Manufacturer', ''),
            'serial_number': calkit_data.get('Serialnumber', ''),
            'standards': []
        }
        
        # Parse standards
        for std_data in calkit_data.get('standards', []):
            standard = CalStandard(
                type=std_data['type'],
                params=std_data['params'],
                id=std_data['params'].get('id', 0)
            )
            self.calkit['standards'].append(standard)
    
    def _parse_calibration_data(self) -> None:
        """Parse calibration points and error terms"""
        # Check if this is LibreVNA format version 3 with measurements
        if 'measurements' in self.raw_data and 'format' in self.raw_data:
            self._parse_librevna_format3()
            return
        
        # Legacy format with pre-computed error terms
        if 'calibration' not in self.raw_data:
            raise ValueError("Missing 'calibration' section in calibration file")
        
        cal_data = self.raw_data['calibration']
        
        # Parse used ports
        self.used_ports = cal_data.get('usedPorts', [])
        if not self.used_ports:
            raise ValueError("No used ports specified in calibration")
        
        # Parse calibration type
        self.calibration_type = cal_data.get('type', 'Unknown')
        
        # Parse frequency points and error terms
        self.calibration_points = []
        self.frequency_points = []
        
        for point_data in cal_data.get('points', []):
            freq = point_data['frequency']
            self.frequency_points.append(freq)
            
            # Parse error terms
            error_terms = point_data.get('errorTerms', {})
            
            # 2-port error terms
            D = self._parse_complex(error_terms.get('D', '0+0i'))
            R = self._parse_complex(error_terms.get('R', '0+0i'))
            S = self._parse_complex(error_terms.get('S', '0+0i'))
            L = self._parse_complex(error_terms.get('L', '0+0i'))
            T = self._parse_complex(error_terms.get('T', '0+0i'))
            I = self._parse_complex(error_terms.get('I', '0+0i'))
            
            # 1-port error terms (alternative representation)
            E00 = self._parse_complex(error_terms.get('E00', '0+0i'))
            E11 = self._parse_complex(error_terms.get('E11', '0+0i'))
            E10E01 = self._parse_complex(error_terms.get('E10E01', '0+0i'))
            
            cal_point = CalPoint(
                frequency=freq,
                D=D, R=R, S=S, L=L, T=T, I=I,
                E00=E00, E11=E11, E10E01=E10E01
            )
            
            self.calibration_points.append(cal_point)
        
        # Sort by frequency
        sorted_indices = np.argsort(self.frequency_points)
        self.frequency_points = [self.frequency_points[i] for i in sorted_indices]
        self.calibration_points = [self.calibration_points[i] for i in sorted_indices]
    
    def _parse_librevna_format3(self) -> None:
        """Parse LibreVNA format version 3 calibration file"""
        logger.info("Parsing LibreVNA format version 3 calibration file")
        
        # Parse calibration type and ports
        self.calibration_type = self.raw_data.get('type', 'Unknown')
        ports = self.raw_data.get('ports')
        
        if ports is None or ports == []:
            self.used_ports = [1]  # Default to port 1
        else:
            self.used_ports = ports
        
        logger.info(f"Calibration type: {self.calibration_type}")
        logger.info(f"Used ports: {self.used_ports}")
        
        # Check if this is a valid calibration
        if self.calibration_type == 'None' or self.calibration_type is None:
            logger.warning("Calibration type is 'None' - this may not be a valid calibration")
        
        # Parse measurements
        measurements = self.raw_data.get('measurements', [])
        logger.info(f"Found {len(measurements)} measurements")
        
        # Group measurements by type and port
        measurement_data = {}
        for measurement in measurements:
            try:
                mtype = measurement.get('type', '')
                data = measurement.get('data', {})
                points = data.get('points', [])
                
                # Handle different measurement types
                if mtype in ['Open', 'Short', 'Load', 'SlidingLoad', 'Reflect']:
                    # One-port measurements
                    port = data.get('port', 1)
                    key = f"{mtype}_port{port}"
                elif mtype in ['Through', 'Isolation', 'Line']:
                    # Two-port measurements
                    port1 = data.get('port1', 1)
                    port2 = data.get('port2', 2)
                    key = f"{mtype}_port{port1}_port{port2}"
                else:
                    key = mtype
                
                if key not in measurement_data:
                    measurement_data[key] = []
                
                if points is not None:
                    for point in points:
                        freq = point.get('frequency', 0.0)
                        
                        # Handle different point formats
                        if 'real' in point and 'imag' in point:
                            # One-port format: real/imag
                            real = point.get('real', 0.0)
                            imag = point.get('imag', 0.0)
                            value = complex(real, imag)
                        elif 'Sparam' in point:
                            # Two-port format: S-parameter matrix
                            s_param = point.get('Sparam', {})
                            # For now, extract S11 as the main value
                            # In a full implementation, we'd handle the full S-parameter matrix
                            s11_real = s_param.get('S11', {}).get('real', 0.0)
                            s11_imag = s_param.get('S11', {}).get('imag', 0.0)
                            value = complex(s11_real, s11_imag)
                        else:
                            continue
                        
                        measurement_data[key].append({
                            'frequency': freq,
                            'value': value,
                            'port': data.get('port', 1),
                            'port1': data.get('port1', 1),
                            'port2': data.get('port2', 2)
                        })
            except Exception as e:
                logger.warning(f"Error parsing measurement: {e}")
                continue
        
        # Get frequency points from any measurement
        if measurement_data:
            # Collect all unique frequencies from all measurements
            freq_set = set()
            for mtype, points in measurement_data.items():
                for point in points:
                    freq_set.add(point['frequency'])
            self.frequency_points = list(freq_set)
        else:
            self.frequency_points = []
        
        # Sort frequency points
        self.frequency_points.sort()
        
        logger.info(f"Frequency points: {len(self.frequency_points)}")
        logger.info(f"Measurement types found: {list(measurement_data.keys())}")
        
        # Compute actual error terms from measurements
        self.calibration_points = []
        for freq in self.frequency_points:
            cal_point = self._compute_error_terms(freq, measurement_data)
            self.calibration_points.append(cal_point)
        
        # Validate error terms
        has_valid_terms = any(
            abs(point.D) > 1e-10 or abs(point.R) > 1e-10 or 
            abs(point.S) > 1e-10 or abs(point.L) > 1e-10 or
            abs(point.T) > 1e-10 or abs(point.I) > 1e-10 or
            abs(point.E00) > 1e-10 or abs(point.E11) > 1e-10 or abs(point.E10E01) > 1e-10
            for point in self.calibration_points
        )
        
        if not has_valid_terms:
            logger.warning("All error terms appear to be zero - calibration may be invalid")
        else:
            logger.info("Successfully computed error terms from measurements")
    
    def _compute_error_terms(self, frequency: float, measurement_data: Dict[str, List[Dict]]) -> CalPoint:
        """
        Compute error terms for a specific frequency using SOLT algorithm
        
        Args:
            frequency: Target frequency in Hz
            measurement_data: Dictionary of measurement data grouped by type
            
        Returns:
            CalPoint with computed error terms
        """
        cal_point = CalPoint(
            frequency=frequency,
            D=0+0j, R=0+0j, S=0+0j, L=0+0j, T=0+0j, I=0+0j,
            E00=0+0j, E11=0+0j, E10E01=0+0j
        )
        
        try:
            # Get measurements for this frequency
            measurements = self._get_measurements_at_frequency(frequency, measurement_data)
            
            if not measurements:
                logger.warning(f"No measurements found for frequency {frequency/1e6:.1f} MHz")
                return cal_point
            
            # Compute SOLT error terms for each port
            for port in self.used_ports:
                port_measurements = self._get_port_measurements(port, measurements)
                
                if self._has_solt_measurements(port_measurements):
                    # Compute 1-port SOLT error terms
                    e00, e11, e10e01 = self._compute_solt_1port(port_measurements, frequency)
                    
                    # Store in appropriate error terms
                    if port == 1:
                        cal_point.E00 = e00
                        cal_point.E11 = e11
                        cal_point.E10E01 = e10e01
                        # Also store in 2-port format for compatibility
                        cal_point.D = e00
                        cal_point.S = e11
                        cal_point.R = e10e01
                
                # Compute 2-port error terms if through measurements are available
                if len(self.used_ports) > 1:
                    for other_port in self.used_ports:
                        if other_port != port:
                            through_measurements = self._get_through_measurements(port, other_port, measurements)
                            if through_measurements:
                                l, t, i = self._compute_through_error_terms(
                                    port, other_port, through_measurements, 
                                    cal_point.D, cal_point.S, cal_point.R, frequency
                                )
                                cal_point.L = l
                                cal_point.T = t
                                cal_point.I = i
                                break  # Only need one through measurement
            
        except Exception as e:
            logger.error(f"Error computing error terms for frequency {frequency/1e6:.1f} MHz: {e}")
        
        return cal_point
    
    def _get_measurements_at_frequency(self, frequency: float, measurement_data: Dict[str, List[Dict]]) -> Dict[str, complex]:
        """Get measurements at specific frequency with interpolation"""
        measurements = {}
        
        for mtype, points in measurement_data.items():
            if not points:
                continue
                
            # Find closest frequency points
            frequencies = [p['frequency'] for p in points]
            if not frequencies:
                continue
                
            # Simple linear interpolation
            if frequency <= min(frequencies):
                measurements[mtype] = points[0]['value']
            elif frequency >= max(frequencies):
                measurements[mtype] = points[-1]['value']
            else:
                # Find surrounding points
                for i in range(len(points) - 1):
                    if points[i]['frequency'] <= frequency <= points[i + 1]['frequency']:
                        # Linear interpolation
                        f1, f2 = points[i]['frequency'], points[i + 1]['frequency']
                        v1, v2 = points[i]['value'], points[i + 1]['value']
                        alpha = (frequency - f1) / (f2 - f1)
                        measurements[mtype] = v1 + alpha * (v2 - v1)
                        break
        
        return measurements
    
    def _get_port_measurements(self, port: int, measurements: Dict[str, complex]) -> Dict[str, complex]:
        """Get measurements for a specific port"""
        port_measurements = {}
        
        for mtype, value in measurements.items():
            if f"_port{port}" in mtype:
                base_type = mtype.split("_port")[0]
                port_measurements[base_type] = value
        
        return port_measurements
    
    def _get_through_measurements(self, port1: int, port2: int, measurements: Dict[str, complex]) -> Optional[complex]:
        """Get through measurements between two ports"""
        key = f"Through_port{port1}_port{port2}"
        return measurements.get(key)
    
    def _has_solt_measurements(self, port_measurements: Dict[str, complex]) -> bool:
        """Check if we have the required SOLT measurements"""
        required = ['Open', 'Short', 'Load']
        return all(meas in port_measurements for meas in required)
    
    def _compute_solt_1port(self, measurements: Dict[str, complex], frequency: float) -> Tuple[complex, complex, complex]:
        """
        Compute 1-port SOLT error terms using the exact algorithm from LibreVNA GUI
        
        This implements the SOLT algorithm from calibration.cpp:computeSOLT()
        
        Args:
            measurements: Dictionary with 'Open', 'Short', 'Load' measurements
            frequency: Frequency in Hz
            
        Returns:
            Tuple of (E00, E11, E10E01) error terms
        """
        # Get measured values
        s_open = measurements.get('Open', 0+0j)
        s_short = measurements.get('Short', 0+0j)
        s_load = measurements.get('Load', 0+0j)
        
        # Ideal values (assuming 50 ohm system)
        # In a full implementation, these would come from calibration kit definitions
        s_open_ideal = 1+0j  # Open circuit
        s_short_ideal = -1+0j  # Short circuit
        s_load_ideal = 0+0j  # Load (50 ohm)
        
        # LibreVNA GUI SOLT algorithm implementation
        # From calibration.cpp lines 759-763
        
        # Calculate denominator
        denom = (s_load_ideal * s_open_ideal * (s_open - s_load) + 
                s_load_ideal * s_short_ideal * (s_load - s_short) + 
                s_open_ideal * s_short_ideal * (s_short - s_open))
        
        if abs(denom) < 1e-12:
            logger.warning(f"Near-zero denominator in SOLT calculation at {frequency/1e6:.1f} MHz")
            return 0+0j, 0+0j, 0+0j
        
        # Compute E00 (Directivity) - line 760
        e00 = (s_load_ideal * s_open * (s_short * (s_open_ideal - s_short_ideal) + s_load * s_short_ideal) - 
               s_load_ideal * s_open_ideal * s_load * s_short + 
               s_open_ideal * s_load * s_short_ideal * (s_short - s_open)) / denom
        
        # Compute E11 (Source match) - line 761
        e11 = (s_load_ideal * (s_open - s_short) + 
               s_open_ideal * (s_short - s_load) + 
               s_short_ideal * (s_load - s_open)) / denom
        
        # Compute delta - line 762
        delta = (s_load_ideal * s_load * (s_open - s_short) + 
                s_open_ideal * s_open * (s_short - s_load) + 
                s_short_ideal * s_short * (s_load - s_open)) / denom
        
        # Compute E10E01 (Reflection tracking) - line 763
        e10e01 = e00 * e11 - delta
        
        logger.debug(f"SOLT 1-port at {frequency/1e6:.1f} MHz: E00={e00:.6f}, E11={e11:.6f}, E10E01={e10e01:.6f}")
        
        return e00, e11, e10e01
    
    def _compute_through_error_terms(self, port1: int, port2: int, through_measurement: complex, 
                                   e00: complex, e11: complex, e10e01: complex, frequency: float) -> Tuple[complex, complex, complex]:
        """
        Compute through error terms (L, T, I) using LibreVNA GUI algorithm
        
        This implements the through error term computation from calibration.cpp:computeSOLT()
        lines 794-798
        
        Args:
            port1: First port number
            port2: Second port number
            through_measurement: Measured through value (S21)
            e00, e11, e10e01: 1-port error terms
            frequency: Frequency in Hz
            
        Returns:
            Tuple of (L, T, I) error terms
        """
        # For now, implement a simplified version
        # The full algorithm requires the actual through standard definition and S-parameter matrix
        
        # Ideal through standard (assuming perfect through)
        s11_ideal = 0+0j  # No reflection
        s21_ideal = 1+0j  # Perfect transmission
        s12_ideal = 1+0j  # Perfect reverse transmission
        s22_ideal = 0+0j  # No reflection
        
        # Extract measured values
        s11_meas = 0+0j  # Would need S11 measurement
        s21_meas = through_measurement  # S21 measurement
        s22_meas = 0+0j  # Would need S22 measurement
        
        # Isolation (simplified - would need isolation measurement)
        isolation = 0+0j
        
        # Compute delta for through standard
        delta_s = s11_ideal * s22_ideal - s21_ideal * s12_ideal
        
        # Load match calculation (line 795-796)
        # L = ((S11 - E00) * (1 - E11 * S11_ideal) - S11_ideal * E10E01) / 
        #     ((S11 - E00) * (S22_ideal - E11 * delta_s) - delta_s * E10E01)
        numerator_l = ((s11_meas - e00) * (1 - e11 * s11_ideal) - s11_ideal * e10e01)
        denominator_l = ((s11_meas - e00) * (s22_ideal - e11 * delta_s) - delta_s * e10e01)
        
        if abs(denominator_l) > 1e-12:
            l = numerator_l / denominator_l
        else:
            l = 0+0j
            logger.warning(f"Near-zero denominator in L calculation at {frequency/1e6:.1f} MHz")
        
        # Transmission tracking calculation (line 797)
        # T = (S21 - isolation) * (1 - E11 * S11_ideal - L * S22_ideal + E11 * L * delta_s) / S21_ideal
        t_numerator = (s21_meas - isolation) * (1 - e11 * s11_ideal - l * s22_ideal + e11 * l * delta_s)
        t = t_numerator / s21_ideal if abs(s21_ideal) > 1e-12 else s21_meas
        
        # Isolation
        i = isolation
        
        logger.debug(f"Through error terms at {frequency/1e6:.1f} MHz: L={l:.6f}, T={t:.6f}, I={i:.6f}")
        
        return l, t, i
    
    def _parse_complex(self, value: str) -> complex:
        """
        Parse complex number from string format
        
        Args:
            value: Complex number as string (e.g., "1.0+2.5i", "3.0-1.2i")
            
        Returns:
            Complex number
        """
        if isinstance(value, (int, float)):
            return complex(value)
        
        if isinstance(value, str):
            # Remove spaces and handle different formats
            value = value.replace(' ', '')
            
            # Handle pure real numbers
            if 'i' not in value and 'j' not in value:
                return complex(float(value))
            
            # Handle pure imaginary numbers
            if value in ['i', 'j']:
                return 1j
            if value == '-i' or value == '-j':
                return -1j
            
            # Replace 'i' with 'j' for Python complex parsing
            value = value.replace('i', 'j')
            
            try:
                return complex(value)
            except ValueError:
                # Try to parse manually for edge cases
                return self._manual_complex_parse(value.replace('j', 'i'))
        
        return complex(0)
    
    def _manual_complex_parse(self, value: str) -> complex:
        """Manually parse complex numbers for edge cases"""
        # Handle cases like "1.0+2.5i" or "3.0-1.2i"
        import re
        
        # Pattern to match real and imaginary parts
        pattern = r'([+-]?\d*\.?\d*)([+-]?\d*\.?\d*)i?'
        match = re.match(pattern, value)
        
        if match:
            real_part = float(match.group(1)) if match.group(1) else 0.0
            imag_part = float(match.group(2)) if match.group(2) else 0.0
            return complex(real_part, imag_part)
        
        return complex(0)
    
    def _validate_calibration(self) -> None:
        """Validate calibration data completeness and consistency"""
        if not self.calibration_points:
            raise ValueError("No calibration points found")
        
        if len(self.frequency_points) != len(self.calibration_points):
            raise ValueError("Frequency points and calibration points count mismatch")
        
        # Check for duplicate frequencies
        if len(set(self.frequency_points)) != len(self.frequency_points):
            raise ValueError("Duplicate frequency points found")
        
        # Validate error terms are not all zero
        has_valid_terms = False
        for point in self.calibration_points:
            if (abs(point.D) > 1e-10 or abs(point.R) > 1e-10 or 
                abs(point.S) > 1e-10 or abs(point.L) > 1e-10 or
                abs(point.T) > 1e-10 or abs(point.I) > 1e-10):
                has_valid_terms = True
                break
        
        if not has_valid_terms:
            logger.warning("All error terms appear to be zero - calibration may be invalid")
    
    def get_frequency_range(self) -> Tuple[float, float]:
        """
        Get frequency range of calibration data
        
        Returns:
            Tuple of (min_frequency, max_frequency) in Hz
        """
        if not self.frequency_points:
            return (0.0, 0.0)
        
        return (min(self.frequency_points), max(self.frequency_points))
    
    def get_calibration_point(self, frequency: float) -> Optional[CalPoint]:
        """
        Get calibration point for specific frequency
        
        Args:
            frequency: Frequency in Hz
            
        Returns:
            CalPoint if exact match found, None otherwise
        """
        for point in self.calibration_points:
            if abs(point.frequency - frequency) < 1e-6:
                return point
        return None
    
    def get_nearest_calibration_points(self, frequency: float, count: int = 2) -> List[CalPoint]:
        """
        Get nearest calibration points for interpolation
        
        Args:
            frequency: Target frequency in Hz
            count: Number of nearest points to return
            
        Returns:
            List of nearest CalPoint objects
        """
        if not self.calibration_points:
            return []
        
        # Calculate distances and sort
        distances = [(abs(point.frequency - frequency), point) 
                    for point in self.calibration_points]
        distances.sort(key=lambda x: x[0])
        
        return [point for _, point in distances[:count]]
    
    def is_frequency_in_range(self, frequency: float) -> bool:
        """
        Check if frequency is within calibration range
        
        Args:
            frequency: Frequency to check in Hz
            
        Returns:
            True if frequency is within range
        """
        if not self.frequency_points:
            return False
        
        min_freq, max_freq = self.get_frequency_range()
        return min_freq <= frequency <= max_freq
    
    def get_calibration_info(self) -> Dict[str, Any]:
        """
        Get summary information about the calibration
        
        Returns:
            Dictionary with calibration information
        """
        return {
            'file_path': self.file_path,
            'calibration_type': self.calibration_type,
            'used_ports': self.used_ports,
            'frequency_points': len(self.frequency_points),
            'frequency_range': self.get_frequency_range(),
            'calkit': self.calkit,
            'has_valid_terms': any(
                abs(point.D) > 1e-10 or abs(point.R) > 1e-10 or 
                abs(point.S) > 1e-10 or abs(point.L) > 1e-10 or
                abs(point.T) > 1e-10 or abs(point.I) > 1e-10
                for point in self.calibration_points
            )
        }
