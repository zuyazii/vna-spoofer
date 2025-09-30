# VNA-CLI: C++ Headless Command-Line Tool for LibreVNA

A headless C++ command-line tool for LibreVNA Vector Network Analyzer devices that performs automated S-parameter measurements with calibration and pass/fail evaluation.

## Overview

This tool connects to a LibreVNA device over USB (WinUSB), performs S11/S21/S12/S22 sweeps over a specified frequency range, applies calibration to each datapoint, and evaluates pass/fail criteria against a magnitude threshold.

### Key Features

- **Headless Operation**: No GUI required, uses QtCore for event loop
- **USB Communication**: WinUSB/libusb-1.0 for Windows/Linux
- **Full 2-Port Measurements**: S11, S21, S12, S22 in a single sweep
- **Calibration Support**: Loads and applies .cal files
- **Automatic Segmentation**: Handles large point counts by splitting sweeps
- **Pass/Fail Evaluation**: Configurable threshold with clear exit codes
- **Multiple Output Formats**: JSON, CSV for integration

## Prerequisites

### Build Dependencies

- **CMake** >= 3.16
- **Qt5Core** (QtCore only, no GUI)
- **libusb-1.0** (WinUSB backend on Windows)
- **C++17 compiler** (GCC, Clang, or MSVC)
- **nlohmann/json** (header-only, optional)

### Runtime Dependencies

- LibreVNA device with WinUSB driver installed (Windows)
- USB permissions configured (Linux)

## Building

### Linux

```bash
# Install dependencies (Ubuntu/Debian)
sudo apt-get install cmake qtbase5-dev libusb-1.0-0-dev build-essential

# Build
cd vna-cli
mkdir build && cd build
cmake ..
make

# Install (optional)
sudo make install
```

### Windows

```bash
# Install Qt5 and libusb (using vcpkg or manual installation)
vcpkg install qt5-base:x64-windows libusb:x64-windows

# Build
cd vna-cli
mkdir build && cd build
cmake -DCMAKE_TOOLCHAIN_FILE=[vcpkg root]/scripts/buildsystems/vcpkg.cmake ..
cmake --build . --config Release

# Executable will be in build/bin/Release/vna-cli.exe
```

### macOS

```bash
# Install dependencies (using Homebrew)
brew install cmake qt@5 libusb

# Build
cd vna-cli
mkdir build && cd build
cmake -DCMAKE_PREFIX_PATH=/usr/local/opt/qt@5 ..
make

# Install (optional)
sudo make install
```

## Usage

### Basic Command

```bash
vna-cli --cal calibration.cal --fstart 1000000 --fstop 3000000000 --points 501
```

### Command-Line Options

| Option | Short | Description | Default |
|--------|-------|-------------|---------|
| `--cal FILE` | `-c` | Path to calibration file (.cal) | **Required** |
| `--serial SERIAL` | `-s` | Device serial number | First device |
| `--fstart HZ` | | Start frequency (Hz) | 1000000 (1 MHz) |
| `--fstop HZ` | | Stop frequency (Hz) | 3000000000 (3 GHz) |
| `--points N` | `-p` | Number of points | 501 |
| `--ifbw HZ` | | IF bandwidth (Hz) | 1000 |
| `--power DBM` | | Output power (dBm) | -10 |
| `--logsweep` | | Use logarithmic sweep | Linear |
| `--dwell SEC` | | Dwell time per point (s) | 0.001 |
| `--threshold-db DB` | `-t` | Pass/fail threshold (dB) | -10.0 |
| `--timeout-s SEC` | | Overall timeout (s) | 120 |
| `--json FILE` | | Save results to JSON | - |
| `--csv FILE` | | Save results to CSV | - |

### Examples

**1. Basic S-parameter sweep (1 MHz to 3 GHz)**
```bash
vna-cli --cal my_cal.cal --fstart 1e6 --fstop 3e9 --points 501
```

**2. High-resolution measurement with custom threshold**
```bash
vna-cli --cal filter.cal --fstart 100e6 --fstop 1e9 --points 1001 \
        --ifbw 100 --threshold-db -20
```

**3. Logarithmic sweep with JSON output**
```bash
vna-cli --cal device.cal --fstart 1e6 --fstop 10e9 --points 201 \
        --logsweep --json results.json
```

**4. Connect to specific device by serial**
```bash
vna-cli --serial ABC123 --cal calibration.cal --fstart 1e6 --fstop 3e9
```

## Exit Codes

- **0**: All measurements passed threshold criteria
- **1**: One or more measurements failed, or connection/configuration error
- **2**: Invalid arguments or usage error

## Architecture

### Components

```
vna-cli/
├── src/
│   ├── main.cpp           # CLI argument parsing and application entry
│   └── controller.cpp     # Workflow orchestration and state machine
├── include/
│   └── controller.hpp     # Controller interface and data structures
├── third_party/           # Vendored dependencies (LibreVNA driver)
│   └── librevna/          # To be added: LibreVNA C++ source
├── docs/                  # Documentation
└── CMakeLists.txt         # Build configuration
```

### Workflow

