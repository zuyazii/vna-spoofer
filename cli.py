#!/usr/bin/env python3
"""
LibreVNA CLI - Command-line interface for LibreVNA Vector Network Analyzer

This script provides a command-line interface for communicating with LibreVNA devices.
It supports device discovery, connection, and basic VNA operations.

Usage:
    python cli.py --help                    # Show help
    python cli.py list                      # List available devices
    python cli.py connect                   # Connect to first available device
    python cli.py connect --serial ABC123   # Connect to specific device
    python cli.py info                      # Get device information
    python cli.py test                      # Test basic communication
    python cli.py s11 test --start-freq 0.001 --stop-freq 3.0  # Run S11 test

    Examples:
        # List all available devices
        python cli.py list
        
        # Connect and get device info
        python cli.py connect
        python cli.py info
        
        # Test communication
        python cli.py test
        
        # Run S11 test (1 MHz to 3 GHz)
        python cli.py s11 test --start-freq 0.001 --stop-freq 3.0
"""

import click
import sys
import json
import yaml
import os
from typing import List, Optional
from pathlib import Path
from src.librevna.device import DeviceManager, HeadlessLibreVNA, LibreVNA
from src.librevna.measure import S11Test, S11TestConfig
from src.librevna.config import ConfigManager, ConfigFormat
from src.librevna.output import JSONGenerator, CSVGenerator, TouchstoneGenerator, LogGenerator, OutputConfig
from src.librevna.output.plotter import VNAPlotter
from src.librevna.utils import setup_logging, LogConfig


class FrequencyParamType(click.ParamType):
    """Custom parameter type for frequency input in GHz (decimal format)"""
    name = "frequency"
    
    def convert(self, value, param, ctx):
        if isinstance(value, (int, float)):
            # Convert GHz to Hz
            return float(value) * 1e9
        try:
            # Parse string input and convert GHz to Hz
            return float(value) * 1e9
        except (ValueError, TypeError):
            self.fail(f"'{value}' is not a valid frequency. Use decimal format (e.g., 1.5 for 1.5 GHz)", param, ctx)


@click.group()
@click.version_option(version="0.1.0", prog_name="LibreVNA CLI")
@click.option('--quiet', '-q', is_flag=True, help='Suppress console output, logs only to files')
@click.option('--log-level', type=click.Choice(['DEBUG', 'INFO', 'WARNING', 'ERROR', 'CRITICAL']), 
              default='INFO', help='Set logging level')
@click.pass_context
def cli(ctx, quiet, log_level):
    """
    LibreVNA CLI - Command-line interface for LibreVNA Vector Network Analyzer
    
    This tool provides a command-line interface for communicating with LibreVNA devices.
    Use the commands below to discover, connect to, and interact with your LibreVNA.
    """
    # Store global options in context
    ctx.ensure_object(dict)
    ctx.obj['quiet'] = quiet
    ctx.obj['log_level'] = log_level
    
    # Setup global logging configuration
    log_config = LogConfig(
        level=log_level,
        console_output=not quiet,
        file_output=True,
        log_directory="logs"
    )
    setup_logging(log_config)


@cli.command()
def list_devices():
    """
    List all available LibreVNA devices
    
    This command scans for available LibreVNA devices and displays information
    about each device including manufacturer, product name, serial number,
    and transport method (USB/TCP).
    
    Example:
        python cli.py list
    """
    try:
        manager = DeviceManager()
        devices = manager.list_devices()
        
        if not devices:
            click.echo("No LibreVNA devices found.")
            click.echo("\nTroubleshooting:")
            click.echo("- Check USB connection and drivers")
            click.echo("- Ensure device is powered on")
            click.echo("- Try running with administrator privileges")
            return
        
        click.echo(f"Found {len(devices)} LibreVNA device(s):")
        click.echo("=" * 40)
        
        for i, device in enumerate(devices, 1):
            click.echo(f"{i}. {device['manufacturer']} {device['product']}")
            click.echo(f"   Serial: {device['serial']}")
            click.echo(f"   Transport: {device['transport']}")
            if device['transport'] == 'USB':
                click.echo(f"   VID/PID: 0x{device['vid']:04x}/0x{device['pid']:04x}")
            click.echo()
            
    except Exception as e:
        click.echo(f"Error listing devices: {e}", err=True)
        sys.exit(1)


