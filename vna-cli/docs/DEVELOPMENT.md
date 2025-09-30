# Development Guide - VNA-CLI

This guide helps developers understand the architecture and contribute to the VNA-CLI project.

## Architecture Overview

### Design Principles

1. **Minimal Dependencies**: Only QtCore (no GUI), libusb-1.0, standard C++17
2. **Headless Operation**: No user interaction, completely automated
3. **Reuse LibreVNA Code**: Leverage existing drivers, calibration, and protocol
4. **Signal/Slot Architecture**: Qt signals for async USB communication
5. **Clear Exit Codes**: 0 = pass, 1 = fail, consistent with CI/CD expectations

### Component Diagram

```
┌─────────────────────────────────────────────────────────┐
│                      main.cpp                           │
│  - Parse CLI arguments                                  │
│  - Create QCoreApplication                              │
│  - Initialize Controller                                │
│  - Start event loop                                     │
└───────────────────────┬─────────────────────────────────┘
                        │
                        ▼
┌─────────────────────────────────────────────────────────┐
│                   Controller                            │
│  - Orchestrate workflow                                 │
│  - Manage state machine                                 │
│  - Connect driver signals                               │
│  - Apply calibration                                    │
│  - Evaluate pass/fail                                   │
│  - Generate output                                      │
└───────┬──────────────┬──────────────┬───────────────────┘
        │              │              │
        ▼              ▼              ▼
┌──────────────┐ ┌──────────┐ ┌────────────────┐
│LibreVNAUSB   │ │Calibration│ │ S-Parameter    │
│Driver        │ │  Engine   │ │  Evaluation    │
│              │ │           │ │                │
│- USB I/O     │ │- Load .cal│ │- Convert to dB │
│- Protocol    │ │- Correct  │ │- Check thresh  │
│- Signals     │ │  measure  │ │- Aggregate     │
└──────────────┘ └──────────┘ └────────────────┘
```

### State Machine

```
[Start] → [Connect Device] → [Load Cal] → [Configure VNA] → [Sweep]
                   ↓              ↓              ↓              ↓
                [ERROR]        [ERROR]        [ERROR]      [Receive Data]
                   ↓              ↓              ↓              ↓
                [Exit 1]       [Exit 1]       [Exit 1]    [Apply Cal]
                                                               ↓
                                                          [Evaluate]
                                                               ↓
                                                         [Pass/Fail]
                                                               ↓
                                                          [Report]
                                                               ↓
                                                       [Exit 0 or 1]
```

## Code Structure

### controller.hpp

**Key Structures**:
- `SweepConfig`: Input configuration from CLI
- `SParameterData`: Collected measurement data
- `EvaluationResult`: Pass/fail results

**Controller Class**:
- Public: `run()` - Start workflow
- Signals: `finished(int)` - Exit code
- Slots: `onInfoUpdated()`, `onVNAMeasurementReceived()`, etc.
- Private: Implementation methods

### controller.cpp

**Workflow Methods**:
1. `connectToDevice()`: Initialize USB driver, connect signals
2. `loadCalibration()`: Read .cal file, validate
3. `configureVNA()`: Build VNASettings, handle segmentation
4. `startSweep()`: Initiate measurement
5. `processMeasurement()`: Collect data, apply cal, evaluate
6. `evaluateCriteria()`: Check thresholds, determine pass/fail
7. `printReport()`: Console output
8. `saveJSON()`/`saveCSV()`: File outputs
9. `cleanup()`: Disconnect, emit finished signal

### main.cpp

**Responsibilities**:
- Parse command-line arguments with QCommandLineParser
- Validate input parameters
- Create Controller instance with SweepConfig
- Connect Controller::finished to QCoreApplication::exit
- Run event loop

## LibreVNA Integration Points

### Required Source Files

From LibreVNA repository, you'll need:

**USB Driver**:
```
Software/PC_Application/LibreVNA-GUI/Device/LibreVNA/
├── librevnausbdriver.h
├── librevnausbdriver.cpp
├── librevnadriver.h
└── librevnadriver.cpp
```

**Device Interface**:
```
Software/PC_Application/LibreVNA-GUI/Device/
└── devicedriver.h
```

**Protocol**:
```
Software/VNA_embedded/Application/Communication/
└── Protocol.hpp
```

**Calibration**:
```
Software/PC_Application/LibreVNA-GUI/Calibration/
├── calibration.h
├── calibration.cpp
├── calkit.h
├── calkit.cpp
└── (other calibration files)
```

### Key Classes to Use

**LibreVNAUSBDriver**:
```cpp
// Constructor
LibreVNAUSBDriver();

// Connection
bool connectTo(QString serial);
bool connectToFirstAvailable();

// Configuration
void setVNA(DeviceDriver::VNASettings s, std::function<void(bool)> cb);

// Signals
void InfoUpdated(DeviceDriver::Info);
void VNAmeasurementReceived(DeviceDriver::VNAMeasurement);
void ConnectionLost();
```

