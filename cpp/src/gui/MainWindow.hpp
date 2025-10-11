#pragma once

#include "librevna_headless/device_discovery.hpp"
#include "librevna_headless/host_core.hpp"

#include <QMainWindow>
#include <QList>
#include <QHash>
#include <QSet>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

#include <atomic>
#include <filesystem>
#include <optional>
#include <thread>
#include <vector>

class QLabel;
class QPushButton;
class QToolButton;
class QDoubleSpinBox;
class QSpinBox;
class QListWidget;
class QListWidgetItem;
class QWidget;
class QStackedWidget;
class QButtonGroup;
class QString;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void onLoadCalibration();
    void onSetupCalibration();
    void onScanDevices();
    void onStartTest();
    void onStopTest();
    void onResetTest();
    void onSParameterToggled(bool checked);
    void onDeviceSelectionChanged();
    void onDeviceActivated(QListWidgetItem *item);
    void onCalibrationActivated(QListWidgetItem *item);

private:
    void setupUi();
    QWidget *createCalibrationPanel();
    QWidget *createTestControlPanel();
    QWidget *createChartsPanel();
    QWidget *createStatusPanel();
    QWidget *createChartCard(const QString &parameterId, const QString &description);
    QWidget *createViewTogglePanel(QWidget *parent);

    void refreshCalibrationList();
    [[nodiscard]] std::filesystem::path findCalibrationDirectory() const;
    void appendStatusMessage(const QString &message);
    void updateCalibrationStatus(const QString &status, const QString &styleSheet);
    void updateDeviceStatus(const QString &status, const QString &styleSheet);
    void updateTestState(const QString &state);
    void updateControlsForRunning(bool running);
    void connectToDevice(const librevna::headless::DiscoveredDevice &device);
    void applyCalibrationFromPath(const QString &path);
    void ensureSweepThreadFinished();
    void prepareChartsForSweep();
    void resetCharts();
    void updateChartsWithResults(const std::vector<librevna::headless::VNAMeasurement> &results);
    void setChartBadgeState(QLabel *badge, const QString &text, const QString &state);
    void setAllChartBadges(const QString &text, const QString &state);
    void resetChartToBaseline(const QString &parameterId);

    QLabel *m_calibrationStatusBadge = nullptr;
    QLabel *m_deviceStatusBadge = nullptr;
    QPushButton *m_loadCalibrationButton = nullptr;
    QPushButton *m_setupCalibrationButton = nullptr;
    QPushButton *m_scanDevicesButton = nullptr;
    QPushButton *m_connectDeviceButton = nullptr;
    QDoubleSpinBox *m_startFrequencySpin = nullptr;
    QDoubleSpinBox *m_stopFrequencySpin = nullptr;
    QSpinBox *m_pointsSpin = nullptr;
    QListWidget *m_activityLog = nullptr;
    QListWidget *m_deviceList = nullptr;
    QListWidget *m_calibrationList = nullptr;
    QLabel *m_testStateBadge = nullptr;
    QLabel *m_testHintLabel = nullptr;
    QPushButton *m_startButton = nullptr;
    QPushButton *m_stopButton = nullptr;
    QPushButton *m_resetButton = nullptr;
    QList<QToolButton *> m_parameterButtons;
    QStackedWidget *m_contentStack = nullptr;
    QButtonGroup *m_viewToggleGroup = nullptr;
    QPushButton *m_chartsToggleButton = nullptr;
    QPushButton *m_statusToggleButton = nullptr;
    QSet<QString> m_activeParameters;

    struct ChartComponents
    {
        QLabel *badge = nullptr;
        QChart *chart = nullptr;
        QChartView *view = nullptr;
        QLineSeries *magnitudeSeries = nullptr;
        QLineSeries *phaseSeries = nullptr;
        QValueAxis *axisFrequency = nullptr;
        QValueAxis *axisMagnitude = nullptr;
        QValueAxis *axisPhase = nullptr;
        double baseFrequencyMin = 0.0;
        double baseFrequencyMax = 1.0;
        double baseMagnitudeMin = -100.0;
        double baseMagnitudeMax = 10.0;
        double basePhaseMin = -180.0;
        double basePhaseMax = 180.0;
    };

    librevna::headless::HostCore m_hostCore;
    std::vector<librevna::headless::DiscoveredDevice> m_discoveredDevices;
    QString m_connectedSerial;
    std::filesystem::path m_activeCalibrationPath;
    std::filesystem::path m_calibrationDirectory;

    std::thread m_sweepThread;
    std::atomic<bool> m_cancelRequested{false};
    std::atomic<bool> m_sweepInProgress{false};
    QHash<QString, ChartComponents> m_chartComponents;
    std::vector<librevna::headless::VNAMeasurement> m_latestMeasurements;
};
