# Quick Start Guide - VNA-CLI

Get started with the VNA-CLI C++ command-line tool in 5 minutes.

## Current Status

⚠️ **Important**: This is the **framework implementation** of VNA-CLI. The core structure, interfaces, and workflow are complete, but the LibreVNA C++ driver integration is pending.

**What's Complete**:
- ✅ Project structure and CMake build system
- ✅ Command-line argument parsing with all options
- ✅ Controller class with complete workflow orchestration
- ✅ Signal/slot architecture for async operations
- ✅ Pass/fail evaluation logic
- ✅ JSON/CSV output generation
- ✅ Timeout and error handling
- ✅ Comprehensive documentation

**What's Pending**:
- ⚠️ LibreVNA C++ driver integration (USB communication)
- ⚠️ Actual VNA configuration and sweep execution
- ⚠️ Calibration loading and application
- ⚠️ Measurement data collection

**To Complete**: Follow the detailed instructions in `docs/implementation-plan.md`

---

## Step 1: Prerequisites

### Install Build Tools

**Ubuntu/Debian**:
```bash
sudo apt-get update
sudo apt-get install -y cmake build-essential qtbase5-dev libusb-1.0-0-dev
```

**Fedora/RHEL**:
```bash
sudo dnf install cmake gcc-c++ qt5-qtbase-devel libusb-devel
```

**macOS**:
```bash
brew install cmake qt@5 libusb
```

**Windows** (with vcpkg):
```cmd
vcpkg install qt5-base:x64-windows libusb:x64-windows
```

---

## Step 2: Build the Project

### Quick Build

```bash
cd vna-cli
./build.sh
```

### Manual Build

```bash
cd vna-cli
mkdir build && cd build
cmake ..
make
```

### Build Options

```bash
# Debug build
./build.sh --debug

# Clean build
./build.sh --clean

# Build and install
./build.sh --install
```

**Note**: The build will currently succeed for compilation but will fail at the linking stage because LibreVNA driver code is not yet integrated. See warning messages during build.

---

## Step 3: (Pending) Run a Test Sweep

Once LibreVNA integration is complete, you'll be able to run:

```bash
# Basic sweep
./build/bin/vna-cli --cal calibration.cal \
                    --fstart 1000000 \
                    --fstop 3000000000 \
                    --points 501

# With JSON output
./build/bin/vna-cli --cal calibration.cal \
                    --fstart 1e6 \
                    --fstop 3e9 \
                    --points 501 \
                    --json results.json
```

---

## Step 4: Verify Results

The tool will output:
1. **Console**: Test progress and results
2. **Exit Code**: 0 = pass, 1 = fail
3. **JSON**: Detailed results (if --json specified)
4. **CSV**: Data table (if --csv specified)

**Example Output**:
```
=== LibreVNA CLI Tool ===
Frequency range: 1.0 - 3000.0 MHz
Points: 501
IFBW: 1000 Hz
Power: -10 dBm
Threshold: -10.0 dB

Connecting to LibreVNA device...
Connected to: LibreVNA (Serial: ABC123)
Loading calibration file: calibration.cal
Configuring VNA...
Starting VNA sweep...
Processing measurements...

=== Test Results ===
S11: PASS (max: -15.32 dB)
S21: PASS (max: -2.45 dB)
S12: PASS (max: -2.48 dB)
S22: PASS (max: -14.87 dB)

Overall: PASS
```

---

## Next Steps

### For Users

1. **Wait for LibreVNA Integration**: Watch the repository for updates when driver integration is complete
2. **Prepare Calibration Files**: Use LibreVNA GUI to create .cal files for your device
3. **Review Documentation**: Read `docs/usage-examples.md` for detailed examples

### For Developers

1. **Complete Integration**: Follow `docs/implementation-plan.md` Phase 2-8
2. **Add LibreVNA Source**: 
   ```bash
   cd vna-cli/third_party
   git submodule add https://github.com/jankae/LibreVNA.git librevna
   ```
3. **Update CMakeLists.txt**: Include LibreVNA source files
4. **Implement Placeholders**: Replace TODO comments with actual implementations
5. **Test with Hardware**: Verify against real LibreVNA device

---

## Command-Line Options Reference

| Option | Description | Default | Example |
|--------|-------------|---------|---------|
| `--cal FILE` | Calibration file (required) | - | `--cal my.cal` |
| `--serial NUM` | Device serial number | First device | `--serial ABC123` |
| `--fstart HZ` | Start frequency | 1 MHz | `--fstart 1e6` |
| `--fstop HZ` | Stop frequency | 3 GHz | `--fstop 3e9` |
| `--points N` | Number of points | 501 | `--points 1001` |
| `--ifbw HZ` | IF bandwidth | 1000 | `--ifbw 100` |
| `--power DBM` | Output power | -10 | `--power -20` |
| `--logsweep` | Logarithmic sweep | Linear | `--logsweep` |
| `--dwell SEC` | Dwell time | 0.001 | `--dwell 0.01` |
| `--threshold-db` | Pass/fail threshold | -10 | `--threshold-db -20` |
| `--timeout-s SEC` | Overall timeout | 120 | `--timeout-s 300` |
| `--json FILE` | JSON output | - | `--json out.json` |
| `--csv FILE` | CSV output | - | `--csv out.csv` |

---

## Troubleshooting

### Build Issues

**"cmake: command not found"**
```bash
# Install CMake
sudo apt-get install cmake  # Ubuntu/Debian
brew install cmake           # macOS
```

**"Qt5Core not found"**
```bash
# Install Qt5
sudo apt-get install qtbase5-dev  # Ubuntu/Debian
brew install qt@5                  # macOS
```

**"libusb-1.0 not found"**
```bash
# Install libusb
sudo apt-get install libusb-1.0-0-dev  # Ubuntu/Debian
brew install libusb                     # macOS
```

**Link errors about LibreVNA**
- This is expected until LibreVNA source is integrated
- See `docs/implementation-plan.md` for integration steps

### Runtime Issues (Future)

**"Device not found"**
- Check USB connection
- Verify driver installation (WinUSB on Windows)
- Check permissions (Linux udev rules)

**"Failed to load calibration"**
- Verify calibration file path
- Ensure file is valid .cal format
- Check calibration frequency range matches sweep

**"Operation timed out"**
- Increase timeout: `--timeout-s 300`
- Check device connection
- Reduce point count or IFBW

---

## Resources

- **Main README**: `README.md` - Project overview and features
- **Implementation Plan**: `docs/implementation-plan.md` - Detailed integration guide
- **Usage Examples**: `docs/usage-examples.md` - Practical examples
- **LibreVNA Project**: https://github.com/jankae/LibreVNA
- **Protocol Reference**: LibreVNA Protocol.hpp documentation

---

## Support

For help and questions:

1. **Check Documentation**: Read the detailed guides in `docs/`
2. **Review Implementation Plan**: See what's complete and what's pending
3. **GitHub Issues**: Report problems or ask questions
4. **LibreVNA Community**: Consult LibreVNA project for device-specific questions

---

## Contributing

We welcome contributions! Priority areas:

1. **Complete LibreVNA integration** (see implementation-plan.md)
2. **Add unit tests** for completed components
3. **Improve error handling** and user feedback
4. **Add TCP transport** support
5. **Create example calibration files** and test cases

See `docs/implementation-plan.md` for detailed contribution guidelines.

---

**Last Updated**: 2024  
**Status**: Framework Complete, Driver Integration Pending  
**Version**: 0.1.0
