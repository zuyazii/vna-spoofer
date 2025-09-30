"""
Device management for LibreVNA

This module provides device discovery, connection management, and high-level
operations for LibreVNA devices. It acts as the main interface between the
transport layer and the protocol layer.

Classes:
- LibreVNA: Main device class for VNA operations
- DeviceManager: Device discovery and management utilities

Usage:
    from . import LibreVNA
    
    # Create device instance
    vna = LibreVNA()
    
    # Connect to device
    vna.connect()
    
    # Perform operations
    info = vna.get_device_info()
    vna.set_frequency_range(1e6, 6e9)
    
    # Disconnect
    vna.disconnect()
"""

from .librevna import LibreVNA
from .device_manager import DeviceManager

__all__ = ["LibreVNA", "DeviceManager"]
