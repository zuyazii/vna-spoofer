"""
Device manager for LibreVNA discovery and management

This module provides utilities for discovering, listing, and managing LibreVNA devices.
It handles device enumeration, connection validation, and provides a unified interface
for device operations regardless of the transport method.

Usage:
    from . import DeviceManager
    
    # Create device manager
    manager = DeviceManager()
    
    # List available devices
    devices = manager.list_devices()
    for device in devices:
        print(f"Found: {device['manufacturer']} {device['product']}")
        print(f"Serial: {device['serial']}")
        print(f"Transport: {device['transport']}")
    
    # Get device by serial
    device = manager.get_device_by_serial("ABC123")
    
    # Validate device connection
    if manager.validate_device(device):
        print("Device is ready for use")
"""

from typing import List, Dict, Any, Optional


class DeviceManager:
    """
    Device manager for LibreVNA discovery and management
    
    This class provides utilities for discovering, listing, and managing LibreVNA devices.
    It supports multiple transport methods and provides a unified interface for device operations.
    """
    
    def __init__(self):
        """Initialize device manager"""
        self._devices: List[Dict[str, Any]] = []
        self._refresh_devices()
    
    def _refresh_devices(self) -> None:
        """
        Refresh the list of available devices
        
        This method scans for available LibreVNA devices using all supported
        transport methods and updates the internal device list. It tries LibUSB
        first for better WinUSB driver support, then falls back to pyusb.
        """
        self._devices = []
        
        # Try LibUSB transport first (better WinUSB support)
        try:
            from ..transport import LibUSBTransport
            libusb_transport = LibUSBTransport()
            libusb_devices = libusb_transport.discover_devices()
            
            for device in libusb_devices:
                device_info = {
                    'transport': 'LibUSB',
                    'vid': device['vid'],
                    'pid': device['pid'],
                    'serial': device['serial'],
                    'manufacturer': device['manufacturer'],
                    'product': device['product'],
                    'connection_params': {
                        'vid': device['vid'],
                        'pid': device['pid'],
                        'serial': device['serial']
                    }
                }
                self._devices.append(device_info)
                
        except ImportError:
            # LibUSB transport not available, try pyusb fallback
            print("LibUSB transport not available, trying pyusb fallback...")
            self._discover_pyusb_devices()
        except Exception as e:
            print(f"Warning: LibUSB device discovery failed: {e}")
            print("Trying pyusb fallback...")
            self._discover_pyusb_devices()
        
        # TODO: Add TCP device discovery when implemented
        # TCP devices would be discovered by scanning network ranges
    
    def _discover_pyusb_devices(self) -> None:
        """
        Discover devices using pyusb as fallback
        
        This method is called when LibUSB transport is not available or fails.
        It provides backward compatibility with the original pyusb implementation.
        """
        try:
            from ..transport import USBTransport
            usb_transport = USBTransport()
            usb_devices = usb_transport.discover_devices()
            
            for device in usb_devices:
                device_info = {
                    'transport': 'USB',
                    'vid': device['vid'],
                    'pid': device['pid'],
                    'serial': device['serial'],
                    'manufacturer': device['manufacturer'],
                    'product': device['product'],
                    'connection_params': {
                        'vid': device['vid'],
                        'pid': device['pid'],
                        'serial': device['serial']
                    }
                }
                self._devices.append(device_info)
                
        except ImportError:
            # USB transport not available
            print("Warning: No USB transport available (neither LibUSB nor pyusb)")
        except Exception as e:
            print(f"Warning: pyusb device discovery failed: {e}")
    
    def list_devices(self, refresh: bool = False) -> List[Dict[str, Any]]:
        """
        List all available LibreVNA devices
        
        Args:
            refresh: If True, refresh device list before returning
            
        Returns:
            List[Dict]: List of device information dictionaries
            
        Example:
            devices = manager.list_devices()
            for device in devices:
                print(f"Device: {device['manufacturer']} {device['product']}")
                print(f"Serial: {device['serial']}")
                print(f"Transport: {device['transport']}")
        """
        if refresh:
            self._refresh_devices()
        
        return self._devices.copy()
    
    def get_device_by_serial(self, serial: str) -> Optional[Dict[str, Any]]:
        """
        Get device information by serial number
        
        Args:
            serial: Device serial number
            
        Returns:
            Dict: Device information if found, None otherwise
            
        Example:
            device = manager.get_device_by_serial("ABC123")
            if device:
                print(f"Found device: {device['product']}")
            else:
                print("Device not found")
        """
        for device in self._devices:
            if device['serial'] == serial:
                return device
        return None
    
    def get_device_by_transport(self, transport: str) -> List[Dict[str, Any]]:
        """
        Get devices by transport type
        
        Args:
            transport: Transport type ('USB', 'TCP', etc.)
            
        Returns:
            List[Dict]: List of devices using specified transport
            
        Example:
            usb_devices = manager.get_device_by_transport('USB')
            print(f"Found {len(usb_devices)} USB devices")
        """
        return [device for device in self._devices if device['transport'] == transport]
    
    def validate_device(self, device_info: Dict[str, Any]) -> bool:
        """
        Validate that a device is accessible and ready for use
        
        Args:
            device_info: Device information dictionary
            
        Returns:
            bool: True if device is valid and accessible, False otherwise
            
        Example:
            device = manager.get_device_by_serial("ABC123")
            if device and manager.validate_device(device):
                print("Device is ready for use")
            else:
                print("Device is not accessible")
        """
        try:
            # Import here to avoid circular import
            from .librevna import LibreVNA
            
            # Create LibreVNA instance and attempt connection
            vna = LibreVNA()
            
            # Connect using device parameters
            if device_info['transport'] in ['LibUSB', 'USB']:
                connection_params = device_info['connection_params']
                vna.connect(
                    transport=device_info['transport'],
                    vid=connection_params['vid'],
                    pid=connection_params['pid'],
                    serial=connection_params['serial']
                )
            else:
                # Handle other transport types when implemented
                return False
            
            # Test basic communication
            info = vna.get_device_info()
            vna.disconnect()
            
            return info is not None
            
        except Exception as e:
            print(f"Device validation failed: {e}")
            return False
    
    def get_device_count(self) -> int:
        """
        Get total number of available devices
        
        Returns:
            int: Number of available devices
            
        Example:
            count = manager.get_device_count()
            print(f"Found {count} LibreVNA devices")
        """
        return len(self._devices)
    
    def get_transport_counts(self) -> Dict[str, int]:
        """
        Get count of devices by transport type
        
        Returns:
            Dict[str, int]: Dictionary mapping transport types to device counts
            
        Example:
            counts = manager.get_transport_counts()
            print(f"USB devices: {counts.get('USB', 0)}")
            print(f"TCP devices: {counts.get('TCP', 0)}")
        """
        counts = {}
        for device in self._devices:
            transport = device['transport']
            counts[transport] = counts.get(transport, 0) + 1
        return counts
    
    def print_device_summary(self) -> None:
        """
        Print a summary of all available devices
        
        This method provides a formatted output showing all discovered devices
        with their key information.
        
        Example:
            manager.print_device_summary()
            # Output:
            # LibreVNA Devices Summary:
            # ========================
            # 1. LibreVNA (USB)
            #    Serial: ABC123
            #    Manufacturer: LibreVNA
            #    VID/PID: 0x1209/0x4121
            # 
            # 2. LibreVNA (TCP)
            #    Host: 192.168.1.100
            #    Ports: 19544/19545
        """
        print("LibreVNA Devices Summary:")
        print("=" * 25)
        
        if not self._devices:
            print("No LibreVNA devices found.")
            print("\nTroubleshooting:")
            print("- Check USB connection and drivers")
            print("- Ensure device is powered on")
            print("- Try running with administrator privileges")
            return
        
        for i, device in enumerate(self._devices, 1):
            print(f"{i}. {device['product']} ({device['transport']})")
            print(f"   Serial: {device['serial']}")
            print(f"   Manufacturer: {device['manufacturer']}")
            
            if device['transport'] == 'USB':
                print(f"   VID/PID: 0x{device['vid']:04x}/0x{device['pid']:04x}")
            elif device['transport'] == 'TCP':
                # TODO: Add TCP-specific information when implemented
                pass
            
            print()
    
    def __repr__(self) -> str:
        """String representation of device manager"""
        count = len(self._devices)
        return f"DeviceManager({count} devices)"
