#include "controller.hpp"
#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <cmath>
#include <iostream>
#include <iomanip>

Controller::Controller(const SweepConfig& config, QObject* parent)
    : QObject(parent)
    , m_config(config)
    , m_connected(false)
    , m_sweepComplete(false)
    , m_receivedPoints(0)
{
    // Setup timeout timer
    m_timeoutTimer = new QTimer(this);
    m_timeoutTimer->setSingleShot(true);
    connect(m_timeoutTimer, &QTimer::timeout, this, &Controller::onTimeout);
}

Controller::~Controller()
{
    cleanup(0);
}

int Controller::run()
{
    std::cout << "=== LibreVNA CLI Tool ===" << std::endl;
    std::cout << "Frequency range: " << m_config.freqStart / 1e6 << " - " 
              << m_config.freqStop / 1e6 << " MHz" << std::endl;
    std::cout << "Points: " << m_config.points << std::endl;
    std::cout << "IFBW: " << m_config.ifbw << " Hz" << std::endl;
    std::cout << "Power: " << m_config.power << " dBm" << std::endl;
    std::cout << "Threshold: " << m_config.thresholdDb << " dB" << std::endl;
    std::cout << std::endl;

    // Start timeout timer
    m_timeoutTimer->start(m_config.timeoutSeconds * 1000);

    // Connect to device
    if (!connectToDevice()) {
        std::cerr << "Error: Failed to connect to device" << std::endl;
        cleanup(1);
        return 1;
    }

    // Load calibration
    if (!loadCalibration()) {
        std::cerr << "Error: Failed to load calibration file" << std::endl;
        cleanup(1);
        return 1;
    }

    // Configure VNA
    if (!configureVNA()) {
        std::cerr << "Error: Failed to configure VNA" << std::endl;
        cleanup(1);
        return 1;
    }

    // Start sweep
    if (!startSweep()) {
        std::cerr << "Error: Failed to start sweep" << std::endl;
        cleanup(1);
        return 1;
    }

    // Event loop will handle the rest
    // Measurements will come in via signals/slots
    return 0;
}

bool Controller::connectToDevice()
{
    std::cout << "Connecting to LibreVNA device..." << std::endl;
    
    // TODO: Implement actual USB device connection using LibreVNAUSBDriver
    // For now, this is a placeholder implementation
    
    // Steps:
    // 1. Create LibreVNAUSBDriver instance
    // 2. Connect signals (InfoUpdated, ConnectionLost, VNAmeasurementReceived)
    // 3. Call driver->connectTo(serial) or connectToFirstAvailable()
    // 4. Wait for connection confirmation
    // 5. Request DeviceInfo and Status
    
    std::cout << "Note: USB driver integration pending - placeholder implementation" << std::endl;
    std::cout << "This requires LibreVNA C++ source code to be vendored/submoduled" << std::endl;
    
    m_connected = false;
    return false;  // Return false until actual implementation
}

bool Controller::loadCalibration()
{
    std::cout << "Loading calibration file: " << m_config.calibrationFile << std::endl;
    
    // TODO: Implement calibration loading using Calibration::fromFile
    // For now, this is a placeholder
    
    // Steps:
    // 1. Create Calibration instance
    // 2. Call calibration->fromFile(path)
    // 3. Validate calibration data
    // 4. Store for later use in correctMeasurement()
    
    QFile file(QString::fromStdString(m_config.calibrationFile));
    if (!file.exists()) {
        std::cerr << "Calibration file not found: " << m_config.calibrationFile << std::endl;
        return false;
    }
    
    std::cout << "Note: Calibration loading pending - placeholder implementation" << std::endl;
    return false;  // Return false until actual implementation
}

bool Controller::configureVNA()
{
    std::cout << "Configuring VNA..." << std::endl;
    
    // TODO: Implement VNA configuration using LibreVNADriver::setVNA
    // For now, this is a placeholder
    
    // Steps:
    // 1. Build VNASettings structure:
    //    - freqStart, freqStop, points
    //    - IFBW, power, logSweep, dwellTime
    //    - excitedPorts = {1, 2} for full 2-port measurement
    // 2. Check if points > device maxPoints
    // 3. If so, segment the sweep
    // 4. Call driver->setVNA(settings, callback)
    
    std::cout << "Note: VNA configuration pending - placeholder implementation" << std::endl;
    return false;  // Return false until actual implementation
}

bool Controller::startSweep()
{
    std::cout << "Starting VNA sweep..." << std::endl;
    
    // TODO: Implement sweep initiation
    // The actual sweep starts when setVNA is called
    // Measurements will arrive via VNAmeasurementReceived signal
    
    std::cout << "Note: Sweep start pending - placeholder implementation" << std::endl;
    return false;  // Return false until actual implementation
}

void Controller::onInfoUpdated()
{
    std::cout << "Device info updated" << std::endl;
}

