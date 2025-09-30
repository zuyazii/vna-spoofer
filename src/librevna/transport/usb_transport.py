"""
USB transport implementation for LibreVNA communication

This module implements USB communication with LibreVNA devices using the pyusb library.
It handles device discovery, connection management, and bulk endpoint communication.

The LibreVNA device uses:
- Vendor ID: 0x1209 (current) or 0x0483 (legacy)
- Product ID: 0x4121
- Interface: 0
- Bulk endpoints: 0x01 (OUT), 0x81 (IN)

Usage:
    from . import USBTransport
    
    # Create USB transport
    transport = USBTransport()
    
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
    with USBTransport() as transport:
        transport.connect()
        # ... use transport
        # Automatically disconnected on exit
"""

import usb.core
import usb.util
from typing import Optional, List, Dict, Any
from .base_transport import BaseTransport


class USBTransport(BaseTransport):
    """
    USB transport implementation for LibreVNA devices
    
    This class handles USB communication with LibreVNA devices using the pyusb library.
    It provides device discovery, connection management, and bulk endpoint communication.
    """
    
    # LibreVNA device identifiers
    VENDOR_IDS = [0x1209, 0x0483]  # Current and legacy VID
    PRODUCT_ID = 0x4121
    
    # USB configuration
    INTERFACE = 0
    CONFIGURATION = 1
    
    def __init__(self):
        """
        Initialize USB transport
        
        Sets up the transport layer but does not connect to any device.
        Use connect() method to establish connection.
        """
        super().__init__()
        self._device: Optional[usb.core.Device] = None
        self._endpoint_out: Optional[usb.core.Endpoint] = None
        self._endpoint_in: Optional[usb.core.Endpoint] = None
        self._interface_claimed = False
    
    def discover_devices(self) -> List[Dict[str, Any]]:
        """
        Discover available LibreVNA devices
        
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
            # Find all devices with this VID/PID combination
            found_devices = usb.core.find(
                find_all=True,
                idVendor=vid,
                idProduct=self.PRODUCT_ID
            )
            
            for device in found_devices:
                try:
                    # Get device information
                    device_info = {
                        'vid': device.idVendor,
                        'pid': device.idProduct,
                        'serial': usb.util.get_string(device, device.iSerialNumber) or "Unknown",
                        'manufacturer': usb.util.get_string(device, device.iManufacturer) or "Unknown",
                        'product': usb.util.get_string(device, device.iProduct) or "Unknown",
                        'device': device  # Keep reference for connection
                    }
                    devices.append(device_info)
                    
                except (usb.core.USBError, ValueError) as e:
                    # Skip devices that can't be accessed
                    print(f"Warning: Could not access device {vid:04x}:{self.PRODUCT_ID:04x}: {e}")
                    continue
        
        return devices
    
    def connect(self, vid: Optional[int] = None, pid: Optional[int] = None, 
                serial: Optional[str] = None, **kwargs) -> bool:
        """
        Connect to LibreVNA device via USB
        
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
                self._device = usb.core.find(idVendor=vid, idProduct=pid)
            elif serial is not None:
                # Connect to device with specific serial
                devices = self.discover_devices()
                matching_devices = [d for d in devices if d['serial'] == serial]
                if not matching_devices:
                    raise ValueError(f"Device with serial '{serial}' not found")
                self._device = matching_devices[0]['device']
            else:
                # Connect to first available device
                self._device = usb.core.find(idVendor=self.VENDOR_IDS[0], idProduct=self.PRODUCT_ID)
                if self._device is None:
                    # Try legacy VID
                    self._device = usb.core.find(idVendor=self.VENDOR_IDS[1], idProduct=self.PRODUCT_ID)
            
            if self._device is None:
                raise ConnectionError("No LibreVNA device found. Please check connection and drivers.")
            
            # Set configuration
            try:
                self._device.set_configuration(self.CONFIGURATION)
            except usb.core.USBError as e:
                raise ConnectionError(f"Failed to set device configuration: {e}")
            
            # Claim interface
            try:
                usb.util.claim_interface(self._device, self.INTERFACE)
                self._interface_claimed = True
            except usb.core.USBError as e:
                raise ConnectionError(f"Failed to claim interface: {e}")
            
            # Find endpoints
            self._find_endpoints()
            
            # Get device information
            self._device_info = {
                'vid': self._device.idVendor,
                'pid': self._device.idProduct,
                'serial': usb.util.get_string(self._device, self._device.iSerialNumber) or "Unknown",
                'manufacturer': usb.util.get_string(self._device, self._device.iManufacturer) or "Unknown",
                'product': usb.util.get_string(self._device, self._device.iProduct) or "Unknown",
                'transport': 'USB'
            }
            
            self._connected = True
            return True
            
        except Exception as e:
            self.disconnect()
            raise ConnectionError(f"USB connection failed: {e}")
    
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
            # Send data via bulk OUT endpoint
            bytes_written = self._endpoint_out.write(data)
            if bytes_written != len(data):
                raise ConnectionError(f"Only {bytes_written} of {len(data)} bytes sent")
                
        except usb.core.USBError as e:
            raise ConnectionError(f"USB send failed: {e}")
    
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
            # Receive data via bulk IN endpoint
            data = self._endpoint_in.read(size)
            return bytes(data)
            
        except usb.core.USBError as e:
            raise ConnectionError(f"USB receive failed: {e}")
    
    def disconnect(self) -> None:
        """
        Disconnect from LibreVNA device and cleanup resources
        
        This method:
        - Releases the USB interface
        - Disposes of USB resources
        - Resets connection state
        - Clears device references
        """
        self._connected = False
        
        if self._device is not None and self._interface_claimed:
            try:
                usb.util.release_interface(self._device, self.INTERFACE)
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
        Check if USB transport is connected to device
        
        Returns:
            bool: True if connected, False otherwise
        """
        return self._connected and self._device is not None
    
    def __repr__(self) -> str:
        """String representation of USB transport"""
        if self._connected and self._device_info:
            return f"USBTransport(connected, {self._device_info['manufacturer']} {self._device_info['product']})"
        else:
            return "USBTransport(disconnected)"
