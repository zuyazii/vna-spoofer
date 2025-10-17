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
#include <QVariant>
#include <QBoxLayout>
#include <QResizeEvent>
#include <QShowEvent>

#include <cstdint>
#include <atomic>
#include <filesystem>
#include <optional>
#include <thread>
#include <vector>
#include <functional>

class QLabel;
class QCheckBox;
class QPushButton;
class QToolButton;
class QDoubleSpinBox;
class QSpinBox;
class QListWidget;
class QListWidgetItem;
class QComboBox;
class QWidget;
class QStackedWidget;
class QButtonGroup;
class QString;
class QResizeEvent;
class QShowEvent;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

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
    void onLanguageSelectionChanged(int index);

private:
    enum class Language
    {
        English,
        SimplifiedChinese,
        TraditionalChinese
    };

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
    void updateThresholdEditorsEnabled();
    void resetThresholdEditorsToDefault();
    [[nodiscard]] QHash<QString, double> collectThresholds() const;
    void updateResponsiveLayout(int availableWidth);
    void adjustWindowForScreen();
    void registerTouchInputWidget(QWidget *widget);
    void showVirtualKeyboard();
    void initializeTranslations();
    void applyTranslations();
    void setLanguage(Language language);
    [[nodiscard]] QString translateText(const QString &text) const;
    [[nodiscard]] Language languageFromIndex(int index) const;
    [[nodiscard]] int indexFromLanguage(Language language) const;
    void registerTranslatable(const QString &key, std::function<void(const QString &)> setter);
    void setTestHint(const QString &key, const QList<QVariant> &args = {});

    struct TranslatableItem
    {
        QString key;
        std::function<void(const QString &)> setter;
    };

    struct ParameterExportInfo
    {
        QString name;
        double worstDb = -300.0;
        double failFrequencyHz = 0.0;
        bool pass = true;
        double thresholdDb = -10.0;
    };
    void persistSweepOutputs(const std::vector<librevna::headless::VNAMeasurement> &results,
                             bool overallPass,
                             double startFrequencyHz,
                             double stopFrequencyHz,
                             std::uint32_t pointCount,
                             double ifBandwidthHz,
                             double powerDbm,
                             const QStringList &activeParameters,
                             const QList<ParameterExportInfo> &parameterSummaries);

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
    QHash<QString, QDoubleSpinBox *> m_thresholdEditors;
    QComboBox *m_languageCombo = nullptr;
    QList<TranslatableItem> m_translatableItems;
    QString m_currentCalibrationStatusKey;
    QString m_currentCalibrationStatusStyle;
    QString m_currentDeviceStatusKey;
    QString m_currentDeviceStatusStyle;
    QString m_currentTestStateKey;
    QString m_testHintKey;
    QList<QVariant> m_testHintArgs;
    QHash<QString, double> m_lastThresholds;
    QStackedWidget *m_contentStack = nullptr;
    QButtonGroup *m_viewToggleGroup = nullptr;
    QPushButton *m_chartsToggleButton = nullptr;
    QPushButton *m_statusToggleButton = nullptr;
    QSet<QString> m_activeParameters;
    QBoxLayout *m_infoRowLayout = nullptr;
    QWidget *m_calibrationPanel = nullptr;
    QWidget *m_testControlPanel = nullptr;
    bool m_initialShowHandled = false;
    QSet<QWidget*> m_touchInputWidgets;

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
        QCheckBox *magnitudeToggle = nullptr;
        QCheckBox *phaseToggle = nullptr;
        QLineSeries *thresholdSeries = nullptr;
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
    Language m_currentLanguage = Language::English;
    QHash<QString, QString> m_translationZhHans;
    QHash<QString, QString> m_translationZhHant;

    std::thread m_sweepThread;
    std::atomic<bool> m_cancelRequested{false};
    std::atomic<bool> m_sweepInProgress{false};
    QHash<QString, ChartComponents> m_chartComponents;
    std::vector<librevna::headless::VNAMeasurement> m_latestMeasurements;
};