1. **Initialize**: Parse arguments, create QCoreApplication
2. **Connect**: Initialize LibreVNAUSBDriver, connect to device
3. **Load Calibration**: Read .cal file, validate
4. **Configure**: Build VNASettings, handle segmentation if needed
5. **Sweep**: Execute measurement, receive datapoints via signals
6. **Calibrate**: Apply calibration correction to each point
7. **Evaluate**: Convert to dB, check threshold, determine pass/fail
8. **Report**: Print summary, save JSON/CSV if requested
9. **Exit**: Return appropriate exit code

### Protocol & Transport

- **USB Endpoints**:
  - OUT (host→device): 0x01
  - IN (device→host): 0x81
  - LOG IN (optional): 0x82
- **Protocol**: Uses Protocol.hpp shared with firmware (not custom format)
- **Driver**: LibreVNAUSBDriver handles packet framing, ACKs, retries

### Calibration

The tool reuses LibreVNA's calibration engine:
- Loads `.cal` files via `Calibration::fromFile()`
- Applies correction via `calibration->correctMeasurement(VNAMeasurement)`
- Supports 1-port and 2-port calibration methods

### Segmentation

If requested points exceed device's `maxPoints`:
1. Calculate number of segments
2. For each segment:
   - Adjust freqStart/freqStop
   - Call setVNA with segment-specific settings
   - Renumber received points to global index
3. Merge all segments into final dataset

## Integration with LibreVNA C++ Source

**Note**: This implementation currently contains placeholder code. To complete it, you need to:

1. **Add LibreVNA source as submodule**:
   ```bash
   cd vna-cli/third_party
   git submodule add https://github.com/jankae/LibreVNA.git librevna
   ```

2. **Update CMakeLists.txt** to include:
   - LibreVNA USB driver source files
   - Protocol.hpp
   - Calibration classes
   - Device driver base classes

3. **Link against required components**:
   - `LibreVNAUSBDriver`
   - `Calibration`
   - `DeviceDriver` base classes

4. **Implement connections in controller.cpp**:
   - Replace placeholder code with actual driver calls
   - Wire up Qt signals/slots for VNAmeasurementReceived
   - Handle device info, status, and errors

## Testing

### With Real Hardware

1. Connect LibreVNA device via USB
2. Ensure WinUSB driver is installed (Windows) or permissions set (Linux)
3. Run with known-good calibration file:
   ```bash
   vna-cli --cal test.cal --fstart 1e6 --fstop 3e9 --points 51
   ```

### Compare with GUI

Run the same sweep in LibreVNA GUI and vna-cli, verify:
- Calibrated S-parameter values match within tolerance
- Pass/fail evaluation is consistent
- Segmentation works correctly at boundary conditions

### Validation

- Verify calibration is applied correctly by comparing raw vs corrected values
- Test with various point counts (< maxPoints, = maxPoints, > maxPoints)
- Test timeout and connection loss handling
- Verify exit codes for pass/fail scenarios

## Current Status

This is the **initial framework** for the C++ VNA-CLI tool. The following components are implemented:

✅ **Complete**:
- Project structure and CMake build system
- Command-line argument parsing
- Controller class skeleton with workflow orchestration
- Main event loop and signal/slot architecture
- Pass/fail evaluation logic
- JSON/CSV output interfaces

⚠️ **Placeholder/TODO**:
- LibreVNA C++ driver integration (requires vendoring source)
- USB device connection and communication
- Protocol packet handling
- Calibration loading and application
- VNA configuration and sweep execution
- Measurement data collection
- Actual implementation of correctMeasurement()

## Next Steps

1. **Vendor LibreVNA source**: Add as submodule or copy required files
2. **Complete USB integration**: Implement connectToDevice() with LibreVNAUSBDriver
3. **Add calibration**: Integrate Calibration class, implement loading/correction
4. **Implement VNA config**: Port setVNA() logic from GUI source
5. **Handle measurements**: Process VNAmeasurementReceived signal
6. **Add segmentation**: Split large sweeps across multiple setVNA calls
7. **Testing**: Validate against real hardware and compare with GUI

## Contributing

To contribute to this project:

1. Complete the LibreVNA driver integration
2. Add comprehensive error handling
3. Implement TCP transport support
4. Add Touchstone export
5. Create unit tests for calibration and evaluation
6. Improve documentation and examples

## License

This project follows the same license as LibreVNA. See LICENSE file for details.

## References

- [LibreVNA Project](https://github.com/jankae/LibreVNA)
- [LibreVNA Protocol.hpp](https://github.com/jankae/LibreVNA/blob/master/Software/VNA_embedded/Application/Communication/Protocol.hpp)
- [LibreVNA USB Driver](https://github.com/jankae/LibreVNA/tree/master/Software/PC_Application/LibreVNA-GUI/Device/LibreVNA)
- [Calibration Implementation](https://github.com/jankae/LibreVNA/tree/master/Software/PC_Application/LibreVNA-GUI/Calibration)

## Support

For issues and questions:
- Check LibreVNA documentation
- Review the troubleshooting section
- Create an issue on GitHub
