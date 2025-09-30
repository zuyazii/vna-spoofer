"""
Transport layer for LibreVNA communication

This module provides transport abstractions for communicating with LibreVNA devices.
Supports multiple USB transport methods with TCP transport planned for future releases.

Classes:
- BaseTransport: Abstract base class for all transport implementations
- LibUSBTransport: USB communication using libusb-package (recommended for WinUSB drivers)
- USBTransport: USB communication using pyusb library (fallback for legacy systems)
- TCPTransport: TCP socket communication (planned)

Usage:
    from . import LibUSBTransport, USBTransport
    
    # Create LibUSB transport (recommended)
    transport = LibUSBTransport()
    
    # Or create pyusb transport (fallback)
    transport = USBTransport()
    
    # Connect to device
    transport.connect()
    
    # Send/receive data
    transport.send(b'\x01\x02\x03')
    response = transport.recv(64)
    
    # Disconnect
    transport.disconnect()
"""

from .base_transport import BaseTransport
from .usb_transport import USBTransport

# Import LibUSB transport with fallback
try:
    from .libusb_transport import LibUSBTransport
    __all__ = ["BaseTransport", "LibUSBTransport", "USBTransport"]
except ImportError:
    # LibUSB transport not available, only export pyusb transport
    __all__ = ["BaseTransport", "USBTransport"]