@cli.command()
@click.option('--serial', '-s', help='Device serial number')
@click.option('--vid', type=int, help='USB Vendor ID (hex)')
@click.option('--pid', type=int, help='USB Product ID (hex)')
@click.option('--transport', '-t', type=click.Choice(['USB', 'TCP']), default='USB', help='Transport method')
def connect(serial: Optional[str], vid: Optional[int], pid: Optional[int], transport: str):
    """
    Connect to a LibreVNA device
    
    This command establishes a connection to a LibreVNA device. By default,
    it connects to the first available USB device. You can specify a particular
    device using the serial number or VID/PID combination.
    
    Examples:
        python cli.py connect                    # Connect to first device
        python cli.py connect --serial ABC123   # Connect to specific device
        python cli.py connect --vid 0x1209 --pid 0x4121  # Connect by VID/PID
    """
    try:
        vna = LibreVNA()
        
        # Prepare connection parameters
        connect_params = {}
        if serial:
            connect_params['serial'] = serial
        if vid is not None:
            connect_params['vid'] = vid
        if pid is not None:
            connect_params['pid'] = pid
        
        click.echo(f"Connecting to LibreVNA device via {transport}...")
        
        # Connect to device
        vna.connect(transport=transport, **connect_params)
        
        if vna.is_connected():
            info = vna.get_device_info()
            click.echo(f"[OK] Connected to: {info['manufacturer']} {info['product']}")
            click.echo(f"  Serial: {info['serial']}")
            click.echo(f"  Transport: {info['transport']}")
        else:
            click.echo("[ERROR] Failed to connect to device", err=True)
            sys.exit(1)
            
    except Exception as e:
        click.echo(f"Connection failed: {e}", err=True)
        sys.exit(1)


@cli.command()
@click.option('--serial', '-s', help='Device serial number')
def info(serial: Optional[str]):
    """
    Get device information
    
    This command retrieves and displays detailed information about the connected
    LibreVNA device including manufacturer, product name, serial number, and
    other device-specific information.
    
    Examples:
        python cli.py info                    # Get info from first device
        python cli.py info --serial ABC123   # Get info from specific device
    """
    try:
        vna = LibreVNA()
        
        # Connect to device
        connect_params = {}
        if serial:
            connect_params['serial'] = serial
        
        vna.connect(**connect_params)
        
        if not vna.is_connected():
            click.echo("Not connected to device", err=True)
            sys.exit(1)
        
        # Get device information
        info = vna.get_device_info()
        
        click.echo("LibreVNA Device Information:")
        click.echo("=" * 30)
        click.echo(f"Manufacturer: {info['manufacturer']}")
        click.echo(f"Product: {info['product']}")
        click.echo(f"Serial: {info['serial']}")
        click.echo(f"Transport: {info['transport']}")
        
        if info['transport'] == 'USB':
            click.echo(f"VID: 0x{info['vid']:04x}")
            click.echo(f"PID: 0x{info['pid']:04x}")
        
        vna.disconnect()
        
    except Exception as e:
        click.echo(f"Error getting device info: {e}", err=True)
        sys.exit(1)


@cli.command()
@click.option('--serial', '-s', help='Device serial number')
def test(serial: Optional[str]):
    """
    Test basic communication with device
    
    This command performs basic communication tests with the LibreVNA device
    to verify that the connection is working properly. It sends test commands
    and verifies responses.
    
    Examples:
        python cli.py test                    # Test first device
        python cli.py test --serial ABC123   # Test specific device
    """
    try:
        vna = LibreVNA()
        
        # Connect to device
        connect_params = {}
        if serial:
            connect_params['serial'] = serial
        
        click.echo("Connecting to device...")
        vna.connect(**connect_params)
        
        if not vna.is_connected():
            click.echo("Failed to connect to device", err=True)
            sys.exit(1)
        
        click.echo("[OK] Connected to device")
        
        # Test basic communication
        click.echo("Testing basic communication...")
        
        try:
            # Send a simple test command (this will be updated with actual protocol)
            test_command = b'\x01\x00\x00\x00'  # Placeholder command
            vna.send_command(test_command)
            click.echo("[OK] Command sent successfully")
            
            # Try to receive response (with timeout)
            response = vna.receive_response(64)
            click.echo(f"[OK] Received response: {len(response)} bytes")
            click.echo(f"  Data: {response.hex()}")
            
        except Exception as e:
            click.echo(f"[ERROR] Communication test failed: {e}")
            sys.exit(1)
        
        click.echo("[OK] All tests passed!")
        vna.disconnect()
        
    except Exception as e:
        click.echo(f"Test failed: {e}", err=True)
        sys.exit(1)


