#include "MainView.hpp"

#include <QBoxLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QMessageBox>
#include <QTimer>
#include <QFileDialog>
#include <QDir>
#include <QCoreApplication>
#include <QMetaObject>
#include <QFont>
#include <QStyle>

#include <cmath>
#include <complex>
#include <filesystem>

namespace ui::views {

namespace fs = std::filesystem;

namespace {

constexpr double kFloorMagnitudeDb = -300.0;

fs::path ensureCalibrationStorageRoot() {
    try {
        const fs::path appDir =
            fs::path(QCoreApplication::applicationDirPath().toStdString());
        if (appDir.empty()) {
            return {};
        }
        const fs::path calibrationDir = appDir / "Calibration";
        std::error_code ec;
        fs::create_directories(calibrationDir, ec);
        if (ec && !fs::exists(calibrationDir)) {
            return {};
        }
        const auto canonical = fs::weakly_canonical(calibrationDir, ec);
        return ec ? calibrationDir : canonical;
    } catch (...) {
        return {};
    }
}

} // namespace

MainView::MainView(QWidget* parent)
    : QWidget(parent) {
    setObjectName(QStringLiteral("AppRoot"));
    setAttribute(Qt::WA_StyledBackground, true);

    buildUi();
    setupConnections();

    m_calibrationDirectory = findCalibrationDirectory();
    refreshCalibrationList();
    QTimer::singleShot(0, this, &MainView::scanForDevices);
}

MainView::~MainView() {
    m_cancelRequested = true;
    ensureSweepThreadFinished();
    if (m_hostCore.is_connected()) {
        m_hostCore.disconnect();
    }
}

void MainView::buildUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(40, 40, 40, 40);
    mainLayout->setSpacing(20);

    // Status label at top
    m_statusLabel = new QLabel(tr("Initializing..."), this);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setFixedHeight(40);
    m_statusLabel->setStyleSheet(QStringLiteral(
        "font-size: 18px; font-weight: bold; color: #888; "
        "background-color: #2a2a2a; border-radius: 8px; padding: 8px;"));
    mainLayout->addWidget(m_statusLabel);

    // Result labels container - takes most of the space
    auto* labelsContainer = new QWidget(this);
    labelsContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto* labelsLayout = new QHBoxLayout(labelsContainer);
    labelsLayout->setContentsMargins(0, 0, 0, 0);
    labelsLayout->setSpacing(40);

    // S11 Label
    auto* s11Container = new QWidget(labelsContainer);
    s11Container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto* s11Layout = new QVBoxLayout(s11Container);
    s11Layout->setContentsMargins(0, 0, 0, 0);
    s11Layout->setSpacing(10);

    auto* s11Title = new QLabel(QStringLiteral("S11"), s11Container);
    s11Title->setAlignment(Qt::AlignCenter);
    s11Title->setStyleSheet(QStringLiteral("font-size: 48px; font-weight: bold; color: #666;"));
    s11Layout->addWidget(s11Title);

    m_s11Label = new QLabel(tr("PENDING"), s11Container);
    m_s11Label->setAlignment(Qt::AlignCenter);
    m_s11Label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_s11Label->setStyleSheet(QStringLiteral(
        "font-size: 120px; font-weight: bold; color: #888; "
        "background-color: #2a2a2a; border-radius: 20px; padding: 40px;"));
    m_s11Label->setProperty("resultState", "pending");
    s11Layout->addWidget(m_s11Label, 1);

    labelsLayout->addWidget(s11Container, 1);

    // S22 Label
    auto* s22Container = new QWidget(labelsContainer);
    s22Container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto* s22Layout = new QVBoxLayout(s22Container);
    s22Layout->setContentsMargins(0, 0, 0, 0);
    s22Layout->setSpacing(10);

    auto* s22Title = new QLabel(QStringLiteral("S22"), s22Container);
    s22Title->setAlignment(Qt::AlignCenter);
    s22Title->setStyleSheet(QStringLiteral("font-size: 48px; font-weight: bold; color: #666;"));
    s22Layout->addWidget(s22Title);

