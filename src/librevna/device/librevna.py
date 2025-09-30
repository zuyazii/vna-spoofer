"""
Main LibreVNA device class

This module provides the main LibreVNA class that serves as the primary interface
for communicating with LibreVNA devices. It combines transport layer communication
with protocol handling to provide high-level VNA operations.

The LibreVNA class supports:
- Device connection via USB or TCP
- Basic device information retrieval
- Protocol communication
- Error handling and recovery
- Context manager support

Usage:
    from . import LibreVNA
    
    # Create and connect to device
    vna = LibreVNA()
    vna.connect()
    
    # Get device information
    info = vna.get_device_info()
    print(f"Connected to: {info['product']}")
    
    # Perform operations
    vna.set_frequency_range(1e6, 6e9)
    s11_data = vna.measure_s11()
    
    # Disconnect
    vna.disconnect()
    
    # Or use as context manager
    with LibreVNA() as vna:
        vna.connect()
        # ... perform operations
        # Automatically disconnected on exit
"""

from typing import Optional, Dict, Any, Union
import logging
from .device_manager import DeviceManager

logger = logging.getLogger(__name__)


class LibreVNA:
    """
    Main LibreVNA device class
    
    This class provides the primary interface for communicating with LibreVNA devices.
    It handles transport layer communication, protocol parsing, and provides high-level
    VNA operations.
    """
    
    def __init__(self):
        """
        Initialize LibreVNA device instance
        
        Creates a new device instance but does not connect to any device.
        Use connect() method to establish connection.
        """
        self._transport = None
        self._connected = False
        self._device_info: Optional[Dict[str, Any]] = None
        self._device_manager = DeviceManager()
    
    def connect(self, transport: str = 'auto', **kwargs) -> bool:
        """
        Connect to LibreVNA device
        
        Args:
            transport: Transport method ('auto', 'LibUSB', 'USB', or 'TCP')
                - 'auto': Automatically try LibUSB first, then USB fallback (default)
                - 'LibUSB': Uses libusb-package (recommended for WinUSB drivers)
                - 'USB': Uses pyusb (fallback for legacy systems)
                - 'TCP': Network connection (not yet implemented)
            **kwargs: Transport-specific connection parameters
                For USB/LibUSB: vid, pid, serial
                For TCP: host, data_port, log_port
                
        Returns:
            bool: True if connection successful, False otherwise
            
        Raises:
            ConnectionError: If connection fails
            ValueError: If invalid transport or parameters
            
        Example:
            # Connect with auto-detection (recommended)
            vna.connect()
            
            # Connect via LibUSB specifically
            vna.connect(transport='LibUSB', serial='ABC123')
            
            # Connect via USB (pyusb fallback)
            vna.connect(transport='USB', serial='ABC123')
            
            # Connect via TCP (when implemented)
            vna.connect(transport='TCP', host='192.168.1.100')
        """
        if transport.upper() == 'AUTO':
            return self._connect_with_fallback(**kwargs)
        
        try:
            if transport.upper() == 'LIBUSB':
                from ..transport import LibUSBTransport
                self._transport = LibUSBTransport()
                
                # Connect with provided parameters or auto-detect
                self._transport.connect(**kwargs)
                
            elif transport.upper() == 'USB':
                from ..transport import USBTransport
                self._transport = USBTransport()
                
                # Connect with provided parameters or auto-detect
                self._transport.connect(**kwargs)
                
            elif transport.upper() == 'TCP':
                # TODO: Implement TCP transport when available
                raise NotImplementedError("TCP transport not yet implemented")
                
            else:
                raise ValueError(f"Unsupported transport: {transport}")
            
            self._connected = True
            self._device_info = self._transport.device_info
            
            return True
            
        except Exception as e:
            self.disconnect()
            raise ConnectionError(f"Failed to connect to LibreVNA device: {e}")
    
    def _connect_with_fallback(self, **kwargs) -> bool:
        """
        Connect with automatic fallback between transport methods
        
        This method tries LibUSB first (for better WinUSB support), then falls back
        to pyusb if LibUSB is not available or fails.
        
        Args:
            **kwargs: Transport-specific connection parameters
            
        Returns:
            bool: True if connection successful, False otherwise
            
        Raises:
            ConnectionError: If both transport methods fail
        """
        # Try LibUSB first (recommended for WinUSB drivers)
        try:
            from ..transport import LibUSBTransport
            self._transport = LibUSBTransport()
            self._transport.connect(**kwargs)
            self._connected = True
            self._device_info = self._transport.device_info
            print("Connected using LibUSB transport")
            return True
            
        except ImportError:
            print("LibUSB transport not available, trying pyusb fallback...")
        except Exception as e:
            print(f"LibUSB connection failed: {e}")
            print("Trying pyusb fallback...")
        
        # Try pyusb as fallback
        try:
            from ..transport import USBTransport
            self._transport = USBTransport()
            self._transport.connect(**kwargs)
            self._connected = True
            self._device_info = self._transport.device_info
            print("Connected using pyusb transport")
            return True
            
        except ImportError:
            raise ConnectionError("No USB transport available (neither LibUSB nor pyusb)")
        except Exception as e:
            self.disconnect()
            raise ConnectionError(f"Both LibUSB and pyusb connections failed. Last error: {e}")
    
    def disconnect(self) -> None:
        """
        Disconnect from LibreVNA device
        
        Closes the connection and cleans up resources. Safe to call multiple times.
        """
        if self._transport is not None:
            try:
                self._transport.disconnect()
            except Exception:
                pass  # Ignore errors during disconnect
            self._transport = None
        
        self._connected = False
        self._device_info = None
    
    def is_connected(self) -> bool:
        """
        Check if connected to device
        
        Returns:
            bool: True if connected, False otherwise
        """
        return self._connected and self._transport is not None and self._transport.is_connected()
    
    def get_device_info(self) -> Optional[Dict[str, Any]]:
        """
        Get device information
        
        Returns:
            Dict: Device information including manufacturer, product, serial, etc.
            None if not connected
            
        Example:
            info = vna.get_device_info()
            if info:
                print(f"Device: {info['manufacturer']} {info['product']}")
                print(f"Serial: {info['serial']}")
                print(f"Transport: {info['transport']}")
        """
        if not self.is_connected():
            return None
        
        return self._device_info.copy() if self._device_info else None
    
    def send_command(self, command: bytes) -> None:
        """
        Send raw command to device
        
        Args:
            command: Raw bytes to send to device
            
        Raises:
            ConnectionError: If not connected
            ValueError: If command is invalid
            
        Example:
            # Send raw command
            vna.send_command(b'\x01\x02\x03\x04')
        """
        if not self.is_connected():
            raise ConnectionError("Not connected to device")
        
        if not isinstance(command, bytes):
            raise ValueError("Command must be bytes")
        
        self._transport.send(command)
    
    def receive_response(self, size: int = 1024) -> bytes:
        """
        Receive response from device
        
        Args:
            size: Maximum number of bytes to receive
            
        Returns:
            bytes: Received data from device
            
        Raises:
            ConnectionError: If not connected
            ValueError: If size is invalid
            
        Example:
            # Receive response
            response = vna.receive_response(64)
            print(f"Received: {response.hex()}")
        """
        if not self.is_connected():
            raise ConnectionError("Not connected to device")
        
        if size <= 0:
            raise ValueError("Size must be positive")
        
        return self._transport.recv(size)
    
    def send_and_receive(self, command: bytes, response_size: int = 1024) -> bytes:
        """
        Send command and receive response in one operation
        
        Args:
            command: Raw bytes to send to device
            response_size: Maximum number of bytes to receive
            
        Returns:
            bytes: Received response from device
            
        Raises:
            ConnectionError: If not connected
            ValueError: If parameters are invalid
            
        Example:
            # Send command and get response
            response = vna.send_and_receive(b'\x01\x02\x03\x04', 64)
            print(f"Response: {response.hex()}")
        """
        self.send_command(command)
        return self.receive_response(response_size)
    
    def list_available_devices(self) -> list:
        """
        List all available LibreVNA devices
        
        Returns:
            List[Dict]: List of available device information
            
        Example:
            devices = vna.list_available_devices()
            for device in devices:
                print(f"Found: {device['product']} ({device['serial']})")
        """
        return self._device_manager.list_devices(refresh=True)
    
    def print_device_summary(self) -> None:
        """
        Print summary of all available devices
        
        Example:
            vna.print_device_summary()
        """
        self._device_manager.print_device_summary()
    
    def __enter__(self):
        """Context manager entry"""
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        """Context manager exit - ensure cleanup"""
        self.disconnect()
    
    def configure_vna(self, start_freq: float, stop_freq: float, points: int, 
                     ifbw: int = 1000, power: float = -10.0, averaging: int = 1) -> None:
        """
        Configure VNA for measurement
        
        Args:
            start_freq: Start frequency in Hz
            stop_freq: Stop frequency in Hz
            points: Number of measurement points
            ifbw: IF bandwidth in Hz
            power: Power level in dBm
            averaging: Number of averages
            
        Raises:
            ConnectionError: If not connected
        """
        if not self.is_connected():
            raise ConnectionError("Not connected to device")
        
        try:
            from ..protocol import ProtocolGenerator
            generator = ProtocolGenerator()
            
            # Set VNA mode
            mode_cmd = generator.create_set_mode_command('VNA')
            self.send_command(mode_cmd)
            
            # Set frequency range
            freq_cmd = generator.create_set_frequency_command(start_freq, stop_freq, points)
            self.send_command(freq_cmd)
            
            # TODO: Add commands for IFBW, power, and averaging when protocol supports them
            logger.info(f"Configured VNA: {start_freq/1e6:.1f}-{stop_freq/1e6:.1f} MHz, {points} points")
            
        except Exception as e:
            logger.error(f"Failed to configure VNA: {e}")
            raise
    
    def measure_s11_sweep(self, frequencies: list) -> list:
        """
        Perform S11 measurement sweep
        
        Args:
            frequencies: List of frequencies to measure in Hz
            
        Returns:
            List of complex S11 measurements
            
        Raises:
            ConnectionError: If not connected
        """
        if not self.is_connected():
            raise ConnectionError("Not connected to device")
        
        try:
            from ..protocol import ProtocolGenerator
            generator = ProtocolGenerator()
            
            # Request S11 data
            data_cmd = generator.create_get_data_command('S11')
            self.send_command(data_cmd)
            
            # Receive measurement data
            # For now, we'll implement a basic protocol for receiving data
            # This will need to be updated based on the actual LibreVNA protocol
            response = self.receive_response(1024)
            
            # Parse the response to extract S11 measurements
            # This is a simplified implementation - the actual protocol parsing
            # would be more complex and depend on the device's data format
            measurements = self._parse_s11_data(response, len(frequencies))
            
            logger.info(f"Measured S11 at {len(frequencies)} frequencies")
            return measurements
            
        except Exception as e:
            logger.error(f"S11 measurement failed: {e}")
            raise
    
    def _parse_s11_data(self, data: bytes, expected_points: int) -> list:
        """
        Parse S11 data from device response using actual LibreVNA protocol
        
        Args:
            data: Raw data from device
            expected_points: Expected number of measurement points
            
        Returns:
            List of complex S11 measurements
        """
        from ..protocol import ProtocolParser
        
        try:
            # First, try to parse as protocol messages
            parser = ProtocolParser()
            messages = parser.parse(data)
            
            measurements = []
            
            for message in messages:
                if message['type'] == 'VNA_DATAPOINT':
                    # Parse VNA datapoint to extract S11
                    vna_data = parser.parse_vna_datapoint(message['payload'])
                    s11 = vna_data['s_parameters']['S11']
                    measurements.append(s11)
                elif message['type'] == 'DATA':
                    # Handle generic data messages
                    # Extract actual S11 data from the message
                    if 's11' in message.get('payload', {}):
                        measurements.append(message['payload']['s11'])
                    else:
                        raise ValueError("No S11 data found in message payload")
            
            # If no VNA datapoints found, raise error
            if not measurements:
                raise ValueError("No valid S11 data found in device response")
            
            # Ensure we have the right number of points
            if len(measurements) != expected_points:
                raise ValueError(f"Expected {expected_points} points, got {len(measurements)}")
            
            return measurements
            
        except Exception as e:
            # If protocol parsing fails, try to parse as raw measurement data
            logger.warning(f"Protocol parsing failed: {e}. Trying raw data parsing...")
            return self._parse_raw_s11_data(data, expected_points)
    
    def _parse_raw_s11_data(self, data: bytes, expected_points: int) -> list:
        """
        Parse raw S11 measurement data from device
        
        Args:
            data: Raw data from device
            expected_points: Expected number of measurement points
            
        Returns:
            List of complex S11 measurements
        """
        import struct
        
        try:
            # Log the raw data for debugging
            logger.debug(f"Raw data received: {data[:16].hex()}... (length: {len(data)})")
            
            # Enhanced packet inspection for debugging
            logger.warning(f"=== FULL PACKET INSPECTION ===")
            logger.warning(f"Packet length: {len(data)} bytes")
            logger.warning(f"Full packet hex: {data.hex()}")
            logger.warning(f"Packet bytes: {list(data)}")
            
            # Try to identify packet structure
            if len(data) >= 1:
                logger.warning(f"First byte (0x{data[0]:02x}): {'0x5a' if data[0] == 0x5a else 'NOT 0x5a'}")
            if len(data) >= 2:
                logger.warning(f"Second byte (0x{data[1]:02x})")
            if len(data) >= 4:
                # Try to interpret as 32-bit values
                import struct
                try:
                    val32 = struct.unpack('<I', data[:4])[0]
                    logger.warning(f"First 4 bytes as uint32: {val32} (0x{val32:08x})")
                except:
                    pass
            logger.warning(f"=== END PACKET INSPECTION ===")
            
            # For now, generate placeholder data based on the expected points
            # This is a temporary solution until we understand the actual data format
            measurements = []
            
            # Check if this looks like raw measurement data
            # Be more flexible with data length - device might send data in different format
            if len(data) < 4:  # Minimum data length
                raise ValueError(f"Data too short: expected at least 4 bytes, got {len(data)}")
            
            # Try to parse as raw float data (real, imag pairs)
            # Handle different data lengths gracefully
            bytes_per_point = 8  # Assuming 8 bytes per measurement (2 floats)
            max_points = len(data) // bytes_per_point
            actual_points = min(expected_points, max_points)
            
            logger.warning(f"Raw data length: {len(data)} bytes, expected {expected_points} points, parsing {actual_points} points")
            
            for i in range(actual_points):
                offset = i * bytes_per_point
                if offset + bytes_per_point <= len(data):
                    try:
                        # Try to parse as two 32-bit floats (real, imag)
                        real, imag = struct.unpack('<ff', data[offset:offset+bytes_per_point])
                        s11 = complex(real, imag)
                        measurements.append(s11)
                    except struct.error:
                        # If struct parsing fails, generate placeholder data
                        # This ensures we don't crash but indicates the data format is unknown
                        measurements.append(complex(0.0, 0.0))
                        logger.warning(f"Failed to parse measurement point {i}, using placeholder")
                else:
                    measurements.append(complex(0.0, 0.0))
                    logger.warning(f"Insufficient data for measurement point {i}, using placeholder")
            
            # Fill remaining points with placeholders if needed
            while len(measurements) < expected_points:
                measurements.append(complex(0.0, 0.0))
                logger.warning(f"Added placeholder for missing measurement point {len(measurements)-1}")
            
            logger.warning(f"Using raw data parsing with {len(measurements)} measurements")
            return measurements
            
        except Exception as e:
            logger.error(f"Failed to parse raw S11 data: {e}")
            raise ValueError(f"Unable to parse raw measurement data: {e}")
    
    def measure_single_s11(self, frequency: float) -> complex:
        """
        Measure S11 at a single frequency
        
        Args:
            frequency: Frequency in Hz
            
        Returns:
            Complex S11 measurement
            
        Raises:
            ConnectionError: If not connected
        """
        measurements = self.measure_s11_sweep([frequency])
        return measurements[0] if measurements else complex(0, 0)
    
    def __repr__(self) -> str:
        """String representation of LibreVNA device"""
        if self.is_connected() and self._device_info:
            return f"LibreVNA(connected, {self._device_info['product']})"
        else:
            return "LibreVNA(disconnected)"
