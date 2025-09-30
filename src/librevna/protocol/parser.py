"""
Protocol parser for LibreVNA communication

This module provides parsing functionality for the LibreVNA binary protocol.
It handles incoming data from the device and converts it into structured
message objects that can be processed by the application.

The parser supports:
- Message header parsing
- Payload extraction
- Checksum validation
- Error handling and recovery
- Multiple message types

Usage:
    from . import ProtocolParser
    
    # Create parser
    parser = ProtocolParser()
    
    # Parse incoming data
    messages = parser.parse(data)
    for message in messages:
        print(f"Message type: {message['type']}")
        print(f"Payload: {message['payload']}")
    
    # Parse single message
    message = parser.parse_message(data)
"""

import struct
import zlib
from typing import List, Dict, Any, Optional, Tuple
from .exceptions import ParseError, ChecksumError


class ProtocolParser:
    """
    Protocol parser for LibreVNA binary protocol
    
    This class handles parsing of incoming protocol data from LibreVNA devices.
    It supports the binary protocol format used for device communication.
    """
    
    # Protocol constants
    SYNC_BYTE = 0x5A  # Sync byte for message start (LibreVNA uses 0x5A)
    HEADER_SIZE = 7  # Sync(1) + Length(2) + Type(1) + CRC32(4) = 8 bytes total
    CRC_SIZE = 4  # CRC32 size in bytes
    MAX_PAYLOAD_SIZE = 1024  # Maximum payload size
    
    # Message types (matching LibreVNA firmware protocol)
    MSG_TYPE_INFO = 0x01
    MSG_TYPE_DATA = 0x02
    MSG_TYPE_ERROR = 0x03
    MSG_TYPE_ACK = 0x04
    MSG_TYPE_VNA_DATAPOINT = 27  # VNA measurement datapoint (LibreVNA uses 27)
    
    def __init__(self):
        """Initialize protocol parser"""
        self._buffer = bytearray()
        self._message_types = {
            self.MSG_TYPE_INFO: 'INFO',
            self.MSG_TYPE_DATA: 'DATA',
            self.MSG_TYPE_ERROR: 'ERROR',
            self.MSG_TYPE_ACK: 'ACK',
            self.MSG_TYPE_VNA_DATAPOINT: 'VNA_DATAPOINT'
        }
    
    def parse(self, data: bytes) -> List[Dict[str, Any]]:
        """
        Parse incoming data and extract complete messages
        
        Args:
            data: Raw bytes received from device
            
        Returns:
            List[Dict]: List of parsed message dictionaries
            
        Raises:
            ParseError: If data cannot be parsed
            
        Example:
            # Parse multiple messages from received data
            messages = parser.parse(received_data)
            for message in messages:
                print(f"Type: {message['type']}, Payload: {message['payload']}")
        """
        if not isinstance(data, bytes):
            raise ParseError("Data must be bytes")
        
        # Add new data to buffer
        self._buffer.extend(data)
        
        messages = []
        
        # Process complete messages from buffer
        while len(self._buffer) >= 8:  # Minimum packet size
            try:
                # Try to parse a message
                message, consumed = self._parse_message_from_buffer()
                if message is not None:
                    messages.append(message)
                    # Remove consumed data from buffer
                    self._buffer = self._buffer[consumed:]
                else:
                    # No complete message found, wait for more data
                    break
                    
            except ParseError as e:
                # If parsing fails, try to find next sync byte
                sync_pos = self._find_next_sync()
                if sync_pos > 0:
                    # Remove data up to sync byte
                    self._buffer = self._buffer[sync_pos:]
                else:
                    # No sync byte found, clear buffer
                    self._buffer.clear()
                raise e
        
        return messages
    
    def _parse_message_from_buffer(self) -> Tuple[Optional[Dict[str, Any]], int]:
        """
        Parse a single message from the buffer using LibreVNA protocol format:
        0x5A + Length(2B) + Type(1B) + Payload + CRC32(4B)
        
        Returns:
            Tuple: (message_dict, bytes_consumed) or (None, 0) if incomplete
            
        Raises:
            ParseError: If message is malformed
        """
        # Need at least sync + length + type + crc = 8 bytes minimum
        if len(self._buffer) < 8:
            return None, 0
        
        # Check for sync byte
        if self._buffer[0] != self.SYNC_BYTE:
            raise ParseError(f"Invalid sync byte: 0x{self._buffer[0]:02x}", 
                           data=bytes(self._buffer[:1]), position=0)
        
        # Parse header: Length(2B) + Type(1B)
        try:
            length, msg_type = struct.unpack('<HB', self._buffer[1:4])
        except struct.error as e:
            raise ParseError(f"Failed to parse header: {e}", 
                           data=bytes(self._buffer[:4]), position=1)
        
        # Validate message type
        if msg_type not in self._message_types:
            raise ParseError(f"Unknown message type: 0x{msg_type:02x}", 
                           data=bytes(self._buffer[:4]), position=3)
        
        # Check if we have enough data for complete message
        # Total size = sync(1) + length(2) + type(1) + payload(length) + crc(4)
        total_size = 4 + length + self.CRC_SIZE
        if len(self._buffer) < total_size:
            return None, 0  # Incomplete message
        
        # Extract payload and CRC
        payload = bytes(self._buffer[4:4+length])
        received_crc = struct.unpack('<I', self._buffer[4+length:total_size])[0]
        
        # Calculate CRC32 for validation (sync + length + type + payload)
        data_for_crc = self._buffer[1:4+length]  # Exclude sync byte and CRC
        calculated_crc = zlib.crc32(data_for_crc) & 0xFFFFFFFF
        
        if calculated_crc != received_crc:
            raise ChecksumError(
                f"CRC32 mismatch",
                expected=received_crc,
                actual=calculated_crc
            )
        
        # Create message dictionary
        message = {
            'type': self._message_types[msg_type],
            'type_code': msg_type,
            'length': length,
            'payload': payload,
            'crc32': received_crc,
            'raw_data': bytes(self._buffer[:total_size])
        }
        
        return message, total_size
    
    def _find_next_sync(self) -> int:
        """
        Find the next sync byte in the buffer
        
        Returns:
            int: Position of next sync byte, or -1 if not found
        """
        for i in range(1, len(self._buffer)):
            if self._buffer[i] == self.SYNC_BYTE:
                return i
        return -1
    
    
    def parse_message(self, data: bytes) -> Dict[str, Any]:
        """
        Parse a single complete message
        
        Args:
            data: Complete message data
            
        Returns:
            Dict: Parsed message dictionary
            
        Raises:
            ParseError: If message cannot be parsed
            
        Example:
            # Parse a single message
            message = parser.parse_message(message_data)
            print(f"Message type: {message['type']}")
        """
        if not isinstance(data, bytes):
            raise ParseError("Data must be bytes")
        
        if len(data) < 8:  # Minimum packet size
            raise ParseError(f"Message too short: {len(data)} bytes", 
                           data=data, position=0)
        
        # Check sync byte
        if data[0] != self.SYNC_BYTE:
            raise ParseError(f"Invalid sync byte: 0x{data[0]:02x}", 
                           data=data[:1], position=0)
        
        # Parse header: Length(2B) + Type(1B)
        try:
            length, msg_type = struct.unpack('<HB', data[1:4])
        except struct.error as e:
            raise ParseError(f"Failed to parse header: {e}", 
                           data=data[:4], position=1)
        
        # Validate message type
        if msg_type not in self._message_types:
            raise ParseError(f"Unknown message type: 0x{msg_type:02x}", 
                           data=data[:4], position=3)
        
        # Check message length
        expected_size = 4 + length + self.CRC_SIZE
        if len(data) != expected_size:
            raise ParseError(f"Message size mismatch: expected {expected_size}, got {len(data)}", 
                           data=data, position=4)
        
        # Extract payload and CRC
        payload = data[4:4+length]
        received_crc = struct.unpack('<I', data[4+length:expected_size])[0]
        
        # Calculate CRC32 for validation
        data_for_crc = data[1:4+length]  # Exclude sync byte and CRC
        calculated_crc = zlib.crc32(data_for_crc) & 0xFFFFFFFF
        
        if calculated_crc != received_crc:
            raise ChecksumError(
                f"CRC32 mismatch",
                expected=received_crc,
                actual=calculated_crc
            )
        
        # Create message dictionary
        message = {
            'type': self._message_types[msg_type],
            'type_code': msg_type,
            'length': length,
            'payload': payload,
            'crc32': received_crc,
            'raw_data': data
        }
        
        return message
    
    def parse_vna_datapoint(self, payload: bytes) -> Dict[str, Any]:
        """
        Parse VNA datapoint payload based on LibreVNA protocol
        
        Args:
            payload: Raw payload bytes from VNA datapoint message
            
        Returns:
            Dict containing parsed VNA measurement data
            
        Based on LibreVNA firmware VNADatapoint structure:
        - Contains point number, frequency/power, and I/Q values for receivers
        - S-parameters calculated as input/ref ratios
        """
        if len(payload) < 32:  # Minimum size for VNADatapoint
            raise ParseError(f"VNA datapoint payload too small: {len(payload)} bytes")
        
        try:
            # Parse VNADatapoint structure based on LibreVNA firmware
            # Structure: point_num(4) + frequency/power(8) + I/Q values for receivers
            point_num = struct.unpack('<I', payload[0:4])[0]  # Point number
            
            # For sweep mode: frequency in Hz (8 bytes)
            # For zero-span mode: timestamp in microseconds (8 bytes)
            frequency_or_time = struct.unpack('<Q', payload[4:12])[0]
            
            # Parse I/Q values for Port1, Port2, and Ref receivers
            # Each receiver has I and Q values (4 bytes each = 8 bytes total)
            # Structure: Port1_I(4) + Port1_Q(4) + Port2_I(4) + Port2_Q(4) + Ref_I(4) + Ref_Q(4)
            port1_i = struct.unpack('<f', payload[12:16])[0]
            port1_q = struct.unpack('<f', payload[16:20])[0]
            port2_i = struct.unpack('<f', payload[20:24])[0]
            port2_q = struct.unpack('<f', payload[24:28])[0]
            ref_i = struct.unpack('<f', payload[28:32])[0]
            ref_q = struct.unpack('<f', payload[32:36])[0]
            
            # Create complex values for each receiver
            port1_complex = complex(port1_i, port1_q)
            port2_complex = complex(port2_i, port2_q)
            ref_complex = complex(ref_i, ref_q)
            
            # Calculate S-parameters as input/ref ratios
            # S11 = Port1 / Ref, S21 = Port2 / Ref, etc.
            s11 = port1_complex / ref_complex if ref_complex != 0 else complex(0, 0)
            s21 = port2_complex / ref_complex if ref_complex != 0 else complex(0, 0)
            
            return {
                'point_number': point_num,
                'frequency_or_time': frequency_or_time,
                'raw_receivers': {
                    'port1': port1_complex,
                    'port2': port2_complex,
                    'ref': ref_complex
                },
                's_parameters': {
                    's11': s11,
                    's21': s21
                },
                'raw_data': payload
            }
            
        except struct.error as e:
            raise ParseError(f"Failed to parse VNA datapoint structure: {e}")
    
    def clear_buffer(self) -> None:
        """
        Clear the internal parsing buffer
        
        This should be called when starting a new communication session
        or when recovering from parsing errors.
        
        Example:
            # Clear buffer before starting new session
            parser.clear_buffer()
        """
        self._buffer.clear()
    
    def get_buffer_size(self) -> int:
        """
        Get current buffer size
        
        Returns:
            int: Number of bytes in internal buffer
            
        Example:
            size = parser.get_buffer_size()
            print(f"Buffer contains {size} bytes")
        """
        return len(self._buffer)
    
    def __repr__(self) -> str:
        """String representation of parser"""
        return f"ProtocolParser(buffer_size={len(self._buffer)})"
