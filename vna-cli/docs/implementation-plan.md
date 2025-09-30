# VNA-CLI Implementation Plan

## Overview

This document outlines the complete implementation plan for the C++ headless command-line tool for LibreVNA, based on the problem statement requirements.

## Project Status

### Current State (Phase 1 - Framework Complete)

✅ **Completed**:
- [x] Project structure created (src/, include/, third_party/, docs/)
- [x] CMakeLists.txt with Qt5Core and libusb dependencies
- [x] Command-line argument parsing (main.cpp)
- [x] Controller class skeleton (controller.hpp/cpp)
- [x] Signal/slot architecture for async operations
- [x] Basic workflow orchestration
- [x] Pass/fail evaluation framework
- [x] JSON/CSV output interfaces
- [x] Comprehensive README and documentation

### Next Phases (Implementation)

The following phases require actual LibreVNA C++ source code to be integrated:

## Phase 2: LibreVNA Source Integration

### 2.1 Add LibreVNA as Dependency

**Option A: Git Submodule (Recommended)**
```bash
cd vna-cli/third_party
git submodule add https://github.com/jankae/LibreVNA.git librevna
git submodule update --init --recursive
```

**Option B: Vendor Subset**
Copy only the required source files:
- `Software/PC_Application/LibreVNA-GUI/Device/LibreVNA/librevnausbdriver.{h,cpp}`
- `Software/PC_Application/LibreVNA-GUI/Device/LibreVNA/librevnadriver.{h,cpp}`
- `Software/PC_Application/LibreVNA-GUI/Device/devicedriver.h`
- `Software/VNA_embedded/Application/Communication/Protocol.hpp`
- `Software/PC_Application/LibreVNA-GUI/Calibration/calibration.{h,cpp}`
- `Software/PC_Application/LibreVNA-GUI/Calibration/*.{h,cpp}` (all calibration files)

**Required Files**:
- USB Driver: `librevnausbdriver.{h,cpp}`
- Base Driver: `librevnadriver.{h,cpp}`
- Protocol: `Protocol.hpp`
- Device Interface: `devicedriver.h`
- Calibration: `calibration.{h,cpp}`, `calkit.{h,cpp}`, etc.
- VNA Measurement: Data structures for VNAMeasurement

### 2.2 Update CMakeLists.txt

Add LibreVNA source files:

```cmake
# LibreVNA source files
set(LIBREVNA_SOURCES
    third_party/librevna/Software/PC_Application/LibreVNA-GUI/Device/LibreVNA/librevnausbdriver.cpp
    third_party/librevna/Software/PC_Application/LibreVNA-GUI/Device/LibreVNA/librevnadriver.cpp
    third_party/librevna/Software/PC_Application/LibreVNA-GUI/Calibration/calibration.cpp
    # Add other required files
)

# Include directories for LibreVNA
include_directories(
    third_party/librevna/Software/PC_Application/LibreVNA-GUI
    third_party/librevna/Software/VNA_embedded/Application/Communication
)

# Add to executable
add_executable(vna-cli ${VNA_CLI_SOURCES} ${LIBREVNA_SOURCES})
```

### 2.3 Handle Dependencies

LibreVNA GUI uses:
- Qt5: Core, Widgets (we only need Core)
- libusb-1.0
- nlohmann/json (for calibration files)

Add nlohmann/json:
```cmake
# Option 1: System package
find_package(nlohmann_json REQUIRED)
target_link_libraries(vna-cli nlohmann_json::nlohmann_json)

# Option 2: Header-only include
include_directories(third_party/json/include)
```

## Phase 3: USB Device Integration

### 3.1 Implement connectToDevice()

In `controller.cpp`:

```cpp
#include "Device/LibreVNA/librevnausbdriver.h"

bool Controller::connectToDevice()
{
    // Create USB driver instance
    m_driver = std::make_unique<LibreVNAUSBDriver>();
    
    // Connect signals
    connect(m_driver.get(), &LibreVNAUSBDriver::InfoUpdated,
            this, &Controller::onInfoUpdated);
    connect(m_driver.get(), &LibreVNAUSBDriver::ConnectionLost,
            this, &Controller::onConnectionLost);
    connect(m_driver.get(), &LibreVNAUSBDriver::VNAmeasurementReceived,
            this, &Controller::onVNAMeasurementReceived);
    
    // Connect to device
    bool connected = false;
    if (m_config.serial.empty()) {
        // Connect to first available device
        connected = m_driver->connectToFirstAvailable();
    } else {
        // Connect to specific serial
        connected = m_driver->connectTo(QString::fromStdString(m_config.serial));
    }
    
    if (!connected) {
        return false;
    }
    
    // Request device info
    m_driver->requestDeviceInfo();
    m_driver->requestDeviceStatus();
    
    m_connected = true;
    return true;
}
```