void Controller::onConnectionLost()
{
    std::cerr << "Error: Connection to device lost" << std::endl;
    cleanup(1);
}

void Controller::onVNAMeasurementReceived()
{
    std::cout << "VNA measurement received (point " << m_receivedPoints + 1 
              << " of " << m_config.points << ")" << std::endl;
    
    // TODO: Process received measurement
    // 1. Extract S11, S21, S12, S22 data
    // 2. Apply calibration correction
    // 3. Store in m_data vectors
    // 4. Increment m_receivedPoints
    // 5. Check if sweep is complete
    
    m_receivedPoints++;
    
    if (m_receivedPoints >= m_config.points) {
        m_sweepComplete = true;
        processMeasurement();
    }
}

void Controller::onTimeout()
{
    std::cerr << "Error: Operation timed out" << std::endl;
    cleanup(1);
}

void Controller::processMeasurement()
{
    std::cout << "Processing measurements..." << std::endl;
    
    // Apply calibration
    applyCalibration();
    
    // Evaluate pass/fail
    EvaluationResult result = evaluateCriteria();
    
    // Print report
    printReport(result);
    
    // Save outputs if requested
    // saveJSON(result);
    // saveCSV(result);
    
    // Exit with appropriate code
    int exitCode = result.overallPass ? 0 : 1;
    cleanup(exitCode);
}

void Controller::applyCalibration()
{
    std::cout << "Applying calibration..." << std::endl;
    
    // TODO: Apply calibration to all measurement points
    // For each point:
    //   corrected = m_calibration->correctMeasurement(raw)
}

EvaluationResult Controller::evaluateCriteria()
{
    std::cout << "Evaluating pass/fail criteria..." << std::endl;
    
    EvaluationResult result;
    result.s11Pass = true;
    result.s21Pass = true;
    result.s12Pass = true;
    result.s22Pass = true;
    result.overallPass = true;
    
    // TODO: Implement actual evaluation
    // For each S-parameter:
    //   Convert to dB: 20*log10(|Sij|)
    //   Check if all points <= threshold
    //   Track max value
    
    result.maxValues["S11"] = 0.0;
    result.maxValues["S21"] = 0.0;
    result.maxValues["S12"] = 0.0;
    result.maxValues["S22"] = 0.0;
    
    return result;
}

void Controller::printReport(const EvaluationResult& result)
{
    std::cout << std::endl;
    std::cout << "=== Test Results ===" << std::endl;
    std::cout << std::fixed << std::setprecision(2);
    
    std::cout << "S11: " << (result.s11Pass ? "PASS" : "FAIL") 
              << " (max: " << result.maxValues.at("S11") << " dB)" << std::endl;
    std::cout << "S21: " << (result.s21Pass ? "PASS" : "FAIL") 
              << " (max: " << result.maxValues.at("S21") << " dB)" << std::endl;
    std::cout << "S12: " << (result.s12Pass ? "PASS" : "FAIL") 
              << " (max: " << result.maxValues.at("S12") << " dB)" << std::endl;
    std::cout << "S22: " << (result.s22Pass ? "PASS" : "FAIL") 
              << " (max: " << result.maxValues.at("S22") << " dB)" << std::endl;
    
    std::cout << std::endl;
    std::cout << "Overall: " << (result.overallPass ? "PASS" : "FAIL") << std::endl;
}

bool Controller::saveJSON(const EvaluationResult& result)
{
    // TODO: Implement JSON output
    QJsonObject root;
    root["overallPass"] = result.overallPass;
    
    QJsonObject params;
    params["S11"] = QJsonObject{{"pass", result.s11Pass}, {"maxDb", result.maxValues.at("S11")}};
    params["S21"] = QJsonObject{{"pass", result.s21Pass}, {"maxDb", result.maxValues.at("S21")}};
    params["S12"] = QJsonObject{{"pass", result.s12Pass}, {"maxDb", result.maxValues.at("S12")}};
    params["S22"] = QJsonObject{{"pass", result.s22Pass}, {"maxDb", result.maxValues.at("S22")}};
    root["parameters"] = params;
    
    QJsonDocument doc(root);
    
    QFile file("vna_results.json");
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    
    file.write(doc.toJson());
    file.close();
    
    return true;
}

bool Controller::saveCSV(const EvaluationResult& result)
{
    // TODO: Implement CSV output
    return true;
}

double Controller::toDbMagnitude(const std::complex<double>& value)
{
    return 20.0 * std::log10(std::abs(value));
}

void Controller::cleanup(int exitCode)
{
    std::cout << "Cleaning up..." << std::endl;
    
    // Stop timer
    if (m_timeoutTimer) {
        m_timeoutTimer->stop();
    }
    
    // Disconnect from device
    // TODO: Call driver->disconnect()
    
    emit finished(exitCode);
}
