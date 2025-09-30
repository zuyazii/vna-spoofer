"""
Protocol layer for LibreVNA communication

This module provides protocol parsing and generation for LibreVNA devices.
It handles the binary protocol used for communication between the host
and the LibreVNA device.

Classes:
- ProtocolParser: Parse incoming protocol messages from device
- ProtocolGenerator: Generate outgoing protocol messages to device
- ProtocolError: Exception for protocol-related errors

Usage:
    from . import ProtocolParser, ProtocolGenerator
    
    # Parse incoming data
    parser = ProtocolParser()
    messages = parser.parse(data)
    
    # Generate outgoing commands
    generator = ProtocolGenerator()
    command = generator.create_get_info_command()
"""

from .parser import ProtocolParser
from .generator import ProtocolGenerator
from .exceptions import ProtocolError

__all__ = ["ProtocolParser", "ProtocolGenerator", "ProtocolError"]