### 3.2 Handle Device Signals

```cpp
void Controller::onInfoUpdated()
{
    auto info = m_driver->getInfo();
    std::cout << "Connected to: " << info.hardware_version.toStdString() << std::endl;
    std::cout << "Serial: " << info.serial.toStdString() << std::endl;
    std::cout << "Max Points: " << info.limits.VNA.maxPoints << std::endl;
    
    // Store max points for segmentation
    m_maxPoints = info.limits.VNA.maxPoints;
}

void Controller::onVNAMeasurementReceived(const DeviceDriver::VNAMeasurement& m)
{
    // Store frequency
    if (m_data.frequencies.size() <= m_receivedPoints) {
        m_data.frequencies.push_back(m.frequency);
    }
    
    // Store S-parameters (map by excited port combinations)
    // Port 1 excited: S11, S21
    // Port 2 excited: S12, S22
    for (const auto& datapoint : m.datapoints) {
        if (datapoint.port == 1) {
            if (datapoint.excitedPort == 1) {
                m_data.s11.push_back(std::complex<double>(datapoint.real, datapoint.imag));
            } else if (datapoint.excitedPort == 2) {
                m_data.s12.push_back(std::complex<double>(datapoint.real, datapoint.imag));
            }
        } else if (datapoint.port == 2) {
            if (datapoint.excitedPort == 1) {
                m_data.s21.push_back(std::complex<double>(datapoint.real, datapoint.imag));
            } else if (datapoint.excitedPort == 2) {
                m_data.s22.push_back(std::complex<double>(datapoint.real, datapoint.imag));
            }
        }
    }
    
    m_receivedPoints++;
    
    // Progress indicator
    if (m_receivedPoints % 50 == 0) {
        std::cout << "Progress: " << m_receivedPoints << " / " << m_config.points 
                  << " (" << (100.0 * m_receivedPoints / m_config.points) << "%)" << std::endl;
    }
    
    // Check if sweep complete
    if (m_receivedPoints >= m_config.points) {
        m_sweepComplete = true;
        processMeasurement();
    }
}
```

## Phase 4: Calibration Integration

### 4.1 Implement loadCalibration()

```cpp
#include "Calibration/calibration.h"

bool Controller::loadCalibration()
{
    m_calibration = std::make_unique<Calibration>();
    
    QString calPath = QString::fromStdString(m_config.calibrationFile);
    
    if (!m_calibration->fromFile(calPath)) {
        std::cerr << "Failed to load calibration file" << std::endl;
        return false;
    }
    
    std::cout << "Calibration loaded successfully" << std::endl;
    std::cout << "  Type: " << m_calibration->getType() << std::endl;
    std::cout << "  Points: " << m_calibration->getNumPoints() << std::endl;
    
    return true;
}
```

### 4.2 Implement applyCalibration()

```cpp
void Controller::applyCalibration()
{
    if (!m_calibration) {
        std::cerr << "Warning: No calibration loaded" << std::endl;
        return;
    }
    
    // Apply calibration to each measurement point
    for (size_t i = 0; i < m_data.frequencies.size(); i++) {
        // Create VNAMeasurement structure
        DeviceDriver::VNAMeasurement raw;
        raw.frequency = m_data.frequencies[i];
        
        // Populate datapoints
        // S11
        DeviceDriver::VNAMeasurement::Datapoint dp_s11;
        dp_s11.port = 1;
        dp_s11.excitedPort = 1;
        dp_s11.real = m_data.s11[i].real();
        dp_s11.imag = m_data.s11[i].imag();
        raw.datapoints.push_back(dp_s11);
        
        // Similar for S21, S12, S22...
        
        // Apply calibration correction
        auto corrected = m_calibration->correctMeasurement(raw);
        
        // Update data with corrected values
        m_data.s11[i] = std::complex<double>(corrected.datapoints[0].real,
                                              corrected.datapoints[0].imag);
        // Similar for other parameters...
    }
    
    std::cout << "Calibration applied to " << m_data.frequencies.size() << " points" << std::endl;
}
```

## Phase 5: VNA Configuration & Sweep

### 5.1 Implement configureVNA()

```cpp
bool Controller::configureVNA()
{
    if (!m_driver || !m_connected) {
        return false;
    }
    
    // Build VNA settings
    DeviceDriver::VNASettings settings;
    settings.freqStart = m_config.freqStart;
    settings.freqStop = m_config.freqStop;
    settings.points = m_config.points;
    settings.IFBW = m_config.ifbw;
    settings.dBmStart = m_config.power;
    settings.dBmStop = m_config.power;
    settings.logSweep = m_config.logSweep;
    settings.dwellTime = m_config.dwellTime;
    
    // Set excited ports for full 2-port measurement
    settings.excitedPorts = {1, 2};
    
    // Check if segmentation is needed
    if (m_config.points > m_maxPoints) {
        std::cout << "Warning: Points (" << m_config.points 
                  << ") exceeds device max (" << m_maxPoints << ")" << std::endl;
        std::cout << "Sweep will be segmented" << std::endl;
        
        m_needsSegmentation = true;
        m_segmentCount = (m_config.points + m_maxPoints - 1) / m_maxPoints;
        return configureSegmentedSweep();
    }
    
    // Single sweep
    m_driver->setVNA(settings, [this](bool success) {
        if (!success) {
            std::cerr << "VNA configuration failed" << std::endl;
            cleanup(1);
        }
    });
    
    return true;
}
```

