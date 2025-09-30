"""
Protocol generator for LibreVNA communication

This module provides protocol generation functionality for the LibreVNA binary protocol.
It creates outgoing messages in the correct format for communication with LibreVNA devices.

The generator supports:
- Message header generation
- Payload creation
- Checksum calculation
- Multiple message types
- Parameter validation

Usage:
    from . import ProtocolGenerator
    
    # Create generator
    generator = ProtocolGenerator()
    
    # Generate commands
    get_info_cmd = generator.create_get_info_command()
    set_freq_cmd = generator.create_set_frequency_command(1e6, 6e9)
    
    # Send commands to device
    device.send_command(get_info_cmd)
"""

import struct
from typing import Dict, Any, Optional, Union
from .exceptions import GenerationError


class ProtocolGenerator:
    """
    Protocol generator for LibreVNA binary protocol
    
    This class handles generation of outgoing protocol messages for LibreVNA devices.
    It creates properly formatted binary messages with headers, payloads, and checksums.
    """
    
    # Protocol constants
    HEADER_SIZE = 8  # Size of protocol header in bytes
    SYNC_BYTE = 0xAA  # Sync byte for message start
    MAX_PAYLOAD_SIZE = 1024  # Maximum payload size
    
    # Command types (these will be updated based on actual protocol)
    CMD_GET_INFO = 0x01
    CMD_SET_FREQUENCY = 0x02
    CMD_GET_DATA = 0x03
    CMD_SET_MODE = 0x04
    CMD_CALIBRATE = 0x05
    
    def __init__(self):
        """Initialize protocol generator"""
        self._command_types = {
            'GET_INFO': self.CMD_GET_INFO,
            'SET_FREQUENCY': self.CMD_SET_FREQUENCY,
            'GET_DATA': self.CMD_GET_DATA,
            'SET_MODE': self.CMD_SET_MODE,
            'CALIBRATE': self.CMD_CALIBRATE
        }
    
    def create_message(self, command_type: Union[str, int], payload: bytes = b'') -> bytes:
        """
        Create a protocol message with header and payload
        
        Args:
            command_type: Command type (string name or integer code)
            payload: Message payload data
            
        Returns:
            bytes: Complete protocol message
            
        Raises:
            GenerationError: If message cannot be generated
            
        Example:
            # Create message with string command type
            msg = generator.create_message('GET_INFO')
            
            # Create message with integer command type
            msg = generator.create_message(0x01, b'\x00\x01')
        """
        # Convert command type to integer if needed
        if isinstance(command_type, str):
            if command_type not in self._command_types:
                raise GenerationError(f"Unknown command type: {command_type}")
            cmd_code = self._command_types[command_type]
        else:
            cmd_code = command_type
        
        # Validate payload
        if not isinstance(payload, bytes):
            raise GenerationError("Payload must be bytes")
        
        if len(payload) > self.MAX_PAYLOAD_SIZE:
            raise GenerationError(f"Payload too large: {len(payload)} bytes (max: {self.MAX_PAYLOAD_SIZE})")
        
        # Create message data (without sync byte and checksum)
        message_data = struct.pack('<BHH', cmd_code, len(payload), 0)  # reserved field
        message_data += payload
        
        # Calculate checksum
        checksum = self._calculate_checksum(message_data)
        
        # Create complete message
        message = struct.pack('<B', self.SYNC_BYTE)  # Sync byte
        message += struct.pack('<BHHH', cmd_code, len(payload), checksum, 0)  # Header
        message += payload  # Payload
        
        return message
    
    def create_get_info_command(self) -> bytes:
        """
        Create GET_INFO command message
        
        Returns:
            bytes: GET_INFO command message
            
        Example:
            # Create and send GET_INFO command
            cmd = generator.create_get_info_command()
            device.send_command(cmd)
        """
        return self.create_message('GET_INFO')
    
    def create_set_frequency_command(self, start_freq: float, stop_freq: float, 
                                   points: int = 101) -> bytes:
        """
        Create SET_FREQUENCY command message
        
        Args:
            start_freq: Start frequency in Hz
            stop_freq: Stop frequency in Hz
            points: Number of frequency points
            
        Returns:
            bytes: SET_FREQUENCY command message
            
        Raises:
            GenerationError: If parameters are invalid
            
        Example:
            # Set frequency range from 1 MHz to 6 GHz with 101 points
            cmd = generator.create_set_frequency_command(1e6, 6e9, 101)
            device.send_command(cmd)
        """
        # Validate parameters
        if start_freq < 0 or stop_freq < 0:
            raise GenerationError("Frequencies must be positive")
        
        if start_freq >= stop_freq:
            raise GenerationError("Start frequency must be less than stop frequency")
        
        if points < 2 or points > 10000:
            raise GenerationError("Points must be between 2 and 10000")
        
        # Create payload
        payload = struct.pack('<ddI', start_freq, stop_freq, points)
        
        return self.create_message('SET_FREQUENCY', payload)
    
    def create_get_data_command(self, data_type: str = 'S11') -> bytes:
        """
        Create GET_DATA command message
        
        Args:
            data_type: Type of data to request ('S11', 'S21', 'S12', 'S22')
            
        Returns:
            bytes: GET_DATA command message
            
        Raises:
            GenerationError: If data type is invalid
            
        Example:
            # Request S11 data
            cmd = generator.create_get_data_command('S11')
            device.send_command(cmd)
        """
        # Validate data type
        valid_types = ['S11', 'S21', 'S12', 'S22']
        if data_type not in valid_types:
            raise GenerationError(f"Invalid data type: {data_type}. Valid types: {valid_types}")
        
        # Create payload (data type as 4-byte string)
        payload = data_type.encode('ascii').ljust(4, b'\x00')
        
        return self.create_message('GET_DATA', payload)
    
    def create_set_mode_command(self, mode: str) -> bytes:
        """
        Create SET_MODE command message
        
        Args:
            mode: Operating mode ('VNA', 'GENERATOR', 'SPECTRUM_ANALYZER')
            
        Returns:
            bytes: SET_MODE command message
            
        Raises:
            GenerationError: If mode is invalid
            
        Example:
            # Set VNA mode
            cmd = generator.create_set_mode_command('VNA')
            device.send_command(cmd)
        """
        # Validate mode
        valid_modes = ['VNA', 'GENERATOR', 'SPECTRUM_ANALYZER']
        if mode not in valid_modes:
            raise GenerationError(f"Invalid mode: {mode}. Valid modes: {valid_modes}")
        
        # Create payload (mode as 4-byte string)
        payload = mode.encode('ascii').ljust(4, b'\x00')
        
        return self.create_message('SET_MODE', payload)
    
    def create_calibrate_command(self, cal_type: str, port: int = 1) -> bytes:
        """
        Create CALIBRATE command message
        
        Args:
            cal_type: Calibration type ('OPEN', 'SHORT', 'LOAD', 'THROUGH')
            port: Port number (1 or 2)
            
        Returns:
            bytes: CALIBRATE command message
            
        Raises:
            GenerationError: If parameters are invalid
            
        Example:
            # Calibrate port 1 with open standard
            cmd = generator.create_calibrate_command('OPEN', 1)
            device.send_command(cmd)
        """
        # Validate parameters
        valid_types = ['OPEN', 'SHORT', 'LOAD', 'THROUGH']
        if cal_type not in valid_types:
            raise GenerationError(f"Invalid calibration type: {cal_type}. Valid types: {valid_types}")
        
        if port not in [1, 2]:
            raise GenerationError("Port must be 1 or 2")
        
        # Create payload
        payload = struct.pack('<BI', port, 0)  # port + reserved
        payload += cal_type.encode('ascii').ljust(4, b'\x00')  # calibration type
        
        return self.create_message('CALIBRATE', payload)
    
    def _calculate_checksum(self, data: bytes) -> int:
        """
        Calculate checksum for data
        
        Args:
            data: Data to calculate checksum for
            
        Returns:
            int: Calculated checksum value
        """
        # Simple checksum implementation (will be updated based on actual protocol)
        checksum = 0
        for byte in data:
            checksum = (checksum + byte) & 0xFFFF
        return checksum
    
    def get_supported_commands(self) -> Dict[str, int]:
        """
        Get dictionary of supported commands
        
        Returns:
            Dict[str, int]: Dictionary mapping command names to codes
            
        Example:
            commands = generator.get_supported_commands()
            print("Supported commands:")
            for name, code in commands.items():
                print(f"  {name}: 0x{code:02x}")
        """
        return self._command_types.copy()
    
    def __repr__(self) -> str:
        """String representation of generator"""
        return f"ProtocolGenerator(commands={len(self._command_types)})"
