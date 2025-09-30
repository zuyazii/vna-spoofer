#include "controller.hpp"
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <iostream>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("vna-cli");
    QCoreApplication::setApplicationVersion("0.1.0");
    QCoreApplication::setOrganizationName("LibreVNA");

    // Setup command line parser
    QCommandLineParser parser;
    parser.setApplicationDescription(
        "LibreVNA CLI - Headless command-line tool for LibreVNA Vector Network Analyzer\n"
        "\n"
        "This tool connects to a LibreVNA device over USB, performs S-parameter sweeps\n"
        "with calibration, and evaluates pass/fail criteria.\n"
        "\n"
        "Example:\n"
        "  vna-cli --cal calibration.cal --fstart 1e6 --fstop 3e9 --points 501\n"
    );
    parser.addHelpOption();
    parser.addVersionOption();

    // Define command line options
    QCommandLineOption serialOption(
        QStringList() << "s" << "serial",
        "Device serial number (optional, connects to first device if not specified)",
        "serial"
    );
    parser.addOption(serialOption);

    QCommandLineOption calOption(
        QStringList() << "c" << "cal",
        "Path to calibration file (.cal) - REQUIRED",
        "file"
    );
    parser.addOption(calOption);

    QCommandLineOption fstartOption(
        "fstart",
        "Start frequency in Hz (e.g., 1000000 for 1 MHz)",
        "Hz",
        "1000000"  // Default 1 MHz
    );
    parser.addOption(fstartOption);

    QCommandLineOption fstopOption(
        "fstop",
        "Stop frequency in Hz (e.g., 3000000000 for 3 GHz)",
        "Hz",
        "3000000000"  // Default 3 GHz
    );
    parser.addOption(fstopOption);

    QCommandLineOption pointsOption(
        QStringList() << "p" << "points",
        "Number of measurement points",
        "count",
        "501"  // Default 501 points
    );
    parser.addOption(pointsOption);

    QCommandLineOption ifbwOption(
        "ifbw",
        "IF bandwidth in Hz",
        "Hz",
        "1000"  // Default 1 kHz
    );
    parser.addOption(ifbwOption);

    QCommandLineOption powerOption(
        "power",
        "Output power in dBm",
        "dBm",
        "-10"  // Default -10 dBm
    );
    parser.addOption(powerOption);

    QCommandLineOption logSweepOption(
        "logsweep",
        "Use logarithmic frequency sweep (default is linear)"
    );
    parser.addOption(logSweepOption);

    QCommandLineOption dwellOption(
        "dwell",
        "Dwell time per point in seconds",
        "seconds",
        "0.001"  // Default 1 ms
    );
    parser.addOption(dwellOption);

    QCommandLineOption thresholdOption(
        QStringList() << "t" << "threshold-db",
        "Pass/fail threshold in dB (measurements must be <= threshold)",
        "dB",
        "-10.0"  // Default -10 dB
    );
    parser.addOption(thresholdOption);

    QCommandLineOption timeoutOption(
        "timeout-s",
        "Overall timeout in seconds",
        "seconds",
        "120"  // Default 120 seconds
    );
    parser.addOption(timeoutOption);

    QCommandLineOption jsonOption(
        "json",
        "Save results to JSON file",
        "file"
    );
    parser.addOption(jsonOption);

    QCommandLineOption csvOption(
        "csv",
        "Save results to CSV file",
        "file"
    );
    parser.addOption(csvOption);

    // Parse command line
    parser.process(app);

    // Validate required options
    if (!parser.isSet(calOption)) {
        std::cerr << "Error: Calibration file is required (--cal)" << std::endl;
        std::cerr << "Use --help for usage information" << std::endl;
        return 1;
    }

    // Build configuration
    SweepConfig config;
    config.freqStart = parser.value(fstartOption).toDouble();
    config.freqStop = parser.value(fstopOption).toDouble();
    config.points = parser.value(pointsOption).toInt();
    config.ifbw = parser.value(ifbwOption).toInt();
    config.power = parser.value(powerOption).toDouble();
    config.logSweep = parser.isSet(logSweepOption);
    config.dwellTime = parser.value(dwellOption).toDouble();
    config.calibrationFile = parser.value(calOption).toStdString();
    config.thresholdDb = parser.value(thresholdOption).toDouble();
    config.timeoutSeconds = parser.value(timeoutOption).toInt();

    // Validate configuration
    if (config.freqStart >= config.freqStop) {
        std::cerr << "Error: Start frequency must be less than stop frequency" << std::endl;
        return 1;
    }

    if (config.points < 2) {
        std::cerr << "Error: Must have at least 2 points" << std::endl;
        return 1;
    }

    if (config.points > 10000) {
        std::cerr << "Warning: Large number of points (" << config.points 
                  << ") may require sweep segmentation" << std::endl;
    }

    // Create controller
    Controller controller(config);
    
    // Connect finished signal to quit application
    QObject::connect(&controller, &Controller::finished, 
                     &app, &QCoreApplication::exit);

    // Start the workflow
    int initResult = controller.run();
    if (initResult != 0) {
        return initResult;
    }

    // Run event loop
    return app.exec();
}