### 5.2 Implement Segmentation

```cpp
bool Controller::configureSegmentedSweep()
{
    m_segments.clear();
    
    double freqStep = (m_config.freqStop - m_config.freqStart) / (m_config.points - 1);
    int remainingPoints = m_config.points;
    int startPoint = 0;
    
    while (remainingPoints > 0) {
        int segmentPoints = std::min(remainingPoints, m_maxPoints);
        
        DeviceDriver::VNASettings segment;
        segment.freqStart = m_config.freqStart + startPoint * freqStep;
        segment.freqStop = m_config.freqStart + (startPoint + segmentPoints - 1) * freqStep;
        segment.points = segmentPoints;
        segment.IFBW = m_config.ifbw;
        segment.dBmStart = m_config.power;
        segment.dBmStop = m_config.power;
        segment.logSweep = m_config.logSweep;
        segment.dwellTime = m_config.dwellTime;
        segment.excitedPorts = {1, 2};
        
        m_segments.push_back(segment);
        
        remainingPoints -= segmentPoints;
        startPoint += segmentPoints;
    }
    
    std::cout << "Configured " << m_segments.size() << " segments" << std::endl;
    
    // Start first segment
    m_currentSegment = 0;
    return startNextSegment();
}

bool Controller::startNextSegment()
{
    if (m_currentSegment >= m_segments.size()) {
        // All segments complete
        return true;
    }
    
    std::cout << "Starting segment " << (m_currentSegment + 1) 
              << " / " << m_segments.size() << std::endl;
    
    m_driver->setVNA(m_segments[m_currentSegment], [this](bool success) {
        if (!success) {
            std::cerr << "Segment " << m_currentSegment << " failed" << std::endl;
            cleanup(1);
        }
    });
    
    return true;
}
```

## Phase 6: Pass/Fail Evaluation

### 6.1 Complete evaluateCriteria()

```cpp
EvaluationResult Controller::evaluateCriteria()
{
    EvaluationResult result;
    result.s11Pass = true;
    result.s21Pass = true;
    result.s12Pass = true;
    result.s22Pass = true;
    
    double maxS11 = -999.0;
    double maxS21 = -999.0;
    double maxS12 = -999.0;
    double maxS22 = -999.0;
    
    // Evaluate each parameter
    for (size_t i = 0; i < m_data.frequencies.size(); i++) {
        double s11_db = toDbMagnitude(m_data.s11[i]);
        double s21_db = toDbMagnitude(m_data.s21[i]);
        double s12_db = toDbMagnitude(m_data.s12[i]);
        double s22_db = toDbMagnitude(m_data.s22[i]);
        
        // Track max values
        maxS11 = std::max(maxS11, s11_db);
        maxS21 = std::max(maxS21, s21_db);
        maxS12 = std::max(maxS12, s12_db);
        maxS22 = std::max(maxS22, s22_db);
        
        // Check threshold
        if (s11_db > m_config.thresholdDb) result.s11Pass = false;
        if (s21_db > m_config.thresholdDb) result.s21Pass = false;
        if (s12_db > m_config.thresholdDb) result.s12Pass = false;
        if (s22_db > m_config.thresholdDb) result.s22Pass = false;
    }
    
    result.maxValues["S11"] = maxS11;
    result.maxValues["S21"] = maxS21;
    result.maxValues["S12"] = maxS12;
    result.maxValues["S22"] = maxS22;
    
    result.overallPass = result.s11Pass && result.s21Pass && 
                        result.s12Pass && result.s22Pass;
    
    return result;
}
```

## Phase 7: Output Generation

### 7.1 Complete JSON Output

