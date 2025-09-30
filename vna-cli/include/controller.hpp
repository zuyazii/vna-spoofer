#ifndef CONTROLLER_HPP
#define CONTROLLER_HPP

#include <QObject>
#include <QString>
#include <QCoreApplication>
#include <QTimer>
#include <vector>
#include <complex>
#include <map>
#include <memory>
#include <functional>

/**
 * @brief Configuration for VNA sweep
 */
struct SweepConfig {
    double freqStart;           // Hz
    double freqStop;            // Hz
    int points;                 // Number of points
    int ifbw;                   // IF bandwidth (Hz)
    double power;               // Power level (dBm)
    bool logSweep;              // Logarithmic frequency sweep
    double dwellTime;           // Dwell time per point (seconds)
    std::string calibrationFile; // Path to .cal file
    double thresholdDb;         // Pass/fail threshold (dB)
    int timeoutSeconds;         // Overall timeout
};

/**
 * @brief S-parameter measurement data
 */
struct SParameterData {
    std::vector<double> frequencies;
    std::vector<std::complex<double>> s11;
    std::vector<std::complex<double>> s21;
    std::vector<std::complex<double>> s12;
    std::vector<std::complex<double>> s22;
};

/**
 * @brief Pass/fail result for S-parameter evaluation
 */
struct EvaluationResult {
    bool s11Pass;
    bool s21Pass;
    bool s12Pass;
    bool s22Pass;
    bool overallPass;
    std::map<std::string, double> maxValues;  // Max dB values for each parameter
};

/**
 * @brief Main controller for VNA CLI operations
 * 
 * This class orchestrates the complete measurement workflow:
 * 1. Connect to LibreVNA device over USB
 * 2. Load calibration file
 * 3. Configure and execute VNA sweep
 * 4. Apply calibration to measurements
 * 5. Evaluate pass/fail criteria
 * 6. Generate reports and output files
 */
class Controller : public QObject {
    Q_OBJECT

public:
    explicit Controller(const SweepConfig& config, QObject* parent = nullptr);
    ~Controller();

    /**
     * @brief Start the VNA measurement workflow
     * @return Exit code (0 = success, non-zero = failure)
     */
    int run();

signals:
    /**
     * @brief Emitted when operation is complete
     * @param exitCode Exit code for the application
     */
    void finished(int exitCode);

private slots:
    /**
     * @brief Handle device info update
     */
    void onInfoUpdated();

    /**
     * @brief Handle device connection loss
     */
    void onConnectionLost();

    /**
     * @brief Handle VNA measurement received
     */
    void onVNAMeasurementReceived();

    /**
     * @brief Handle timeout
     */
    void onTimeout();

private:
    /**
     * @brief Connect to LibreVNA device
     * @return true if successful
     */
    bool connectToDevice();

    /**
     * @brief Load calibration file
     * @return true if successful
     */
    bool loadCalibration();

    /**
     * @brief Configure VNA for sweep
     * @return true if successful
     */
    bool configureVNA();

    /**
     * @brief Start VNA sweep
     * @return true if successful
     */
    bool startSweep();

    /**
     * @brief Process received measurement data
     */
    void processMeasurement();

    /**
     * @brief Apply calibration to raw measurements
     */
    void applyCalibration();

    /**
     * @brief Evaluate pass/fail criteria
     * @return Evaluation result
     */
    EvaluationResult evaluateCriteria();

    /**
     * @brief Print summary report
     */
    void printReport(const EvaluationResult& result);

    /**
     * @brief Save results to JSON file
     */
    bool saveJSON(const EvaluationResult& result);

    /**
     * @brief Save results to CSV file
     */
    bool saveCSV(const EvaluationResult& result);

    /**
     * @brief Convert complex value to dB magnitude
     */
    double toDbMagnitude(const std::complex<double>& value);

    /**
     * @brief Cleanup and exit
     */
    void cleanup(int exitCode);

private:
    SweepConfig m_config;
    SParameterData m_data;
    QTimer* m_timeoutTimer;
    
    // Device driver and calibration objects would be added here
    // For now, we'll keep the interface minimal
    // std::unique_ptr<LibreVNAUSBDriver> m_driver;
    // std::unique_ptr<Calibration> m_calibration;
    
    bool m_connected;
    bool m_sweepComplete;
    int m_receivedPoints;
};

#endif // CONTROLLER_HPP
