"""
LibUSB transport implementation for LibreVNA communication

This module implements USB communication with LibreVNA devices using the libusb-package library.
It provides better support for WinUSB drivers and improved device discovery capabilities.

The LibreVNA device uses:
- Vendor ID: 0x1209 (current) or 0x0483 (legacy)
- Product ID: 0x4121
- Interface: 0
- Bulk endpoints: 0x01 (OUT), 0x81 (IN)

Usage:
    from . import LibUSBTransport
    
    # Create LibUSB transport
    transport = LibUSBTransport()
    
    # Connect to first available device
    transport.connect()
    
    # Or connect to specific device
    transport.connect(vid=0x1209, pid=0x4121, serial="ABC123")
    
    # Send/receive data
    transport.send(b'\x01\x02\x03\x04')
    response = transport.recv(64)
    
    # Disconnect
    transport.disconnect()
    
    # Use as context manager
    with LibUSBTransport() as transport:
        transport.connect()
        # ... use transport
        # Automatically disconnected on exit
"""

import usb.core
import usb.util
import usb.backend.libusb1
import os
from typing import Optional, List, Dict, Any
from .base_transport import BaseTransport


class LibUSBTransport(BaseTransport):
    """
    LibUSB transport implementation for LibreVNA devices
    
    This class handles USB communication with LibreVNA devices using the libusb-package library.
    It provides enhanced support for WinUSB drivers and improved device discovery capabilities.
    """
    
    # LibreVNA device identifiers
    VENDOR_IDS = [0x1209, 0x0483]  # Current and legacy VID
    PRODUCT_ID = 0x4121
    
    # USB configuration
    INTERFACE = 0
    CONFIGURATION = 1
    
    def __init__(self):
        """
        Initialize LibUSB transport
        
        Sets up the transport layer but does not connect to any device.
        Use connect() method to establish connection.
        """
        super().__init__()
        self._device: Optional[usb.core.Device] = None
        self._endpoint_out: Optional[usb.core.Endpoint] = None
        self._endpoint_in: Optional[usb.core.Endpoint] = None
        self._interface_claimed = False
        self._backend = None
        self._setup_libusb_backend()
    
    def _setup_libusb_backend(self) -> None:
        """
        Set up libusb1 backend with proper PATH configuration
        
        This method ensures that the libusb-1.0 DLL is accessible by adding
        the libusb-package directory to the PATH environment variable.
        """
        try:
            # Try to get the backend first
            self._backend = usb.backend.libusb1.get_backend()
            
            if self._backend is None:
                # Backend is None, need to set up PATH
                try:
                    import libusb_package
                    dll_dir = os.path.dirname(libusb_package.__file__)
                    
                    # Add libusb-package directory to PATH
                    current_path = os.environ.get('PATH', '')
                    if dll_dir not in current_path:
                        os.environ['PATH'] = dll_dir + os.pathsep + current_path
                    
                    # Try to get backend again
                    self._backend = usb.backend.libusb1.get_backend()
                    
                except ImportError:
                    print("Warning: libusb-package not available, libusb1 backend may not work")
                    self._backend = None
                    
        except Exception as e:
            print(f"Warning: Failed to set up libusb1 backend: {e}")
            self._backend = None
    
    def discover_devices(self) -> List[Dict[str, Any]]:
        """
        Discover available LibreVNA devices using LibUSB
        
        This method provides enhanced device discovery with better support for WinUSB drivers
        and improved error handling for devices that may not be immediately accessible.
        
        Returns:
            List[Dict]: List of device information dictionaries
            Each dict contains: vid, pid, serial, manufacturer, product
            
        Example:
            devices = transport.discover_devices()
            for device in devices:
                print(f"Found device: {device['manufacturer']} {device['product']}")
                print(f"Serial: {device['serial']}")
        """
        devices = []
        
        for vid in self.VENDOR_IDS:
            try:
                # Find all devices with this VID/PID combination using libusb1 backend
                found_devices = usb.core.find(
                    find_all=True,
                    idVendor=vid,
                    idProduct=self.PRODUCT_ID,
                    backend=self._backend
                )
                
                for device in found_devices:
                    try:
                        # Get device information with enhanced error handling
                        device_info = self._get_device_info(device)
                        if device_info:
                            devices.append(device_info)
                            
                    except (usb.core.USBError, ValueError, IndexError) as e:
                        # Skip devices that can't be accessed
                        print(f"Warning: Could not access device {vid:04x}:{self.PRODUCT_ID:04x}: {e}")
                        continue
                        
            except usb.core.USBError as e:
                print(f"Warning: Error scanning for devices with VID {vid:04x}: {e}")
                continue
        
        return devices
    
    def _get_device_info(self, device: usb.core.Device) -> Optional[Dict[str, Any]]:
        """
        Extract device information from USB device
        
        Args:
            device: USB device object
            
        Returns:
            Dict: Device information dictionary or None if extraction fails
        """
        try:
            # Get device descriptors
            device_info = {
                'vid': device.idVendor,
                'pid': device.idProduct,
                'serial': self._get_string_descriptor(device, device.iSerialNumber) or "Unknown",
                'manufacturer': self._get_string_descriptor(device, device.iManufacturer) or "Unknown",
                'product': self._get_string_descriptor(device, device.iProduct) or "Unknown",
                'device': device  # Keep reference for connection
            }
            return device_info
            
        except (usb.core.USBError, ValueError, IndexError):
            return None
    
    def _get_string_descriptor(self, device: usb.core.Device, index: int) -> Optional[str]:
        """
        Safely get string descriptor from device
        
        Args:
            device: USB device object
            index: String descriptor index
            
        Returns:
            str: String descriptor content or None if not available
        """
        try:
            if index > 0:
                return usb.util.get_string(device, index)
        except (usb.core.USBError, ValueError, IndexError):
            pass
        return None
    
    def connect(self, vid: Optional[int] = None, pid: Optional[int] = None, 
                serial: Optional[str] = None, **kwargs) -> bool:
        """
        Connect to LibreVNA device via LibUSB
        
        Args:
            vid: Vendor ID (optional, uses default if not specified)
            pid: Product ID (optional, uses default if not specified)
            serial: Device serial number (optional, connects to first match if not specified)
            **kwargs: Additional connection parameters (ignored for USB)
            
        Returns:
            bool: True if connection successful, False otherwise
            
        Raises:
            ConnectionError: If connection fails
            ValueError: If device not found or invalid parameters
            
        Example:
            # Connect to first available device
            transport.connect()
            
            # Connect to specific device by serial
            transport.connect(serial="ABC123")
            
            # Connect to specific VID/PID
            transport.connect(vid=0x1209, pid=0x4121)
        """
        try:
            # Find device
            if vid is not None and pid is not None:
                # Connect to specific VID/PID
                self._device = usb.core.find(idVendor=vid, idProduct=pid, backend=self._backend)
            elif serial is not None:
                # Connect to device with specific serial
                devices = self.discover_devices()
                matching_devices = [d for d in devices if d['serial'] == serial]
                if not matching_devices:
                    raise ValueError(f"Device with serial '{serial}' not found")
                self._device = matching_devices[0]['device']
            else:
                # Connect to first available device
                self._device = usb.core.find(idVendor=self.VENDOR_IDS[0], idProduct=self.PRODUCT_ID, backend=self._backend)
                if self._device is None:
                    # Try legacy VID
                    self._device = usb.core.find(idVendor=self.VENDOR_IDS[1], idProduct=self.PRODUCT_ID, backend=self._backend)
            
            if self._device is None:
                raise ConnectionError("No LibreVNA device found. Please check connection and drivers.")
            
            # Set configuration with enhanced error handling
            try:
                self._device.set_configuration(self.CONFIGURATION)
            except usb.core.USBError as e:
                # Try to reset device and retry
                try:
                    self._device.reset()
                    self._device.set_configuration(self.CONFIGURATION)
                except usb.core.USBError:
                    raise ConnectionError(f"Failed to set device configuration: {e}")
            
            # Claim interface with enhanced error handling
            try:
                usb.util.claim_interface(self._device, self.INTERFACE)
                self._interface_claimed = True
            except usb.core.USBError as e:
                # Try to detach kernel driver if present
                try:
                    if self._device.is_kernel_driver_active(self.INTERFACE):
                        self._device.detach_kernel_driver(self.INTERFACE)
                    usb.util.claim_interface(self._device, self.INTERFACE)
                    self._interface_claimed = True
                except usb.core.USBError:
                    raise ConnectionError(f"Failed to claim interface: {e}")
            
            # Find endpoints
            self._find_endpoints()
            
            # Get device information
            self._device_info = {
                'vid': self._device.idVendor,
                'pid': self._device.idProduct,
                'serial': self._get_string_descriptor(self._device, self._device.iSerialNumber) or "Unknown",
                'manufacturer': self._get_string_descriptor(self._device, self._device.iManufacturer) or "Unknown",
                'product': self._get_string_descriptor(self._device, self._device.iProduct) or "Unknown",
                'transport': 'LibUSB'
            }
            
            self._connected = True
            return True
            
        except Exception as e:
            self.disconnect()
            raise ConnectionError(f"LibUSB connection failed: {e}")
    
    def _find_endpoints(self) -> None:
        """
        Find bulk endpoints for communication
        
        Raises:
            ConnectionError: If endpoints not found
        """
        try:
            # Get active configuration and interface
            cfg = self._device.get_active_configuration()
            intf = cfg[(self.INTERFACE, 0)]
            
            # Find OUT endpoint (host to device)
            self._endpoint_out = usb.util.find_descriptor(
                intf,
                custom_match=lambda e: usb.util.endpoint_direction(e.bEndpointAddress) == usb.util.ENDPOINT_OUT
            )
            
            # Find IN endpoint (device to host)
            self._endpoint_in = usb.util.find_descriptor(
                intf,
                custom_match=lambda e: usb.util.endpoint_direction(e.bEndpointAddress) == usb.util.ENDPOINT_IN
            )
            
            if self._endpoint_out is None or self._endpoint_in is None:
                raise ConnectionError("Required bulk endpoints not found")
                
        except usb.core.USBError as e:
            raise ConnectionError(f"Failed to find endpoints: {e}")
    
    def send(self, data: bytes) -> None:
        """
        Send data to LibreVNA device via bulk OUT endpoint
        
        Args:
            data: Raw bytes to send to device
            
        Raises:
            ConnectionError: If not connected or send fails
            ValueError: If data is invalid
            
        Example:
            # Send command to device
            transport.send(b'\x01\x02\x03\x04')
            
            # Send protocol packet
            packet = create_protocol_packet(command='GET_INFO')
            transport.send(packet)
        """
        if not self._connected or self._device is None or self._endpoint_out is None:
            raise ConnectionError("Not connected to device")
        
        if not isinstance(data, bytes):
            raise ValueError("Data must be bytes")
        
        if len(data) == 0:
            raise ValueError("Data cannot be empty")
        
        try:
            # Send data via bulk OUT endpoint with timeout
            bytes_written = self._endpoint_out.write(data, timeout=1000)
            if bytes_written != len(data):
                raise ConnectionError(f"Only {bytes_written} of {len(data)} bytes sent")
                
        except usb.core.USBError as e:
            raise ConnectionError(f"LibUSB send failed: {e}")
    
    def recv(self, size: int = 1024) -> bytes:
        """
        Receive data from LibreVNA device via bulk IN endpoint
        
        Args:
            size: Maximum number of bytes to receive
            
        Returns:
            bytes: Received data from device
            
        Raises:
            ConnectionError: If not connected or receive fails
            ValueError: If size is invalid
            
        Example:
            # Receive response from device
            response = transport.recv(64)
            
            # Receive large data block
            data = transport.recv(4096)
        """
        if not self._connected or self._device is None or self._endpoint_in is None:
            raise ConnectionError("Not connected to device")
        
        if size <= 0:
            raise ValueError("Size must be positive")
        
        try:
            # Receive data via bulk IN endpoint with timeout
            data = self._endpoint_in.read(size, timeout=1000)
            return bytes(data)
            
        except usb.core.USBError as e:
            raise ConnectionError(f"LibUSB receive failed: {e}")
    
    def disconnect(self) -> None:
        """
        Disconnect from LibreVNA device and cleanup resources
        
        This method:
        - Releases the USB interface
        - Reattaches kernel driver if it was detached
        - Disposes of USB resources
        - Resets connection state
        - Clears device references
        """
        self._connected = False
        
        if self._device is not None and self._interface_claimed:
            try:
                usb.util.release_interface(self._device, self.INTERFACE)
                # Try to reattach kernel driver if it was detached
                try:
                    if hasattr(self._device, 'attach_kernel_driver'):
                        self._device.attach_kernel_driver(self.INTERFACE)
                except (usb.core.USBError, NotImplementedError):
                    pass  # Ignore errors when reattaching driver (not supported on all platforms)
            except usb.core.USBError:
                pass  # Ignore errors during cleanup
            self._interface_claimed = False
        
        if self._device is not None:
            try:
                usb.util.dispose_resources(self._device)
            except usb.core.USBError:
                pass  # Ignore errors during cleanup
            self._device = None
        
        self._endpoint_out = None
        self._endpoint_in = None
        self._device_info = None
    
    def is_connected(self) -> bool:
        """
        Check if LibUSB transport is connected to device
        
        Returns:
            bool: True if connected, False otherwise
        """
        return self._connected and self._device is not None
    
    def __repr__(self) -> str:
        """String representation of LibUSB transport"""
        if self._connected and self._device_info:
            return f"LibUSBTransport(connected, {self._device_info['manufacturer']} {self._device_info['product']})"
        else:
            return "LibUSBTransport(disconnected)"