    m_s22Label = new QLabel(tr("PENDING"), s22Container);
    m_s22Label->setAlignment(Qt::AlignCenter);
    m_s22Label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_s22Label->setStyleSheet(QStringLiteral(
        "font-size: 120px; font-weight: bold; color: #888; "
        "background-color: #2a2a2a; border-radius: 20px; padding: 40px;"));
    m_s22Label->setProperty("resultState", "pending");
    s22Layout->addWidget(m_s22Label, 1);

    labelsLayout->addWidget(s22Container, 1);

    mainLayout->addWidget(labelsContainer, 1);

    // Button row
    auto* buttonContainer = new QWidget(this);
    buttonContainer->setFixedHeight(80);
    auto* buttonLayout = new QHBoxLayout(buttonContainer);
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->setSpacing(20);

    buttonLayout->addStretch(1);

    m_calButton = new QPushButton(tr("LOAD CAL"), buttonContainer);
    m_calButton->setMinimumSize(200, 60);
    m_calButton->setStyleSheet(QStringLiteral(
        "QPushButton { font-size: 24px; font-weight: bold; background-color: #FF9800; "
        "color: white; border: none; border-radius: 10px; padding: 10px 30px; }"
        "QPushButton:hover { background-color: #F57C00; }"
        "QPushButton:disabled { background-color: #666; color: #999; }"));
    buttonLayout->addWidget(m_calButton);

    m_startButton = new QPushButton(tr("START"), buttonContainer);
    m_startButton->setMinimumSize(200, 60);
    m_startButton->setStyleSheet(QStringLiteral(
        "QPushButton { font-size: 24px; font-weight: bold; background-color: #4CAF50; "
        "color: white; border: none; border-radius: 10px; padding: 10px 30px; }"
        "QPushButton:hover { background-color: #45a049; }"
        "QPushButton:disabled { background-color: #666; color: #999; }"));
    m_startButton->setEnabled(false);
    buttonLayout->addWidget(m_startButton);

    m_stopButton = new QPushButton(tr("STOP"), buttonContainer);
    m_stopButton->setMinimumSize(200, 60);
    m_stopButton->setStyleSheet(QStringLiteral(
        "QPushButton { font-size: 24px; font-weight: bold; background-color: #f44336; "
        "color: white; border: none; border-radius: 10px; padding: 10px 30px; }"
        "QPushButton:hover { background-color: #da190b; }"
        "QPushButton:disabled { background-color: #666; color: #999; }"));
    m_stopButton->setEnabled(false);
    buttonLayout->addWidget(m_stopButton);

    m_resetButton = new QPushButton(tr("RESET"), buttonContainer);
    m_resetButton->setMinimumSize(200, 60);
    m_resetButton->setStyleSheet(QStringLiteral(
        "QPushButton { font-size: 24px; font-weight: bold; background-color: #2196F3; "
        "color: white; border: none; border-radius: 10px; padding: 10px 30px; }"
        "QPushButton:hover { background-color: #1976D2; }"
        "QPushButton:disabled { background-color: #666; color: #999; }"));
    buttonLayout->addWidget(m_resetButton);

    buttonLayout->addStretch(1);

    mainLayout->addWidget(buttonContainer);
}

void MainView::setupConnections() {
    connect(m_calButton, &QPushButton::clicked, this, &MainView::loadCalibrationFromFile);
    connect(m_startButton, &QPushButton::clicked, this, &MainView::startSweep);
    connect(m_stopButton, &QPushButton::clicked, this, &MainView::stopSweep);
    connect(m_resetButton, &QPushButton::clicked, this, &MainView::resetSweep);
}

void MainView::scanForDevices() {
    m_statusLabel->setText(tr("Scanning for VNA devices..."));
    m_statusLabel->setStyleSheet(QStringLiteral(
        "font-size: 18px; font-weight: bold; color: #888; "
        "background-color: #2a2a2a; border-radius: 8px; padding: 8px;"));

    std::string errorMessage;
    auto devices = librevna::headless::discover_devices(errorMessage);

    m_discoveredDevices = std::move(devices);

    if (m_discoveredDevices.empty()) {
        m_selectedDeviceIndex = -1;
        m_statusLabel->setText(tr("No VNA Found - Please connect a LibreVNA device"));
        m_statusLabel->setStyleSheet(QStringLiteral(
            "font-size: 18px; font-weight: bold; color: #f44336; "
            "background-color: #2a2a2a; border-radius: 8px; padding: 8px;"));
        m_calButton->setEnabled(false);
        m_startButton->setEnabled(false);
        showWarning(tr("No VNA Found"),
                    tr("No LibreVNA device detected.\n\nPlease connect a LibreVNA and restart the application."));
    } else {
        m_selectedDeviceIndex = 0;
        connectToDevice(0);
    }
}

