# LibreVNA CLI

A command-line interface for LibreVNA Vector Network Analyzer devices.

## Overview

LibreVNA CLI provides a Python-based command-line interface for communicating with LibreVNA devices via USB. It supports device discovery, connection management, and basic VNA operations.

## Features

- **Device Discovery**: Automatically find and list available LibreVNA devices
- **Enhanced USB Communication**:
  - LibUSB transport (recommended for WinUSB drivers)
  - pyusb transport (fallback for legacy systems)
  - Automatic fallback between transport methods
- **Protocol Support**: Binary protocol parsing and generation
- **CLI Interface**: Easy-to-use command-line interface
- **Cross-Platform**: Works on Windows, macOS, and Linux
- **WinUSB Driver Support**: Optimized for Windows WinUSB drivers
- **Headless Sweep Engine**: Optional C++ binary (`librevna-cli`) for calibrated sweeps with JSON output

## Installation

### Prerequisites

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

## Usage

### Basic Commands

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

### Headless Sweep via C++ Binary

The repository now ships with a C++ project (`cpp/`) that builds a small
`librevna-cli` executable. Python remains the orchestration layer, invoking the
binary to perform fully calibrated sweeps and parsing the JSON response.

```bash
# Configure & build the C++ project (requires CMake, a C++17 compiler, Qt, libusb)
cmake -S cpp -B cpp/build
cmake --build cpp/build --config Release

# Run a headless sweep from Python (frequencies in GHz)
python cli.py headless-sweep \
  --cal Calibration/my_calibration.cal \
  --start-freq 0.001 \
  --stop-freq 3.0 \
  --points 201 \
  --ifbw 1000 \
  --power -10 \
  --threshold -10

# Or call the native binary directly
cpp/build/librevna-cli --cal Calibration/my_calibration.cal --fstart 1e6 --fstop 3e9 \
  --points 201 --ifbw 1000 --power -10 --threshold -10
```

The Python wrapper automatically locates the binary (respecting the
`LIBREVNA_CLI_BIN` environment variable) and raises informative errors if the
executable is missing.

> **Note:** The default build uses a stub host core that simulates sweeps so the
> CLI can be exercised without hardware. When the official LibreVNA host stack
> is available, configure CMake with
> `-DLIBREVNA_HEADLESS_ENABLE_REAL_DRIVER=ON` to compile against the vendored
> sources and link Qt/libusb for live measurements.

### Command Options

```bash
# Show help
python cli.py --help

# Show help for specific command
python cli.py connect --help
```

## Development

### Project Structure

```
librevna-cli/
├── librevna/                 # Main package
│   ├── device/              # Device management
│   ├── transport/           # Transport layer (USB, TCP)
│   └── protocol/            # Protocol parsing and generation
├── cpp/                     # Headless C++ project (librevna-cli binary)
├── cli.py                   # Command-line interface
├── setup.py                 # Package setup
├── requirements.txt         # Core dependencies
├── requirements-dev.txt     # Development dependencies
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

- [ ] TCP transport support
- [ ] Calibration procedures
- [ ] S-parameter measurements
- [ ] Data export (Touchstone, CSV)
- [ ] GUI interface
- [ ] Advanced VNA operations

## Support

For issues and questions:

- Create an issue on GitHub
- Check the troubleshooting section
- Review the LibreVNA documentation
