# LibreVNA CLI

A command-line interface for LibreVNA Vector Network Analyzer devices.

## Overview

This repository contains two CLI implementations for LibreVNA:

1. **Python CLI** (root directory): Full-featured Python-based interface with S11 testing, calibration, and visualization
2. **C++ CLI** (vna-cli/ directory): Headless C++ command-line tool for automated S-parameter testing with pass/fail evaluation

### Python CLI

LibreVNA CLI provides a Python-based command-line interface for communicating with LibreVNA devices via USB. It supports device discovery, connection management, and S11 testing with calibration.

## Features

### Python CLI Features

- **Device Discovery**: Automatically find and list available LibreVNA devices
- **Enhanced USB Communication**:
  - LibUSB transport (recommended for WinUSB drivers)
  - pyusb transport (fallback for legacy systems)
  - Automatic fallback between transport methods
- **Protocol Support**: Binary protocol parsing and generation
- **S11 Testing**: Complete S11 test automation with calibration
- **Calibration**: Load and apply calibration data
- **Multiple Output Formats**: JSON, CSV, Touchstone
- **CLI Interface**: Easy-to-use command-line interface
- **Cross-Platform**: Works on Windows, macOS, and Linux
- **WinUSB Driver Support**: Optimized for Windows WinUSB drivers

### C++ CLI Features (vna-cli/)

The C++ CLI is a **headless command-line tool** designed for production testing and automation:

- **Full 2-Port S-Parameter Measurement**: S11, S21, S12, S22 in a single sweep
- **Calibration Support**: Load and apply .cal files to measurements
- **Automated Pass/Fail**: Configurable magnitude threshold with clear exit codes
- **Sweep Segmentation**: Automatically handles point counts exceeding device limits
- **Multiple Output Formats**: JSON, CSV for integration with test systems
- **Headless Operation**: No GUI dependencies, QtCore only
- **Production Ready**: Designed for CI/CD and automated test systems

For details, see [vna-cli/README.md](vna-cli/README.md) and [vna-cli/QUICKSTART.md](vna-cli/QUICKSTART.md).

## Installation

### Python CLI Installation

#### Prerequisites

- Python 3.8 or higher
- USB drivers for LibreVNA device (if required by your OS)

### Install from Source

```bash
# Clone the repository
git clone https://github.com/your-username/librevna-cli.git
cd librevna-cli

# Install in development mode
pip install -e .

# Or install with development dependencies
pip install -e ".[dev]"
```

### Install Dependencies

```bash
# Install core dependencies
pip install -r requirements.txt

# Install development dependencies (optional)
pip install -r requirements-dev.txt
```

### C++ CLI Installation

For the C++ CLI tool, see the dedicated documentation:
- **Quick Start**: [vna-cli/QUICKSTART.md](vna-cli/QUICKSTART.md)
- **Full README**: [vna-cli/README.md](vna-cli/README.md)
- **Development Guide**: [vna-cli/docs/DEVELOPMENT.md](vna-cli/docs/DEVELOPMENT.md)

Brief steps:
```bash
cd vna-cli
./build.sh

# Or manual build
mkdir build && cd build
cmake ..
make
```

## Usage

### Python CLI Basic Commands

```bash
# List available devices
python cli.py list

# Connect to first available device
python cli.py connect

# Connect to specific device by serial
python cli.py connect --serial ABC123

# Get device information
python cli.py info

# Test communication
python cli.py test

# Show version information
python cli.py version
```

### Command Options

```bash
# Show help
python cli.py --help

# Show help for specific command
python cli.py connect --help
```

### C++ CLI Usage

```bash
# Basic S-parameter sweep with calibration and pass/fail
vna-cli --cal calibration.cal \
        --fstart 1000000 \
        --fstop 3000000000 \
        --points 501 \
        --threshold-db -10

# With JSON output for automation
vna-cli --cal test.cal \
        --fstart 1e6 \
        --fstop 3e9 \
        --json results.json

# Exit codes: 0 = pass, 1 = fail
```

For more examples, see [vna-cli/docs/usage-examples.md](vna-cli/docs/usage-examples.md)

## Development

### Project Structure