void MainView::connectToDevice(int index) {
    if (index < 0 || index >= static_cast<int>(m_discoveredDevices.size())) {
        return;
    }

    ensureSweepThreadFinished();

    const auto& device = m_discoveredDevices[static_cast<std::size_t>(index)];
    const QString label = QString::fromStdString(device.label.empty() ? std::string("LibreVNA") : device.label);
    const QString serial = device.serial.empty() ? tr("<no-serial>")
                                                 : QString::fromStdString(device.serial);

    if (m_hostCore.is_connected()) {
        m_hostCore.disconnect();
    }

    if (!m_hostCore.connect(device.serial)) {
        const QString message = QString::fromStdString(m_hostCore.last_error_message());
        m_statusLabel->setText(tr("VNA Connection Failed"));
        m_statusLabel->setStyleSheet(QStringLiteral(
            "font-size: 18px; font-weight: bold; color: #f44336; "
            "background-color: #2a2a2a; border-radius: 8px; padding: 8px;"));
        m_calButton->setEnabled(false);
        m_startButton->setEnabled(false);
        showWarning(tr("Connection Failed"),
                    tr("Unable to connect to %1 (%2).\n%3").arg(label, serial, message));
        return;
    }

    m_connectedSerial = serial;
    m_statusLabel->setText(tr("VNA Connected - Load a calibration file to continue"));
    m_statusLabel->setStyleSheet(QStringLiteral(
        "font-size: 18px; font-weight: bold; color: #FF9800; "
        "background-color: #2a2a2a; border-radius: 8px; padding: 8px;"));
    m_calButton->setEnabled(true);
    m_startButton->setEnabled(false);
}

void MainView::loadCalibrationFromFile() {
    QString startDir;
    if (!m_calibrationDirectory.empty()) {
        startDir = QString::fromStdString(m_calibrationDirectory.u8string());
    } else {
        startDir = QDir::currentPath();
    }

    const QString selected = QFileDialog::getOpenFileName(
        this,
        tr("Select Calibration File"),
        startDir,
        tr("Calibration Files (*.cal);;All Files (*.*)"));

    if (selected.isEmpty()) {
        return;
    }

    applyCalibrationFromPath(selected);
}

void MainView::applyCalibrationFromPath(const QString& path) {
    if (path.isEmpty()) {
        return;
    }

    const fs::path calPath = fs::path(path.toStdString());
    if (!fs::exists(calPath)) {
        showWarning(tr("Calibration"), tr("The selected calibration file no longer exists."));
        return;
    }

    if (!m_hostCore.load_calibration(calPath)) {
        const QString message = QString::fromStdString(m_hostCore.last_error_message());
        showWarning(tr("Calibration Failed"),
                    tr("Unable to load calibration file.\n%1").arg(message));
        return;
    }

    m_activeCalibrationPath = calPath;

    // Update status to show ready state
    const QString calName = QString::fromStdString(calPath.filename().string());
    m_statusLabel->setText(tr("Ready - Cal: %1").arg(calName));
    m_statusLabel->setStyleSheet(QStringLiteral(
        "font-size: 18px; font-weight: bold; color: #4CAF50; "
        "background-color: #2a2a2a; border-radius: 8px; padding: 8px;"));
    m_startButton->setEnabled(true);
}