```cpp
bool Controller::saveJSON(const EvaluationResult& result)
{
    QJsonObject root;
    
    // Configuration
    QJsonObject config;
    config["freqStart"] = m_config.freqStart;
    config["freqStop"] = m_config.freqStop;
    config["points"] = m_config.points;
    config["thresholdDb"] = m_config.thresholdDb;
    root["configuration"] = config;
    
    // Results
    QJsonObject results;
    results["overallPass"] = result.overallPass;
    
    QJsonArray params;
    for (const auto& param : {"S11", "S21", "S12", "S22"}) {
        QJsonObject p;
        p["parameter"] = QString(param);
        p["pass"] = (param == std::string("S11") ? result.s11Pass :
                    param == std::string("S21") ? result.s21Pass :
                    param == std::string("S12") ? result.s12Pass :
                    result.s22Pass);
        p["maxDb"] = result.maxValues.at(param);
        params.append(p);
    }
    results["parameters"] = params;
    root["results"] = results;
    
    // Data (optional, can be large)
    QJsonArray dataPoints;
    for (size_t i = 0; i < m_data.frequencies.size(); i++) {
        QJsonObject point;
        point["frequency"] = m_data.frequencies[i];
        point["s11_db"] = toDbMagnitude(m_data.s11[i]);
        point["s21_db"] = toDbMagnitude(m_data.s21[i]);
        point["s12_db"] = toDbMagnitude(m_data.s12[i]);
        point["s22_db"] = toDbMagnitude(m_data.s22[i]);
        dataPoints.append(point);
    }
    root["data"] = dataPoints;
    
    // Write to file
    QJsonDocument doc(root);
    QFile file("vna_results.json");
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    file.write(doc.toJson());
    file.close();
    
    return true;
}
```

### 7.2 Complete CSV Output

```cpp
bool Controller::saveCSV(const EvaluationResult& result)
{
    QFile file("vna_results.csv");
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    
    QTextStream out(&file);
    
    // Header
    out << "Frequency (Hz),S11 (dB),S21 (dB),S12 (dB),S22 (dB)\n";
    
    // Data
    for (size_t i = 0; i < m_data.frequencies.size(); i++) {
        out << QString::number(m_data.frequencies[i], 'f', 0) << ","
            << QString::number(toDbMagnitude(m_data.s11[i]), 'f', 3) << ","
            << QString::number(toDbMagnitude(m_data.s21[i]), 'f', 3) << ","
            << QString::number(toDbMagnitude(m_data.s12[i]), 'f', 3) << ","
            << QString::number(toDbMagnitude(m_data.s22[i]), 'f', 3) << "\n";
    }
    
    file.close();
    return true;
}
```

## Phase 8: Testing & Validation

### 8.1 Unit Tests

Create test cases for:
- Command-line parsing
- Calibration loading
- S-parameter conversion (dB)
- Threshold evaluation
- Segmentation logic

### 8.2 Integration Tests

Test with real hardware:
1. Connect to device, verify serial/info
2. Load known-good calibration file
3. Run sweep, verify data received
4. Compare with GUI results (same cal + settings)
5. Test segmentation with large point counts
6. Verify timeout handling

### 8.3 Validation Checklist

- [ ] Device connects reliably on Windows (WinUSB)
- [ ] Device connects reliably on Linux (libusb)
- [ ] Calibration file loads without errors
- [ ] S-parameters match GUI values (within tolerance)
- [ ] Segmentation works for > maxPoints
- [ ] Timeout triggers correctly
- [ ] Connection loss handled gracefully
- [ ] Exit codes correct (0=pass, 1=fail)
- [ ] JSON output valid and complete
- [ ] CSV output readable and correct

## Timeline Estimate

- **Phase 2** (Source Integration): 2-4 hours
- **Phase 3** (USB Integration): 4-6 hours
- **Phase 4** (Calibration): 3-5 hours
- **Phase 5** (VNA Config): 4-6 hours
- **Phase 6** (Evaluation): 2-3 hours
- **Phase 7** (Output): 2-3 hours
- **Phase 8** (Testing): 4-8 hours

**Total**: 21-35 hours

## Dependencies & Blockers

1. **LibreVNA source code**: Must vendor or submodule
2. **Real hardware**: Required for testing
3. **Calibration file**: Need valid .cal file for device
4. **WinUSB driver**: Windows testing requires proper driver

## Success Criteria Checklist

From problem statement:

- [ ] Reliably connect to device on Windows (WinUSB)
- [ ] Claim interface 0, run sweep, receive datapoints
- [ ] Load .cal file and apply calibration
- [ ] Produce correct S-parameter magnitudes (dB)
- [ ] Properly segment sweeps if points > maxPoints
- [ ] Handle timeouts and connection loss gracefully
- [ ] Clear exit status (0=pass, non-zero=fail)
- [ ] Concise report printed to stdout
- [ ] Reuse LibreVNA USB/TCP drivers
- [ ] Use Protocol.hpp (shared with firmware)
- [ ] Use Calibration engine from LibreVNA
- [ ] QtCore only (no GUI)

## Notes

- Current implementation provides complete **framework** and **interfaces**
- All placeholder code is clearly marked with `// TODO:` comments
- Architecture follows problem statement requirements exactly
- Once LibreVNA source is integrated, implementation can proceed systematically
- Each phase is independent and can be tested incrementally