@cli.command()
def version():
    """
    Show version information
    
    This command displays version information for the LibreVNA CLI tool
    and related components.
    """
    try:
        import librevna
        click.echo(f"LibreVNA CLI Version: 0.1.0")
        click.echo(f"LibreVNA Library Version: {librevna.__version__}")
        
        # Show Python version
        import sys
        click.echo(f"Python Version: {sys.version}")
        
        # Show available transports
        click.echo("\nAvailable Transports:")
        try:
            from src.librevna.transport import USBTransport
            click.echo("  ✓ USB Transport (pyusb)")
        except ImportError:
            click.echo("  ✗ USB Transport (pyusb not available)")
        
        try:
            from src.librevna.transport import TCPTransport
            click.echo("  ✓ TCP Transport")
        except ImportError:
            click.echo("  ✗ TCP Transport (not implemented)")
        
    except Exception as e:
        click.echo(f"Error getting version info: {e}", err=True)
        sys.exit(1)


# S11 Test Commands
@cli.group()
def s11():
    """S11 testing and automation commands"""
    pass


@s11.command()
@click.option('--start-freq', '-s', type=FrequencyParamType(), default=0.001, help='Start frequency in GHz (decimal format, e.g., 1.5 for 1.5 GHz)')
@click.option('--stop-freq', '-e', type=FrequencyParamType(), default=6.0, help='Stop frequency in GHz (decimal format, e.g., 3.0 for 3 GHz)')
@click.option('--points', '-p', type=int, default=201, help='Number of measurement points')
@click.option('--pass-threshold', type=float, default=-10.0, help='Pass threshold in dB')
@click.option('--fail-threshold', type=float, default=-5.0, help='Fail threshold in dB')
@click.option('--calibration', '-c', type=click.Path(exists=True), help='Calibration file path')
@click.option('--output-dir', '-o', type=click.Path(), default='output', help='Output directory')
@click.option('-f', '--formats', multiple=True, default=['json', 'csv'], help='Output formats (json, csv, touchstone, log, plot, all). Use multiple times for multiple formats. Use "all" to include all formats.')
@click.option('--config', type=click.Path(exists=True), help='Configuration file')
@click.option('--verbose', '-v', is_flag=True, help='Verbose output (overrides global log level)')
@click.pass_context
def test(ctx, start_freq, stop_freq, points, pass_threshold, fail_threshold, 
         calibration, output_dir, formats, config, verbose):
    """
    Run S11 test with specified parameters
    
    Examples:
        # Basic S11 test (1 MHz to 3 GHz)
        python cli.py s11 test --start-freq 0.001 --stop-freq 3.0
        
        # S11 test with calibration
        python cli.py s11 test -c calibration.cal --pass-threshold -15
        
        # S11 test with custom output
        python cli.py s11 test -o results --formats json --formats csv --formats touchstone
        
        # S11 test with all output formats
        python cli.py s11 test --formats all
    """
    # Use global logging configuration, override level if verbose is specified
    if verbose:
        # Override global log level for verbose output
        log_config = LogConfig(
            level="DEBUG",
            console_output=not ctx.obj['quiet'],
            file_output=True,
            log_directory="logs"
        )
        setup_logging(log_config)
    
    try:
        # Load configuration if provided
        if config:
            with open(config, 'r') as f:
                if config.endswith('.yaml') or config.endswith('.yml'):
                    config_data = yaml.safe_load(f)
                else:
                    config_data = json.load(f)
        else:
            config_data = {}
        
        # Handle "all" format option
        available_formats = ['json', 'csv', 'touchstone', 'log', 'plot']
        if 'all' in formats:
            # Replace 'all' with all available formats
            formats = [f for f in formats if f != 'all'] + available_formats
            # Remove duplicates while preserving order
            formats = list(dict.fromkeys(formats))
        
        # Create test configuration
        test_config = S11TestConfig(
            start_frequency=config_data.get('frequency', {}).get('start', start_freq),
            stop_frequency=config_data.get('frequency', {}).get('stop', stop_freq),
            point_count=config_data.get('frequency', {}).get('points', points),
            pass_threshold_db=config_data.get('thresholds', {}).get('pass_db', pass_threshold),
            fail_threshold_db=config_data.get('thresholds', {}).get('fail_db', fail_threshold),
            calibration_file=config_data.get('calibration', {}).get('file', calibration),
            output_format=list(formats)
        )
        
        # Create output configuration with shared timestamp for grouping related files
        output_config = OutputConfig.create_with_shared_timestamp(
            output_directory=output_dir,
            include_raw_data=True,
            include_corrected_data=True
        )
        
        # Connect to device and initialize S11 test with device manager
        vna = LibreVNA()
        try:
            # Connect to device
            vna.connect()
            if not vna.is_connected():
                click.echo("Error: Could not connect to device", err=True)
                sys.exit(1)
            else:
                device_manager = vna
                click.echo(f"Connected to device: {vna.get_device_info()['product']}")
        except Exception as e:
            click.echo(f"Error: Device connection failed ({e})", err=True)
            sys.exit(1)
        
        # Initialize S11 test with device manager
        s11_test = S11Test(device_manager=device_manager)
        
        # Reset warning counters before test
        from src.librevna.calibration.corrector import reset_warning_counts
        reset_warning_counts()
        
        # Run test
        if not ctx.obj['quiet']:
            # Display frequency range in appropriate units
            if test_config.stop_frequency >= 1e9:
                start_display = test_config.start_frequency/1e9
                stop_display = test_config.stop_frequency/1e9
                unit = "GHz"
            else:
                start_display = test_config.start_frequency/1e6
                stop_display = test_config.stop_frequency/1e6
                unit = "MHz"
            click.echo(f"Running S11 test: {start_display:.3f}-{stop_display:.3f} {unit}")
            click.echo(f"Points: {test_config.point_count}, Pass threshold: {test_config.pass_threshold_db} dB")
        
        result = s11_test.run_test(test_config)
        
        # Generate outputs
        output_files = []
        for format_type in formats:
            if format_type == 'json':
                generator = JSONGenerator(output_config)
                output_files.append(generator.generate(result, "s11_test"))
            elif format_type == 'csv':
                generator = CSVGenerator(output_config)
                output_files.append(generator.generate(result, "s11_test"))
            elif format_type == 'touchstone':
                generator = TouchstoneGenerator(output_config)
                output_files.append(generator.generate(result, "s11_test"))
            elif format_type == 'log':
                generator = LogGenerator(output_config)
                output_files.append(generator.generate(result, "s11_test"))
            elif format_type == 'plot':
                # Generate plot using the JSON data
                json_generator = JSONGenerator(output_config)
                json_file = json_generator.generate(result, "s11_test")
                output_files.append(json_file)
                
                # Create plot in the same directory as the JSON file
                json_dir = os.path.dirname(json_file)
                plotter = VNAPlotter()
                plot_file = plotter.plot_from_json(json_file, json_dir)
                if plot_file:
                    output_files.append(plot_file)
                    click.echo(f"Plot generated: {plot_file}")
        
        # Display results (always show, even in quiet mode)
        click.echo(f"\nTest Results:")
        click.echo(f"  Status: {result.status.value}")
        click.echo(f"  Pass/Fail: {result.pass_fail_result.value}")
        click.echo(f"  Duration: {result.duration:.2f} seconds")
        # Display worst case frequency in appropriate units
        if result.worst_case_frequency is not None:
            if result.worst_case_frequency >= 1e9:
                freq_display = result.worst_case_frequency/1e9
                freq_unit = "GHz"
            else:
                freq_display = result.worst_case_frequency/1e6
                freq_unit = "MHz"
            click.echo(f"  Worst Case: {result.worst_case_db:.2f} dB @ {freq_display:.3f} {freq_unit}")
        else:
            worst_case_str = f"{result.worst_case_db:.2f}" if result.worst_case_db is not None else "N/A"
            click.echo(f"  Worst Case: {worst_case_str} dB @ N/A")
        click.echo(f"  Output files: {len(output_files)}")
        
        # Show calibration diagnostics
        if test_config.calibration_file:
            click.echo(f"\nCalibration Diagnostics:")
            try:
                from src.librevna.calibration.cal_file import CalFile
                cal_file = CalFile(test_config.calibration_file)
                cal_info = cal_file.get_calibration_info()
                
                click.echo(f"  File: {cal_info['file_path']}")
                click.echo(f"  Type: {cal_info['calibration_type']}")
                click.echo(f"  Ports: {cal_info['used_ports']}")
                click.echo(f"  Frequency Points: {cal_info['frequency_points']}")
                click.echo(f"  Frequency Range: {cal_info['frequency_range'][0]/1e6:.1f} - {cal_info['frequency_range'][1]/1e6:.1f} MHz")
                click.echo(f"  Valid Error Terms: {cal_info['has_valid_terms']}")
                
                # Show error term statistics
                if cal_file.calibration_points:
                    error_terms_stats = {
                        'E00': [abs(p.E00) for p in cal_file.calibration_points if abs(p.E00) > 1e-10],
                        'E11': [abs(p.E11) for p in cal_file.calibration_points if abs(p.E11) > 1e-10],
                        'E10E01': [abs(p.E10E01) for p in cal_file.calibration_points if abs(p.E10E01) > 1e-10],
                        'D': [abs(p.D) for p in cal_file.calibration_points if abs(p.D) > 1e-10],
                        'R': [abs(p.R) for p in cal_file.calibration_points if abs(p.R) > 1e-10],
                        'S': [abs(p.S) for p in cal_file.calibration_points if abs(p.S) > 1e-10],
                        'L': [abs(p.L) for p in cal_file.calibration_points if abs(p.L) > 1e-10],
                        'T': [abs(p.T) for p in cal_file.calibration_points if abs(p.T) > 1e-10],
                        'I': [abs(p.I) for p in cal_file.calibration_points if abs(p.I) > 1e-10]
                    }
                    
                    click.echo(f"  Error Term Statistics:")
                    for term, values in error_terms_stats.items():
                        if values:
                            avg_val = sum(values) / len(values)
                            max_val = max(values)
                            click.echo(f"    {term}: {len(values)} points, avg={avg_val:.6f}, max={max_val:.6f}")
                        else:
                            click.echo(f"    {term}: No valid values")
                            
            except Exception as e:
                click.echo(f"  Error loading calibration diagnostics: {e}")
        
        # Show warning summary if any warnings were suppressed (always show this)
        from src.librevna.calibration.corrector import get_warning_summary
        warning_summary = get_warning_summary()
        if warning_summary:
            if ctx.obj['quiet']:
                click.echo(f"  {warning_summary}")
            else:
                click.echo(f"  {warning_summary}")
        
        # Clean up device connection
        if 'vna' in locals() and vna.is_connected():
            vna.disconnect()
        
        # Exit with appropriate code
        if result.pass_fail_result.value == 'pass':
            sys.exit(0)
        elif result.pass_fail_result.value == 'fail':
            sys.exit(1)
        else:
            sys.exit(2)
            
    except Exception as e:
        # Clean up device connection on error
        if 'vna' in locals() and vna.is_connected():
            vna.disconnect()
        click.echo(f"Error running S11 test: {e}", err=True)
        if verbose:
            import traceback
            traceback.print_exc()
        sys.exit(1)