**Calibration**:
```cpp
// Load calibration
bool fromFile(QString filename);

// Apply correction
DeviceDriver::VNAMeasurement correctMeasurement(DeviceDriver::VNAMeasurement m);

// Query
Type getType();  // e.g., TwoPort, OnePort
int getNumPoints();
```

**DeviceDriver::VNASettings**:
```cpp
struct VNASettings {
    double freqStart;
    double freqStop;
    int points;
    int IFBW;
    double dBmStart;
    double dBmStop;
    bool logSweep;
    double dwellTime;
    std::set<int> excitedPorts;  // {1, 2} for full 2-port
};
```

**DeviceDriver::VNAMeasurement**:
```cpp
struct VNAMeasurement {
    double frequency;
    std::vector<Datapoint> datapoints;
    
    struct Datapoint {
        int port;         // Receiver port (1 or 2)
        int excitedPort;  // Transmitter port (1 or 2)
        double real;      // Real part
        double imag;      // Imaginary part
    };
};
```

### Signal/Slot Connections

In `Controller::connectToDevice()`:

```cpp
// Create driver
m_driver = std::make_unique<LibreVNAUSBDriver>();

// Connect signals (Qt5 syntax)
connect(m_driver.get(), &LibreVNAUSBDriver::InfoUpdated,
        this, &Controller::onInfoUpdated);

connect(m_driver.get(), &LibreVNAUSBDriver::VNAmeasurementReceived,
        this, &Controller::onVNAMeasurementReceived);

connect(m_driver.get(), &LibreVNAUSBDriver::ConnectionLost,
        this, &Controller::onConnectionLost);
```

## Implementation Checklist

### Phase 2: Driver Integration

- [ ] Add LibreVNA source as git submodule
- [ ] Update CMakeLists.txt with LibreVNA source files
- [ ] Add include paths for LibreVNA headers
- [ ] Link against required Qt5 libraries (Core, Widgets minimal)
- [ ] Verify compilation of LibreVNA code

### Phase 3: USB Connection

- [ ] Implement `connectToDevice()` with LibreVNAUSBDriver
- [ ] Handle device discovery (first available or by serial)
- [ ] Connect all required signals
- [ ] Request device info and status
- [ ] Store device limits (maxPoints) for segmentation
- [ ] Test connection on real hardware

### Phase 4: Calibration

- [ ] Implement `loadCalibration()` with Calibration::fromFile
- [ ] Handle calibration type detection (1-port vs 2-port)
- [ ] Validate calibration frequency range
- [ ] Store calibration instance in controller
- [ ] Implement `applyCalibration()` loop
- [ ] Call `cal->correctMeasurement()` for each point
- [ ] Verify corrected values match GUI

### Phase 5: VNA Configuration

- [ ] Implement `configureVNA()` with VNASettings
- [ ] Set frequency range, points, IFBW, power
- [ ] Configure excitedPorts = {1, 2} for full 2-port
- [ ] Call `driver->setVNA()` with callback
- [ ] Handle configuration errors
- [ ] Test single sweep (points < maxPoints)

### Phase 6: Segmentation

- [ ] Check if points > device maxPoints
- [ ] Calculate number of segments needed
- [ ] Split frequency range into segments
- [ ] Configure and execute each segment
- [ ] Track global vs segment point indices
- [ ] Merge data from all segments
- [ ] Test with large point counts

### Phase 7: Data Collection

- [ ] Implement `onVNAMeasurementReceived()` handler
- [ ] Extract S11, S21, S12, S22 from datapoints
- [ ] Map excitedPort/port combinations correctly
- [ ] Store in m_data vectors
- [ ] Track progress (m_receivedPoints)
- [ ] Detect sweep completion
- [ ] Test data collection accuracy

### Phase 8: Evaluation

- [ ] Complete `evaluateCriteria()` implementation
- [ ] Convert complex values to dB: 20*log10(|S|)
- [ ] Compare each point against threshold
- [ ] Track max values per parameter
- [ ] Determine per-parameter pass/fail
- [ ] Compute overall pass/fail
- [ ] Test with various thresholds

### Phase 9: Output Generation

- [ ] Complete `saveJSON()` with full data
- [ ] Complete `saveCSV()` with frequency/dB columns
- [ ] Add timestamps to output files
- [ ] Handle file write errors gracefully
- [ ] Verify JSON format is valid
- [ ] Test CSV import in Excel/MATLAB

### Phase 10: Error Handling

- [ ] Implement timeout handling
- [ ] Handle USB disconnection gracefully
- [ ] Validate calibration file format
- [ ] Check for device communication errors
- [ ] Provide clear error messages
- [ ] Return appropriate exit codes
- [ ] Add logging for debug

### Phase 11: Testing

