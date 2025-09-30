"""
Integration tests for CLI interface

This module contains tests for the command-line interface functionality,
including command parsing, device discovery, and user interaction.
"""

import pytest
from unittest.mock import Mock, patch, MagicMock
from click.testing import CliRunner
import sys
import os

# Add the project root to the path for imports
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

from cli import cli


class TestCLI:
    """Test cases for CLI interface"""
    
    def setup_method(self):
        """Set up test fixtures before each test method"""
        self.runner = CliRunner()
    
    def test_cli_help(self):
        """Test CLI help command"""
        result = self.runner.invoke(cli, ['--help'])
        assert result.exit_code == 0
        assert "LibreVNA CLI" in result.output
        assert "Command-line interface for LibreVNA" in result.output
        assert "list-devices" in result.output
        assert "connect" in result.output
        assert "info" in result.output
    
    def test_cli_version(self):
        """Test CLI version command"""
        result = self.runner.invoke(cli, ['--version'])
        assert result.exit_code == 0
        assert "LibreVNA CLI" in result.output
        assert "version" in result.output.lower()
    
    @patch('cli.DeviceManager')
    def test_list_devices_command(self, mock_device_manager_class):
        """Test list-devices command"""
        # Mock device manager
        mock_manager = Mock()
        mock_devices = [
            {
                'transport': 'USB',
                'serial': 'TEST123',
                'manufacturer': 'LibreVNA',
                'product': 'VNA',
                'vid': 0x1209,
                'pid': 0x4121
            }
        ]
        mock_manager.list_devices.return_value = mock_devices
        mock_device_manager_class.return_value = mock_manager
        
        result = self.runner.invoke(cli, ['list-devices'])
        assert result.exit_code == 0
        assert "Found 1 LibreVNA device(s):" in result.output
        assert "LibreVNA VNA" in result.output
        assert "Serial: TEST123" in result.output
        assert "Transport: USB" in result.output
        assert "VID/PID: 0x1209/0x4121" in result.output
    
    @patch('cli.DeviceManager')
    def test_list_devices_no_devices(self, mock_device_manager_class):
        """Test list-devices command with no devices"""
        # Mock device manager with no devices
        mock_manager = Mock()
        mock_manager.list_devices.return_value = []
        mock_device_manager_class.return_value = mock_manager
        
        result = self.runner.invoke(cli, ['list-devices'])
        assert result.exit_code == 0
        assert "No LibreVNA devices found" in result.output
        assert "Troubleshooting:" in result.output
    
    @patch('cli.LibreVNA')
    def test_connect_command(self, mock_librevna_class):
        """Test connect command"""
        # Mock LibreVNA instance
        mock_vna = Mock()
        mock_vna.connect.return_value = True
        mock_vna.get_device_info.return_value = {
            'manufacturer': 'LibreVNA',
            'product': 'VNA',
            'serial': 'TEST123',
            'transport': 'USB'
        }
        mock_librevna_class.return_value = mock_vna
        
        result = self.runner.invoke(cli, ['connect'])
        assert result.exit_code == 0
        assert "Connecting to LibreVNA device via USB..." in result.output
        assert "✓ Connected to: LibreVNA VNA" in result.output
        assert "Serial: TEST123" in result.output
        assert "Transport: USB" in result.output
    
    @patch('cli.LibreVNA')
    def test_connect_command_failure(self, mock_librevna_class):
        """Test connect command with connection failure"""
        # Mock LibreVNA instance that fails to connect
        mock_vna = Mock()
        mock_vna.connect.side_effect = ConnectionError("Device not found")
        mock_librevna_class.return_value = mock_vna
        
        result = self.runner.invoke(cli, ['connect'])
        assert result.exit_code == 1
        assert "Error: Device not found" in result.output
    
    @patch('cli.LibreVNA')
    def test_info_command(self, mock_librevna_class):
        """Test info command"""
        # Mock LibreVNA instance
        mock_vna = Mock()
        mock_vna.connect.return_value = True
        mock_vna.get_device_info.return_value = {
            'manufacturer': 'LibreVNA',
            'product': 'VNA',
            'serial': 'TEST123',
            'transport': 'USB',
            'vid': 0x1209,
            'pid': 0x4121
        }
        mock_librevna_class.return_value = mock_vna
        
        result = self.runner.invoke(cli, ['info'])
        assert result.exit_code == 0
        assert "LibreVNA Device Information:" in result.output
        assert "Manufacturer: LibreVNA" in result.output
        assert "Product: VNA" in result.output
        assert "Serial: TEST123" in result.output
        assert "Transport: USB" in result.output
        assert "VID: 0x1209" in result.output
        assert "PID: 0x4121" in result.output
    
    @patch('cli.LibreVNA')
    def test_info_command_not_connected(self, mock_librevna_class):
        """Test info command when not connected"""
        # Mock LibreVNA instance that fails to connect
        mock_vna = Mock()
        mock_vna.connect.side_effect = ConnectionError("Not connected")
        mock_librevna_class.return_value = mock_vna
        
        result = self.runner.invoke(cli, ['info'])
        assert result.exit_code == 1
        assert "Error: Not connected" in result.output
    
    @patch('cli.LibreVNA')
    def test_test_command(self, mock_librevna_class):
        """Test test command"""
        # Mock LibreVNA instance
        mock_vna = Mock()
        mock_vna.connect.return_value = True
        mock_vna.get_device_info.return_value = {
            'manufacturer': 'LibreVNA',
            'product': 'VNA',
            'serial': 'TEST123'
        }
        mock_librevna_class.return_value = mock_vna
        
        result = self.runner.invoke(cli, ['test'])
        assert result.exit_code == 0
        assert "Testing LibreVNA communication..." in result.output
        assert "✓ Connection successful" in result.output
        assert "✓ Device info retrieved" in result.output
        assert "✓ Basic communication test passed" in result.output
    
    @patch('cli.LibreVNA')
    def test_test_command_failure(self, mock_librevna_class):
        """Test test command with failure"""
        # Mock LibreVNA instance that fails
        mock_vna = Mock()
        mock_vna.connect.side_effect = ConnectionError("Connection failed")
        mock_librevna_class.return_value = mock_vna
        
        result = self.runner.invoke(cli, ['test'])
        assert result.exit_code == 1
        assert "✗ Connection failed" in result.output
        assert "✗ Basic communication test failed" in result.output
    
    def test_version_command(self):
        """Test version command"""
        result = self.runner.invoke(cli, ['version'])
        assert result.exit_code == 0
        assert "LibreVNA CLI" in result.output
        assert "Version:" in result.output
        assert "Python" in result.output
    
    def test_invalid_command(self):
        """Test invalid command"""
        result = self.runner.invoke(cli, ['invalid-command'])
        assert result.exit_code != 0
        assert "No such command" in result.output or "Usage:" in result.output
    
    def test_command_with_invalid_options(self):
        """Test command with invalid options"""
        result = self.runner.invoke(cli, ['list-devices', '--invalid-option'])
        assert result.exit_code != 0
        assert "No such option" in result.output or "Usage:" in result.output