```
librevna-cli/
├── cli.py                   # Python CLI entry point
├── src/
│   └── librevna/            # Python package
│       ├── device/          # Device management
│       ├── transport/       # Transport layer (USB, TCP)
│       ├── protocol/        # Protocol parsing and generation
│       ├── calibration/     # Calibration engine
│       └── measure/         # Measurement and testing
├── vna-cli/                 # C++ CLI tool
│   ├── src/                 # C++ source files
│   ├── include/             # C++ headers
│   ├── docs/                # C++ documentation
│   └── CMakeLists.txt       # C++ build configuration
├── setup.py                 # Python package setup
├── requirements.txt         # Python dependencies
└── README.md               # This file
```

### Running Tests

```bash
# Run all tests
pytest

# Run with coverage
pytest --cov=librevna

# Run specific test file
pytest tests/test_transport.py
```

### Code Quality

```bash
# Format code
black .

# Lint code
flake8 .

# Type checking
mypy librevna/
```

## API Usage

### Python API

```python
from librevna.device import LibreVNA

# Create device instance
vna = LibreVNA()

# Connect to device (auto-detects best transport method)
vna.connect()

# Or specify transport method explicitly
vna.connect(transport='LibUSB')  # Recommended for WinUSB drivers
vna.connect(transport='USB')     # pyusb fallback

# Get device information
info = vna.get_device_info()
print(f"Connected to: {info['product']}")

# Send commands
vna.send_command(b'\x01\x02\x03\x04')

# Receive responses
response = vna.receive_response(64)

# Disconnect
vna.disconnect()

# Or use as context manager
with LibreVNA() as vna:
    vna.connect()
    # ... perform operations
    # Automatically disconnected on exit
```

### Transport Layer

```python
from librevna.transport import LibUSBTransport, USBTransport

# Create LibUSB transport (recommended for WinUSB drivers)
transport = LibUSBTransport()

# Or create pyusb transport (fallback)
transport = USBTransport()

# Discover devices
devices = transport.discover_devices()
for device in devices:
    print(f"Found: {device['product']} ({device['serial']})")

# Connect to device
transport.connect()

# Send/receive data
transport.send(b'\x01\x02\x03\x04')
response = transport.recv(64)

# Disconnect
transport.disconnect()
```

## Troubleshooting

### Common Issues

1. **Device not found**

   - Check USB connection
   - Ensure device is powered on
   - Install required USB drivers
   - Try running with administrator privileges

2. **Permission denied**

   - On Linux: Add udev rules for LibreVNA device
   - On Windows: Install proper USB drivers
   - On macOS: Grant USB permissions

3. **Import errors**
   - Ensure all dependencies are installed: `pip install -r requirements.txt`
   - Check Python version (3.8+ required)

### USB Drivers

#### Windows

- **Recommended**: Use LibUSB transport with WinUSB drivers
  - Install WinUSB drivers for LibreVNA device
  - Use Zadig tool to install proper drivers if needed
- **Fallback**: Use pyusb transport with libusb-win32 drivers

#### Linux

- Add udev rules for LibreVNA device:
  ```bash
  sudo cp Examples/LibreVNA-GUI_Source/51-vna.rules /etc/udev/rules.d/
  sudo udevadm control --reload-rules
  ```

#### macOS

- Grant USB permissions in System Preferences
- Install libusb if needed: `brew install libusb`

## Contributing

1. Fork the repository
2. Create a feature branch: `git checkout -b feature-name`
3. Make your changes
4. Run tests: `pytest`
5. Commit your changes: `git commit -am 'Add feature'`
6. Push to the branch: `git push origin feature-name`
7. Submit a pull request

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Acknowledgments

- LibreVNA project for the hardware and protocol documentation
- libusb-package for enhanced USB communication with WinUSB driver support
- PyUSB library for USB communication fallback
- Click library for command-line interface

## Roadmap

### Python CLI
- [ ] TCP transport support
- [x] Calibration procedures
- [x] S-parameter measurements
- [x] Data export (Touchstone, CSV, JSON)
- [ ] GUI interface
- [x] Advanced VNA operations

### C++ CLI
- [x] Project framework and architecture (Phase 1 complete)
- [x] CLI argument parsing and workflow orchestration
- [x] Documentation and usage examples
- [ ] LibreVNA C++ driver integration (Phase 2 pending)
- [ ] USB communication and device management
- [ ] Calibration loading and application
- [ ] VNA configuration and sweep execution
- [ ] Pass/fail evaluation with real measurements
- [ ] Hardware testing and validation
- [ ] TCP transport support

For detailed C++ implementation status, see [vna-cli/docs/implementation-plan.md](vna-cli/docs/implementation-plan.md)

## Support

For issues and questions:

- Create an issue on GitHub
- Check the troubleshooting section
- Review the LibreVNA documentation
