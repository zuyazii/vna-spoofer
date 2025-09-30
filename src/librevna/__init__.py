"""
LibreVNA Python CLI - A command-line interface for LibreVNA Vector Network Analyzer

This package provides a Python interface to communicate with LibreVNA devices
via USB and perform vector network analysis operations including calibration
and S-parameter measurements.

Main Components:
- transport: USB and TCP communication layers
- device: Device discovery and management
- protocol: LibreVNA protocol parsing and generation
- calibration: Calibration algorithms and procedures
- cli: Command-line interface

Usage:
    from . import LibreVNA
    
    # Connect to device
    vna = LibreVNA()
    vna.connect()
    
    # Perform measurements
    s11_data = vna.measure_s11(frequency_range=(1e6, 6e9))
    
    # Disconnect
    vna.disconnect()
"""

__version__ = "0.1.0"
__author__ = "VNA-CLI Development Team"

from .device import LibreVNA
from .transport import USBTransport
from .protocol import ProtocolParser, ProtocolGenerator

__all__ = [
    "LibreVNA",
    "USBTransport", 
    "ProtocolParser",
    "ProtocolGenerator"
]
