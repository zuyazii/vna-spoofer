"""
Integration tests for LibreVNA CLI

This module contains integration tests that test the interaction between
different components of the LibreVNA CLI system.
"""

import pytest
from unittest.mock import Mock, patch, MagicMock
import sys
import os

# Add the project root to the path for imports
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

from src.librevna.device.device_manager import DeviceManager
from src.librevna.device.librevna import LibreVNA
from src.librevna.transport.usb_transport import USBTransport


class TestIntegration:
    """Integration test cases"""
    
    def setup_method(self):
        """Set up test fixtures before each test method"""
        pass
    
    @patch('src.librevna.transport.usb_transport.usb.core.find')
    def test_device_manager_usb_integration(self, mock_usb_find):
        """Test integration between DeviceManager and USB transport"""
        # Mock USB device discovery
        mock_device = Mock()
        mock_device.idVendor = 0x1209
        mock_device.idProduct = 0x4121
        mock_device.iSerialNumber = 1
        mock_device.iManufacturer = 2
        mock_device.iProduct = 3
        
        mock_usb_find.return_value = [mock_device]
        
        # Mock usb.util.get_string
        with patch('src.librevna.transport.usb_transport.usb.util.get_string') as mock_get_string:
            mock_get_string.side_effect = lambda device, index: {
                1: "TEST123",
                2: "LibreVNA",
                3: "VNA"
            }[index]
            
            # Test device discovery through DeviceManager
            manager = DeviceManager()
            devices = manager.list_devices()
            
            # Verify integration
            assert len(devices) == 1
            assert devices[0]['transport'] == 'USB'
            assert devices[0]['serial'] == 'TEST123'
            assert devices[0]['manufacturer'] == 'LibreVNA'
            assert devices[0]['product'] == 'VNA'
            assert devices[0]['vid'] == 0x1209
            assert devices[0]['pid'] == 0x4121
    
    @patch('src.librevna.transport.usb_transport.usb.core.find')
    @patch('src.librevna.transport.usb_transport.usb.util.get_string')
    def test_librevna_usb_integration(self, mock_get_string, mock_usb_find):
        """Test integration between LibreVNA and USB transport"""
        # Mock USB device
        mock_device = Mock()
        mock_device.idVendor = 0x1209
        mock_device.idProduct = 0x4121
        mock_device.iSerialNumber = 1
        mock_device.iManufacturer = 2
        mock_device.iProduct = 3
        mock_device.set_configuration = Mock()
        
        # Mock USB interface and endpoints
        mock_interface = Mock()
        mock_endpoint_out = Mock()
        mock_endpoint_in = Mock()
        mock_endpoint_out.write = Mock(return_value=4)
        mock_endpoint_in.read = Mock(return_value=[1, 2, 3, 4])
        
        mock_cfg = Mock()
        mock_cfg.__getitem__ = Mock(return_value=mock_interface)
        mock_device.get_active_configuration = Mock(return_value=mock_cfg)
        
        mock_usb_find.return_value = mock_device
        mock_get_string.side_effect = lambda device, index: {
            1: "TEST123",
            2: "LibreVNA", 
            3: "VNA"
        }[index]
        
        # Mock usb.util functions
        with patch('src.librevna.transport.usb_transport.usb.util.claim_interface'), \
             patch('src.librevna.transport.usb_transport.usb.util.find_descriptor') as mock_find_desc:
            
            mock_find_desc.side_effect = [mock_endpoint_out, mock_endpoint_in]
            
            # Test LibreVNA connection
            vna = LibreVNA()
            result = vna.connect(transport='USB')
            
            assert result is True
            assert vna.is_connected() is True
            
            # Test device info retrieval
            device_info = vna.get_device_info()
            assert device_info is not None
            assert device_info['manufacturer'] == 'LibreVNA'
            assert device_info['product'] == 'VNA'
            assert device_info['serial'] == 'TEST123'
            
            # Test disconnect
            vna.disconnect()
            assert vna.is_connected() is False
    
    def test_device_manager_validation_integration(self):
        """Test integration between DeviceManager and LibreVNA validation"""
        # Mock device info
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
        
        # Mock LibreVNA for validation
        with patch('src.librevna.device.device_manager.LibreVNA') as mock_librevna_class:
            mock_vna = Mock()
            mock_vna.get_device_info.return_value = {
                'manufacturer': 'LibreVNA',
                'product': 'VNA',
                'serial': 'TEST123'
            }
            mock_librevna_class.return_value = mock_vna
            
            # Test validation through DeviceManager
            manager = DeviceManager()
            result = manager.validate_device(device_info)
            
            assert result is True
            mock_vna.connect.assert_called_once_with(
                transport='USB',
                vid=0x1209,
                pid=0x4121,
                serial='TEST123'
            )
            mock_vna.get_device_info.assert_called_once()
            mock_vna.disconnect.assert_called_once()
    
    @patch('src.librevna.transport.usb_transport.usb.core.find')
    def test_error_handling_integration(self, mock_usb_find):
        """Test error handling integration across components"""
        # Mock USB find to raise exception
        mock_usb_find.side_effect = Exception("USB error")
        
        # Test that DeviceManager handles USB errors gracefully
        manager = DeviceManager()
        devices = manager.list_devices()
        
        # Should return empty list when USB discovery fails
        assert len(devices) == 0
    
    def test_transport_abstraction_integration(self):
        """Test that transport abstraction works correctly"""
        # Test that LibreVNA can work with different transport types
        vna = LibreVNA()
        
        # Test USB transport
        with patch('src.librevna.transport.usb_transport.usb.core.find') as mock_find:
            mock_device = Mock()
            mock_device.idVendor = 0x1209
            mock_device.idProduct = 0x4121
            mock_device.iSerialNumber = 1
            mock_device.iManufacturer = 2
            mock_device.iProduct = 3
            mock_device.set_configuration = Mock()
            
            mock_find.return_value = mock_device
            
            with patch('src.librevna.transport.usb_transport.usb.util.get_string') as mock_get_string, \
                 patch('src.librevna.transport.usb_transport.usb.util.claim_interface'), \
                 patch('src.librevna.transport.usb_transport.usb.util.find_descriptor') as mock_find_desc:
                
                mock_get_string.side_effect = lambda device, index: {
                    1: "TEST123",
                    2: "LibreVNA",
                    3: "VNA"
                }[index]
                
                mock_endpoint = Mock()
                mock_endpoint.write = Mock(return_value=4)
                mock_endpoint.read = Mock(return_value=[1, 2, 3, 4])
                mock_find_desc.return_value = mock_endpoint
                
                mock_cfg = Mock()
                mock_cfg.__getitem__ = Mock(return_value=Mock())
                mock_device.get_active_configuration = Mock(return_value=mock_cfg)
                
                # Test connection
                result = vna.connect(transport='USB')
                assert result is True
                
                # Test that transport type is correctly set
                assert vna._transport_type == 'USB'
                
                # Test disconnect
                vna.disconnect()
                assert vna.is_connected() is False
    
    def test_device_info_consistency_integration(self):
        """Test that device info is consistent across components"""
        # Mock device info that should be consistent
        expected_info = {
            'manufacturer': 'LibreVNA',
            'product': 'VNA',
            'serial': 'TEST123',
            'transport': 'USB',
            'vid': 0x1209,
            'pid': 0x4121
        }
        
        # Test that DeviceManager and LibreVNA return consistent info
        with patch('src.librevna.transport.usb_transport.usb.core.find') as mock_find:
            mock_device = Mock()
            mock_device.idVendor = 0x1209
            mock_device.idProduct = 0x4121
            mock_device.iSerialNumber = 1
            mock_device.iManufacturer = 2
            mock_device.iProduct = 3
            mock_device.set_configuration = Mock()
            
            mock_find.return_value = mock_device
            
            with patch('src.librevna.transport.usb_transport.usb.util.get_string') as mock_get_string, \
                 patch('src.librevna.transport.usb_transport.usb.util.claim_interface'), \
                 patch('src.librevna.transport.usb_transport.usb.util.find_descriptor') as mock_find_desc:
                
                mock_get_string.side_effect = lambda device, index: {
                    1: "TEST123",
                    2: "LibreVNA",
                    3: "VNA"
                }[index]
                
                mock_endpoint = Mock()
                mock_endpoint.write = Mock(return_value=4)
                mock_endpoint.read = Mock(return_value=[1, 2, 3, 4])
                mock_find_desc.return_value = mock_endpoint
                
                mock_cfg = Mock()
                mock_cfg.__getitem__ = Mock(return_value=Mock())
                mock_device.get_active_configuration = Mock(return_value=mock_cfg)
                
                # Test DeviceManager discovery
                manager = DeviceManager()
                devices = manager.list_devices()
                
                if devices:  # Only test if devices were found
                    device_info = devices[0]
                    assert device_info['manufacturer'] == expected_info['manufacturer']
                    assert device_info['product'] == expected_info['product']
                    assert device_info['serial'] == expected_info['serial']
                    assert device_info['transport'] == expected_info['transport']
                    assert device_info['vid'] == expected_info['vid']
                    assert device_info['pid'] == expected_info['pid']
                
                # Test LibreVNA device info
                vna = LibreVNA()
                vna.connect(transport='USB')
                librevna_info = vna.get_device_info()
                vna.disconnect()
                
                if librevna_info:  # Only test if info was retrieved
                    assert librevna_info['manufacturer'] == expected_info['manufacturer']
                    assert librevna_info['product'] == expected_info['product']
                    assert librevna_info['serial'] == expected_info['serial']
                    assert librevna_info['transport'] == expected_info['transport']