@s11.command()
@click.option('--template', type=click.Choice(['s11_test', 'device']), default='s11_test', help='Template type')
@click.option('--format', type=click.Choice(['json', 'yaml', 'toml']), default='json', help='Output format')
@click.option('--output', '-o', type=click.Path(), help='Output file path')
def generate_config(template, format, output):
    """
    Generate configuration file from template
    
    Examples:
        # Generate S11 test config
        python cli.py s11 generate-config --template s11_test
        
        # Generate device config in YAML format
        python cli.py s11 generate-config --template device --format yaml -o device.yaml
    """
    try:
        config_manager = ConfigManager()
        
        if not output:
            output = f"{template}_config.{format}"
        
        config_format = ConfigFormat.from_string(format)
        config_manager.generate_template_config(template, output, config_format)
        
        click.echo(f"Configuration template generated: {output}")
        
    except Exception as e:
        click.echo(f"Error generating config: {e}", err=True)
        sys.exit(1)


@s11.command()
@click.argument('config_file', type=click.Path(exists=True))
def validate_config(config_file):
    """
    Validate configuration file
    
    Examples:
        # Validate S11 test config
        python cli.py s11 validate-config s11_config.json
    """
    try:
        # Load config file
        with open(config_file, 'r') as f:
            if config_file.endswith('.yaml') or config_file.endswith('.yml'):
                config_data = yaml.safe_load(f)
            else:
                config_data = json.load(f)
        
        # Determine template from config
        template_name = config_data.get('_template', 's11_test')
        
        # Validate
        config_manager = ConfigManager()
        config_manager.validate_config(config_data, template_name)
        
        click.echo(f"✓ Configuration file is valid: {config_file}")
        
    except Exception as e:
        click.echo(f"✗ Configuration validation failed: {e}", err=True)
        sys.exit(1)