void MainView::refreshCalibrationList() {
    m_calibrationFiles.clear();

    if (m_calibrationDirectory.empty() || !fs::exists(m_calibrationDirectory)) {
        return;
    }

    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(m_calibrationDirectory, ec)) {
        if (ec) {
            break;
        }
        if (!entry.is_regular_file()) {
            continue;
        }
        const auto extension = QString::fromStdString(entry.path().extension().string()).toLower();
        if (extension != QStringLiteral(".cal")) {
            continue;
        }
        m_calibrationFiles.push_back(entry.path());
    }

    std::sort(m_calibrationFiles.begin(), m_calibrationFiles.end());
}

void MainView::ensureSweepThreadFinished() {
    if (m_sweepThread.joinable()) {
        if (m_sweepThread.get_id() == std::this_thread::get_id()) {
            m_sweepThread.detach();
        } else {
            m_sweepThread.join();
        }
    }
    m_sweepThread = std::thread();
    m_sweepInProgress = false;
}

void MainView::startSweep() {
    if (m_sweepInProgress) {
        return;
    }

    if (!m_hostCore.is_connected()) {
        return;
    }

    if (m_activeCalibrationPath.empty()) {
        return;
    }

    librevna::headless::SweepConfiguration configuration;
    configuration.start_frequency_hz = kDemoStartGHz * 1e9;
    configuration.stop_frequency_hz = kDemoStopGHz * 1e9;
    configuration.points = static_cast<std::uint32_t>(kDemoPoints);
    configuration.if_bandwidth_hz = 1000.0;
    configuration.power_dbm = 0.0;
    configuration.timeout_ms = 15000.0;
    configuration.excited_ports = {1, 2};

    m_cancelRequested = false;
    m_sweepInProgress = true;
    updateSweepControlState(true);
    resetResultStates();

    // Update status to show sweep in progress
    m_statusLabel->setText(tr("Sweep in progress..."));
    m_statusLabel->setStyleSheet(QStringLiteral(
        "font-size: 18px; font-weight: bold; color: #2196F3; "
        "background-color: #2a2a2a; border-radius: 8px; padding: 8px;"));

    const QString calName = QString::fromStdString(m_activeCalibrationPath.filename().string());

    m_sweepThread = std::thread([this, configuration, calName]() mutable {
        auto results = m_hostCore.run_sweep(configuration);
        const std::string lastError = m_hostCore.last_error_message();
        const bool cancelled = m_cancelRequested.load();
        const std::size_t resultCount = results.size();

        // Evaluate S11 and S22
        bool s11Pass = true;
        bool s22Pass = true;
        bool s11HasData = false;
        bool s22HasData = false;
        double s11WorstDb = kFloorMagnitudeDb;
        double s22WorstDb = kFloorMagnitudeDb;

        for (const auto& measurement : results) {
            // Check S11
            auto s11It = measurement.parameters.find("S11");
            if (s11It != measurement.parameters.end()) {
                s11HasData = true;
                const std::complex<double> value = s11It->second;
                const double magnitude = std::abs(value);
                const double magnitudeDb =
                    magnitude <= 0.0 ? kFloorMagnitudeDb : 20.0 * std::log10(magnitude);
                if (magnitudeDb > s11WorstDb) {
                    s11WorstDb = magnitudeDb;
                }
                if (magnitudeDb > kDemoThresholdDb) {
                    s11Pass = false;
                }
            }

            // Check S22
            auto s22It = measurement.parameters.find("S22");
            if (s22It != measurement.parameters.end()) {
                s22HasData = true;
                const std::complex<double> value = s22It->second;
                const double magnitude = std::abs(value);
                const double magnitudeDb =
                    magnitude <= 0.0 ? kFloorMagnitudeDb : 20.0 * std::log10(magnitude);
                if (magnitudeDb > s22WorstDb) {
                    s22WorstDb = magnitudeDb;
                }
                if (magnitudeDb > kDemoThresholdDb) {
                    s22Pass = false;
                }
            }
        }

        QMetaObject::invokeMethod(
            this,
            [this, cancelled, lastError, resultCount, s11Pass, s22Pass, s11HasData, s22HasData, calName]() {
                if (m_sweepThread.joinable()) {
                    m_sweepThread.join();
                    m_sweepThread = std::thread();
                }

                m_sweepInProgress = false;
                m_cancelRequested = false;
                updateSweepControlState(false);

                // Restore status to ready state
                m_statusLabel->setText(tr("Ready - Cal: %1").arg(calName));
                m_statusLabel->setStyleSheet(QStringLiteral(
                    "font-size: 18px; font-weight: bold; color: #4CAF50; "
                    "background-color: #2a2a2a; border-radius: 8px; padding: 8px;"));

                if (cancelled) {
                    resetResultStates();
                    return;
                }

                if (resultCount == 0) {
                    resetResultStates();
                    const QString message = lastError.empty()
                                                ? tr("No data returned from the sweep.")
                                                : QString::fromStdString(lastError);
                    showWarning(tr("Sweep"), message);
                    return;
                }

                setResultState(QStringLiteral("S11"), s11Pass, s11HasData);
                setResultState(QStringLiteral("S22"), s22Pass, s22HasData);
            },
            Qt::QueuedConnection);
    });
}