- [ ] Test on Windows with WinUSB driver
- [ ] Test on Linux with libusb
- [ ] Test on macOS
- [ ] Compare results with LibreVNA GUI
- [ ] Test segmentation at boundary conditions
- [ ] Test timeout scenarios
- [ ] Test invalid inputs
- [ ] Performance testing

## Coding Standards

### Style

- **Naming**: 
  - Classes: `PascalCase`
  - Methods: `camelCase`
  - Members: `m_camelCase`
  - Constants: `UPPER_CASE`
- **Indentation**: 4 spaces
- **Braces**: K&R style (opening brace on same line)
- **Comments**: Doxygen-style for public API

### Error Handling

- **Return Values**: `bool` for success/failure
- **Exceptions**: Avoid in signal handlers
- **Logging**: Use `std::cout` for info, `std::cerr` for errors
- **Validation**: Check all inputs and return values

### Qt Best Practices

- **Signals/Slots**: Use new syntax (function pointers)
- **Memory**: Use smart pointers (`std::unique_ptr`)
- **Connections**: Use `Qt::QueuedConnection` for cross-thread
- **Event Loop**: Never block, use signals for async

## Testing Strategies

### Unit Tests

Create tests for:
- CLI argument parsing
- Frequency range validation
- dB conversion accuracy
- Threshold evaluation logic
- Segmentation calculations

### Integration Tests

Test with real hardware:
- Device connection/disconnection
- Calibration loading
- Sweep execution
- Data accuracy
- Output file generation

### Validation Tests

Compare with LibreVNA GUI:
- Same device, same calibration
- Same frequency range and settings
- Verify S-parameter values match
- Check calibrated vs uncalibrated

### Regression Tests

Maintain test cases for:
- Boundary conditions
- Error scenarios
- Different device models
- Various calibration types

## Debugging Tips

### USB Communication

```cpp
// Enable libusb debug output
setenv("LIBUSB_DEBUG", "4", 1);  // Before libusb_init
```

### Qt Debug Output

```cpp
qDebug() << "Frequency:" << m.frequency;
qDebug() << "Datapoints:" << m.datapoints.size();
```

### GDB Debugging

```bash
cd build
gdb ./bin/vna-cli
(gdb) run --cal test.cal --fstart 1e6 --fstop 3e9
(gdb) break Controller::onVNAMeasurementReceived
(gdb) continue
(gdb) print m_receivedPoints
```

### Valgrind Memory Check

```bash
valgrind --leak-check=full ./build/bin/vna-cli --cal test.cal ...
```

## Performance Considerations

### Optimization

- **Minimize USB Transactions**: Batch where possible
- **Preallocate Vectors**: Use `reserve()` for known sizes
- **Avoid String Copies**: Use `const&` for QString
- **Cache Calibration**: Don't reload per measurement

### Profiling

```bash
# Compile with profiling
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
make

# Run with profiler
perf record ./bin/vna-cli --cal test.cal ...
perf report
```

## Contributing

### Before You Start

1. Read this entire guide
2. Review implementation-plan.md
3. Check existing issues on GitHub
4. Discuss major changes first

### Pull Request Process

1. **Fork** the repository
2. **Branch** from main: `git checkout -b feature/your-feature`
3. **Implement** your changes
4. **Test** thoroughly
5. **Document** in code comments and README if needed
6. **Commit** with clear messages
7. **Push** to your fork
8. **Open PR** with description

### PR Checklist

- [ ] Code compiles without warnings
- [ ] All existing tests pass
- [ ] New tests added for new features
- [ ] Documentation updated
- [ ] Code follows style guide
- [ ] Commit messages are descriptive

## Resources

### LibreVNA Documentation

- [LibreVNA GitHub](https://github.com/jankae/LibreVNA)
- [Protocol.hpp](https://github.com/jankae/LibreVNA/blob/master/Software/VNA_embedded/Application/Communication/Protocol.hpp)
- [GUI Source](https://github.com/jankae/LibreVNA/tree/master/Software/PC_Application/LibreVNA-GUI)

### Qt Documentation

- [Qt5 Core](https://doc.qt.io/qt-5/qtcore-index.html)
- [Signals & Slots](https://doc.qt.io/qt-5/signalsandslots.html)
- [QCommandLineParser](https://doc.qt.io/qt-5/qcommandlineparser.html)

### USB/libusb

- [libusb-1.0 API](https://libusb.sourceforge.io/api-1.0/)
- [WinUSB Driver](https://zadig.akeo.ie/)

## Support

For development questions:
- **GitHub Issues**: Technical questions and bugs
- **Discussions**: Design decisions and feature requests
- **LibreVNA Community**: Device-specific questions

---

**Happy Coding!**

This framework is ready for LibreVNA integration. Follow the implementation plan systematically, test frequently, and don't hesitate to ask for help. Good luck!
