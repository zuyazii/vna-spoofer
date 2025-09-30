"""
Pytest configuration and fixtures for LibreVNA CLI tests

This module provides shared fixtures and configuration for all tests
in the LibreVNA CLI test suite.
"""

import pytest
import sys
import os
from unittest.mock import Mock, MagicMock

# Add the project root to the path for imports
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'src'))


@pytest.fixture
def mock_usb_device():
    """Mock USB device for testing"""
    device = Mock()
    device.idVendor = 0x1209
    device.idProduct = 0x4121
    device.iSerialNumber = 1
    device.iManufacturer = 2
    device.iProduct = 3
    device.set_configuration = Mock()
    device.get_active_configuration = Mock()
    return device


@pytest.fixture
def mock_usb_endpoints():
    """Mock USB endpoints for testing"""
    endpoint_out = Mock()
    endpoint_out.write = Mock(return_value=4)
    
    endpoint_in = Mock()
    endpoint_in.read = Mock(return_value=[1, 2, 3, 4])
    
    return endpoint_out, endpoint_in


@pytest.fixture
def mock_device_info():
    """Mock device information for testing"""
    return {
        'transport': 'USB',
        'serial': 'TEST123',
        'manufacturer': 'LibreVNA',
        'product': 'VNA',
        'vid': 0x1209,
        'pid': 0x4121,
        'connection_params': {
            'vid': 0x1209,
            'pid': 0x4121,
            'serial': 'TEST123'
        }
    }


@pytest.fixture
def mock_librevna_info():
    """Mock LibreVNA device info response"""
    return {
        'manufacturer': 'LibreVNA',
        'product': 'VNA',
        'serial': 'TEST123',
        'transport': 'USB',
        'vid': 0x1209,
        'pid': 0x4121
    }


@pytest.fixture(autouse=True)
def setup_test_environment():
    """Set up test environment before each test"""
    # Ensure clean environment
    import src.librevna.device.device_manager
    import src.librevna.transport.usb_transport
    
    # Reset any global state if needed
    yield
    
    # Cleanup after test
    pass


def pytest_configure(config):
    """Configure pytest with custom markers"""
    config.addinivalue_line(
        "markers", "unit: Unit tests that test individual components in isolation"
    )
    config.addinivalue_line(
        "markers", "integration: Integration tests that test component interactions"
    )
    config.addinivalue_line(
        "markers", "device: Tests that require actual LibreVNA hardware"
    )
    config.addinivalue_line(
        "markers", "cli: Tests for command-line interface functionality"
    )
    config.addinivalue_line(
        "markers", "slow: Tests that take a long time to run"
    )
    config.addinivalue_line(
        "markers", "network: Tests that require network connectivity"
    )


def pytest_collection_modifyitems(config, items):
    """Modify test collection to add markers based on test names"""
    for item in items:
        # Add markers based on test file names
        if "test_device_manager" in item.nodeid:
            item.add_marker(pytest.mark.unit)
        elif "test_cli" in item.nodeid:
            item.add_marker(pytest.mark.cli)
        elif "test_integration" in item.nodeid:
            item.add_marker(pytest.mark.integration)
        
        # Add markers based on test function names
        if "test_device_" in item.name or "test_hardware_" in item.name:
            item.add_marker(pytest.mark.device)
        
        if "test_slow_" in item.name:
            item.add_marker(pytest.mark.slow)
        
        if "test_network_" in item.name:
            item.add_marker(pytest.mark.network)