void MainView::stopSweep() {
    if (!m_sweepInProgress) {
        return;
    }
    m_cancelRequested = true;
}

void MainView::resetSweep() {
    if (m_sweepInProgress) {
        return;
    }
    resetResultStates();
}

std::filesystem::path MainView::findCalibrationDirectory() const {
    if (const fs::path preferred = ensureCalibrationStorageRoot(); !preferred.empty()) {
        return preferred;
    }

    std::vector<fs::path> candidates;

    try {
        const fs::path appDir = fs::path(QCoreApplication::applicationDirPath().toStdString());
        candidates.push_back(appDir / "Calibration");
        candidates.push_back(appDir / "calibration");
        candidates.push_back(appDir.parent_path() / "calibration");
    } catch (...) {
    }

    candidates.push_back(fs::current_path() / "Calibration");
    candidates.push_back(fs::current_path() / "calibration");

    for (const auto& candidate : candidates) {
        if (!candidate.empty() && fs::exists(candidate) && fs::is_directory(candidate)) {
            std::error_code ec;
            const auto canonical = fs::weakly_canonical(candidate, ec);
            return ec ? candidate : canonical;
        }
    }

    return {};
}

void MainView::updateSweepControlState(bool running) {
    m_calButton->setEnabled(!running);
    m_startButton->setEnabled(!running);
    m_stopButton->setEnabled(running);
    m_resetButton->setEnabled(!running);
}

void MainView::showInformation(const QString& title, const QString& message) {
    QMessageBox::information(this, title, message);
}

void MainView::showWarning(const QString& title, const QString& message) {
    QMessageBox::warning(this, title, message);
}

void MainView::setResultState(const QString& name, bool passed, bool hasData) {
    QLabel* label = nullptr;
    if (name == QStringLiteral("S11")) {
        label = m_s11Label;
    } else if (name == QStringLiteral("S22")) {
        label = m_s22Label;
    }

    if (!label) {
        return;
    }

    if (!hasData) {
        label->setText(tr("NO DATA"));
        label->setStyleSheet(QStringLiteral(
            "font-size: 120px; font-weight: bold; color: #888; "
            "background-color: #2a2a2a; border-radius: 20px; padding: 40px;"));
    } else if (passed) {
        label->setText(tr("PASS"));
        label->setStyleSheet(QStringLiteral(
            "font-size: 120px; font-weight: bold; color: white; "
            "background-color: #4CAF50; border-radius: 20px; padding: 40px;"));
    } else {
        label->setText(tr("FAIL"));
        label->setStyleSheet(QStringLiteral(
            "font-size: 120px; font-weight: bold; color: white; "
            "background-color: #f44336; border-radius: 20px; padding: 40px;"));
    }
}

void MainView::resetResultStates() {
    m_s11Label->setText(tr("PENDING"));
    m_s11Label->setStyleSheet(QStringLiteral(
        "font-size: 120px; font-weight: bold; color: #888; "
        "background-color: #2a2a2a; border-radius: 20px; padding: 40px;"));

    m_s22Label->setText(tr("PENDING"));
    m_s22Label->setStyleSheet(QStringLiteral(
        "font-size: 120px; font-weight: bold; color: #888; "
        "background-color: #2a2a2a; border-radius: 20px; padding: 40px;"));
}

} // namespace ui::views
