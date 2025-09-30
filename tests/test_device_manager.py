"""
Unit tests for DeviceManager class

This module contains comprehensive unit tests for the DeviceManager class,
testing device discovery, management, and validation functionality.
"""

import pytest
from unittest.mock import Mock, patch, MagicMock
from src.librevna.device.device_manager import DeviceManager


class TestDeviceManager:
    """Test cases for DeviceManager class"""
    
    def setup_method(self):
        """Set up test fixtures before each test method"""
        self.manager = DeviceManager()
    
    def test_init(self):
        """Test DeviceManager initialization"""
        manager = DeviceManager()
        assert manager is not None
        assert hasattr(manager, '_devices')
        assert isinstance(manager._devices, list)
    
    @patch('src.librevna.device.device_manager.USBTransport')
    def test_refresh_devices_with_usb_devices(self, mock_usb_transport):
        """Test device refresh with USB devices available"""
        # Mock USB transport and devices
        mock_transport = Mock()
        mock_devices = [
            {
                'vid': 0x1209,
                'pid': 0x4121,
                'serial': 'TEST123',
                'manufacturer': 'LibreVNA',
                'product': 'VNA'
            }
        ]
        mock_transport.discover_devices.return_value = mock_devices
        mock_usb_transport.return_value = mock_transport
        
        # Create new manager to trigger refresh
        manager = DeviceManager()
        
        # Verify devices were discovered
        devices = manager.list_devices()
        assert len(devices) == 1
        assert devices[0]['transport'] == 'USB'
        assert devices[0]['serial'] == 'TEST123'
        assert devices[0]['manufacturer'] == 'LibreVNA'
        assert devices[0]['product'] == 'VNA'
    
    @patch('src.librevna.device.device_manager.USBTransport')
    def test_refresh_devices_no_usb_devices(self, mock_usb_transport):
        """Test device refresh with no USB devices available"""
        # Mock USB transport with no devices
        mock_transport = Mock()
        mock_transport.discover_devices.return_value = []
        mock_usb_transport.return_value = mock_transport
        
        # Create new manager to trigger refresh
        manager = DeviceManager()
        
        # Verify no devices found
        devices = manager.list_devices()
        assert len(devices) == 0
    
    @patch('src.librevna.device.device_manager.USBTransport')
    def test_refresh_devices_usb_import_error(self, mock_usb_transport):
        """Test device refresh when USB transport import fails"""
        # Mock import error
        mock_usb_transport.side_effect = ImportError("USB transport not available")
        
        # Create new manager to trigger refresh
        manager = DeviceManager()
        
        # Verify no devices found due to import error
        devices = manager.list_devices()
        assert len(devices) == 0
    
    def test_list_devices_no_refresh(self):
        """Test listing devices without refresh"""
        # Set up some mock devices
        self.manager._devices = [
            {
                'transport': 'USB',
                'serial': 'TEST123',
                'manufacturer': 'LibreVNA',
                'product': 'VNA',
                'vid': 0x1209,
                'pid': 0x4121
            }
        ]
        
        devices = self.manager.list_devices(refresh=False)
        assert len(devices) == 1
        assert devices[0]['serial'] == 'TEST123'
    
    @patch('src.librevna.device.device_manager.USBTransport')
    def test_list_devices_with_refresh(self, mock_usb_transport):
        """Test listing devices with refresh"""
        # Mock USB transport
        mock_transport = Mock()
        mock_transport.discover_devices.return_value = [
            {
                'vid': 0x1209,
                'pid': 0x4121,
                'serial': 'REFRESH123',
                'manufacturer': 'LibreVNA',
                'product': 'VNA'
            }
        ]
        mock_usb_transport.return_value = mock_transport
        
        # Set initial devices
        self.manager._devices = [
            {
                'transport': 'USB',
                'serial': 'OLD123',
                'manufacturer': 'LibreVNA',
                'product': 'VNA',
                'vid': 0x1209,
                'pid': 0x4121
            }
        ]
        
        # List with refresh
        devices = self.manager.list_devices(refresh=True)
        assert len(devices) == 1
        assert devices[0]['serial'] == 'REFRESH123'
    
    def test_get_device_by_serial_found(self):
        """Test getting device by serial number when device exists"""
        # Set up mock devices
        self.manager._devices = [
            {
                'transport': 'USB',
                'serial': 'TEST123',
                'manufacturer': 'LibreVNA',
                'product': 'VNA',
                'vid': 0x1209,
                'pid': 0x4121
            },
            {
                'transport': 'USB',
                'serial': 'TEST456',
                'manufacturer': 'LibreVNA',
                'product': 'VNA',
                'vid': 0x1209,
                'pid': 0x4121
            }
        ]
        
        device = self.manager.get_device_by_serial('TEST123')
        assert device is not None
        assert device['serial'] == 'TEST123'
        assert device['manufacturer'] == 'LibreVNA'
    
    def test_get_device_by_serial_not_found(self):
        """Test getting device by serial number when device doesn't exist"""
        # Set up mock devices
        self.manager._devices = [
            {
                'transport': 'USB',
                'serial': 'TEST123',
                'manufacturer': 'LibreVNA',
                'product': 'VNA',
                'vid': 0x1209,
                'pid': 0x4121
            }
        ]
        
        device = self.manager.get_device_by_serial('NOTFOUND')
        assert device is None
    
    def test_get_device_by_transport(self):
        """Test getting devices by transport type"""
        # Set up mock devices with different transports
        self.manager._devices = [
            {
                'transport': 'USB',
                'serial': 'USB123',
                'manufacturer': 'LibreVNA',
                'product': 'VNA',
                'vid': 0x1209,
                'pid': 0x4121
            },
            {
                'transport': 'TCP',
                'serial': 'TCP123',
                'manufacturer': 'LibreVNA',
                'product': 'VNA',
                'host': '192.168.1.100',
                'port': 19544
            },
            {
                'transport': 'USB',
                'serial': 'USB456',
                'manufacturer': 'LibreVNA',
                'product': 'VNA',
                'vid': 0x1209,
                'pid': 0x4121
            }
        ]
        
        # Test USB devices
        usb_devices = self.manager.get_device_by_transport('USB')
        assert len(usb_devices) == 2
        assert all(device['transport'] == 'USB' for device in usb_devices)
        
        # Test TCP devices
        tcp_devices = self.manager.get_device_by_transport('TCP')
        assert len(tcp_devices) == 1
        assert tcp_devices[0]['transport'] == 'TCP'
        assert tcp_devices[0]['serial'] == 'TCP123'
        
        # Test non-existent transport
        serial_devices = self.manager.get_device_by_transport('SERIAL')
        assert len(serial_devices) == 0
    
    @patch('src.librevna.device.device_manager.LibreVNA')
    def test_validate_device_success(self, mock_librevna_class):
        """Test successful device validation"""
        # Mock LibreVNA instance
        mock_vna = Mock()
        mock_vna.get_device_info.return_value = {
            'manufacturer': 'LibreVNA',
            'product': 'VNA',
            'serial': 'TEST123'
        }
        mock_librevna_class.return_value = mock_vna
        
        # Set up device info
        device_info = {
            'transport': 'USB',
            'serial': 'TEST123',
            'manufacturer': 'LibreVNA',
            'product': 'VNA',
            'connection_params': {
                'vid': 0x1209,
                'pid': 0x4121,
                'serial': 'TEST123'
            }
        }
        
        # Test validation
        result = self.manager.validate_device(device_info)
        assert result is True
        
        # Verify LibreVNA was used correctly
        mock_vna.connect.assert_called_once_with(
            transport='USB',
            vid=0x1209,
            pid=0x4121,
            serial='TEST123'
        )
        mock_vna.get_device_info.assert_called_once()
        mock_vna.disconnect.assert_called_once()
    
    @patch('src.librevna.device.device_manager.LibreVNA')
    def test_validate_device_failure(self, mock_librevna_class):
        """Test device validation failure"""
        # Mock LibreVNA instance that raises exception
        mock_vna = Mock()
        mock_vna.connect.side_effect = ConnectionError("Connection failed")
        mock_librevna_class.return_value = mock_vna
        
        # Set up device info
        device_info = {
            'transport': 'USB',
            'serial': 'TEST123',
            'manufacturer': 'LibreVNA',
            'product': 'VNA',
            'connection_params': {
                'vid': 0x1209,
                'pid': 0x4121,
                'serial': 'TEST123'
            }
        }
        
        # Test validation
        result = self.manager.validate_device(device_info)
        assert result is False
    
    def test_validate_device_unsupported_transport(self):
        """Test device validation with unsupported transport"""
        device_info = {
            'transport': 'SERIAL',
            'serial': 'TEST123',
            'manufacturer': 'LibreVNA',
            'product': 'VNA'
        }
        
        result = self.manager.validate_device(device_info)
        assert result is False
    
    def test_get_device_count(self):
        """Test getting device count"""
        # Test with no devices
        assert self.manager.get_device_count() == 0
        
        # Add some devices
        self.manager._devices = [
            {'transport': 'USB', 'serial': 'TEST123'},
            {'transport': 'USB', 'serial': 'TEST456'},
            {'transport': 'TCP', 'serial': 'TCP123'}
        ]
        
        assert self.manager.get_device_count() == 3
    
    def test_get_transport_counts(self):
        """Test getting transport counts"""
        # Test with no devices
        counts = self.manager.get_transport_counts()
        assert counts == {}
        
        # Add devices with different transports
        self.manager._devices = [
            {'transport': 'USB', 'serial': 'USB123'},
            {'transport': 'USB', 'serial': 'USB456'},
            {'transport': 'TCP', 'serial': 'TCP123'},
            {'transport': 'USB', 'serial': 'USB789'}
        ]
        
        counts = self.manager.get_transport_counts()
        assert counts['USB'] == 3
        assert counts['TCP'] == 1
    
    def test_print_device_summary_no_devices(self, capsys):
        """Test printing device summary with no devices"""
        self.manager.print_device_summary()
        captured = capsys.readouterr()
        
        assert "No LibreVNA devices found." in captured.out
        assert "Troubleshooting:" in captured.out
        assert "Check USB connection and drivers" in captured.out
    
    def test_print_device_summary_with_devices(self, capsys):
        """Test printing device summary with devices"""
        # Set up mock devices
        self.manager._devices = [
            {
                'transport': 'USB',
                'serial': 'TEST123',
                'manufacturer': 'LibreVNA',
                'product': 'VNA',
                'vid': 0x1209,
                'pid': 0x4121
            },
            {
                'transport': 'USB',
                'serial': 'TEST456',
                'manufacturer': 'LibreVNA',
                'product': 'VNA',
                'vid': 0x1209,
                'pid': 0x4121
            }
        ]
        
        self.manager.print_device_summary()
        captured = capsys.readouterr()
        
        assert "LibreVNA Devices Summary:" in captured.out
        assert "1. VNA (USB)" in captured.out
        assert "2. VNA (USB)" in captured.out
        assert "Serial: TEST123" in captured.out
        assert "Serial: TEST456" in captured.out
        assert "VID/PID: 0x1209/0x4121" in captured.out
    
    def test_repr(self):
        """Test string representation"""
        # Test with no devices
        assert repr(self.manager) == "DeviceManager(0 devices)"
        
        # Test with devices
        self.manager._devices = [
            {'transport': 'USB', 'serial': 'TEST123'},
            {'transport': 'USB', 'serial': 'TEST456'}
        ]
        
        assert repr(self.manager) == "DeviceManager(2 devices)"