@s11.command()
@click.option('--config', type=click.Path(exists=True), help='Configuration file')
@click.option('--output-dir', '-o', type=click.Path(), default='output', help='Output directory')
@click.option('--verbose', '-v', is_flag=True, help='Verbose output (overrides global log level)')
@click.pass_context
def batch(ctx, config, output_dir, verbose):
    """
    Run batch S11 tests from configuration file
    
    Examples:
        # Run batch tests
        python cli.py s11 batch --config batch_config.json
    """
    # Use global logging configuration, override level if verbose is specified
    if verbose:
        # Override global log level for verbose output
        log_config = LogConfig(
            level="DEBUG",
            console_output=not ctx.obj['quiet'],
            file_output=True,
            log_directory="logs"
        )
        setup_logging(log_config)
    
    try:
        # Load batch configuration
        with open(config, 'r') as f:
            if config.endswith('.yaml') or config.endswith('.yml'):
                batch_config = yaml.safe_load(f)
            else:
                batch_config = json.load(f)
        
        # Create base output configuration (will create shared timestamp for each test)
        base_output_config = OutputConfig(output_directory=output_dir)
        
        # Connect to device for batch tests
        vna = LibreVNA()
        try:
            vna.connect()
            if not vna.is_connected():
                click.echo("Error: Could not connect to device for batch tests", err=True)
                sys.exit(1)
            device_manager = vna
            click.echo(f"Connected to device: {vna.get_device_info()['product']}")
        except Exception as e:
            click.echo(f"Error: Device connection failed for batch tests ({e})", err=True)
            sys.exit(1)
        
        # Initialize S11 test with device manager
        s11_test = S11Test(device_manager=device_manager)
        
        # Process each test configuration
        test_configs = batch_config.get('tests', [])
        results = []
        
        for i, test_data in enumerate(test_configs):
            if not verbose:
                click.echo(f"Running test {i+1}/{len(test_configs)}...")
            
            # Create test configuration
            test_config = S11TestConfig(
                start_frequency=test_data.get('start_frequency', 1e6),
                stop_frequency=test_data.get('stop_frequency', 6e9),
                point_count=test_data.get('point_count', 201),
                pass_threshold_db=test_data.get('pass_threshold_db', -10.0),
                fail_threshold_db=test_data.get('fail_threshold_db', -5.0),
                calibration_file=test_data.get('calibration_file'),
                output_format=test_data.get('output_format', ['json', 'csv'])
            )
            
            # Create output configuration with shared timestamp for this test
            output_config = OutputConfig.create_with_shared_timestamp(
                output_directory=base_output_config.output_directory,
                include_raw_data=base_output_config.include_raw_data,
                include_corrected_data=base_output_config.include_corrected_data,
                include_metadata=base_output_config.include_metadata,
                include_statistics=base_output_config.include_statistics
            )
            
            # Run test
            result = s11_test.run_test(test_config)
            results.append(result)
            
            # Generate outputs
            for format_type in test_config.output_format:
                if format_type == 'json':
                    generator = JSONGenerator(output_config)
                    generator.generate(result, f"test_{i+1}")
                elif format_type == 'csv':
                    generator = CSVGenerator(output_config)
                    generator.generate(result, f"test_{i+1}")
        
        # Summary
        passed = sum(1 for r in results if r.pass_fail_result.value == 'pass')
        failed = sum(1 for r in results if r.pass_fail_result.value == 'fail')
        
        click.echo(f"\nBatch Test Summary:")
        click.echo(f"  Total tests: {len(results)}")
        click.echo(f"  Passed: {passed}")
        click.echo(f"  Failed: {failed}")
        
        # Clean up device connection
        if 'vna' in locals() and vna.is_connected():
            vna.disconnect()
        
        # Exit with appropriate code
        if failed == 0:
            sys.exit(0)
        else:
            sys.exit(1)
            
    except Exception as e:
        # Clean up device connection on error
        if 'vna' in locals() and vna.is_connected():
            vna.disconnect()
        click.echo(f"Error running batch tests: {e}", err=True)
        if verbose:
            import traceback
            traceback.print_exc()
        sys.exit(1)


@cli.command(name="headless-sweep")
@click.option('--cal', type=click.Path(exists=True, path_type=Path), required=True, help='Calibration file (.cal)')
@click.option('--serial', type=str, help='Device serial number to target')
@click.option('--start-freq', type=FrequencyParamType(), required=True, help='Start frequency in GHz (converted to Hz)')
@click.option('--stop-freq', type=FrequencyParamType(), required=True, help='Stop frequency in GHz (converted to Hz)')
@click.option('--points', type=int, required=True, help='Number of sweep points')
@click.option('--ifbw', type=float, required=True, help='IF bandwidth in Hz')
@click.option('--power', type=float, default=-10.0, show_default=True, help='Source power in dBm')
@click.option('--threshold', type=float, default=-10.0, show_default=True, help='Pass/fail threshold in dB')
@click.option('--excite', type=str, default='1,2', show_default=True, help='Comma-separated excited ports')
@click.option('--timeout-ms', type=float, default=15000.0, show_default=True, help='Timeout in milliseconds')
@click.option('--progress', is_flag=True, help='Stream NDJSON progress to stderr')
@click.option('--json-dir', type=click.Path(path_type=Path), help='Directory to preserve the generated JSON payload')
def headless_sweep(cal, serial, start_freq, stop_freq, points, ifbw, power, threshold, excite, timeout_ms, progress, json_dir):
    """Run a sweep using the native librevna-cli binary."""

    def parse_ports(raw: str) -> List[int]:
        ports: List[int] = []
        for item in raw.split(','):
            item = item.strip()
            if not item:
                continue
            try:
                ports.append(int(item))
            except ValueError as exc:
                raise click.BadParameter(f"Invalid port '{item}' in --excite") from exc
        if not ports:
            raise click.BadParameter("At least one port must be provided via --excite")
        return ports

    try:
        runner = HeadlessLibreVNA()
    except FileNotFoundError as exc:
        click.echo(f"Error: {exc}", err=True)
        sys.exit(2)

    ports = parse_ports(excite)

    try:
        result = runner.run_sweep(
            cal=cal,
            f_start=start_freq,
            f_stop=stop_freq,
            points=points,
            if_bandwidth=ifbw,
            power_dbm=power,
            threshold_db=threshold,
            serial=serial,
            timeout_ms=timeout_ms,
            excited_ports=ports,
            progress=progress,
            output_dir=json_dir,
        )
    except Exception as exc:
        click.echo(f"Headless sweep failed: {exc}", err=True)
        sys.exit(3)

    click.echo(json.dumps(result.raw, indent=2))
    sys.exit(0 if result.overall_pass else 1)


if __name__ == '__main__':
    cli()
