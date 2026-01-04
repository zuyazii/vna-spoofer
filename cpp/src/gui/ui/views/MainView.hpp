// Copyright (c) 2025 vna-spoofer
// SPDX-License-Identifier: MIT

#pragma once

#include "librevna_headless/device_discovery.hpp"
#include "librevna_headless/host_core.hpp"

#include <QHash>
#include <QLocale>
#include <QWidget>

#include <atomic>
#include <filesystem>
#include <thread>
#include <vector>

class QLabel;
class QPushButton;

namespace ui::views {

class MainView : public QWidget {
    Q_OBJECT

public:
    explicit MainView(QWidget* parent = nullptr);
    ~MainView() override;

signals:
    void startRequested();
    void stopRequested();
    void resetRequested();

private:
    void buildUi();
    void setupConnections();
    void scanForDevices();
    void connectToDevice(int index);
    void loadCalibrationFromFile();
    void applyCalibrationFromPath(const QString& path);
    void refreshCalibrationList();
    void ensureSweepThreadFinished();
    void startSweep();
    void stopSweep();
    void resetSweep();
    std::filesystem::path findCalibrationDirectory() const;
    void updateSweepControlState(bool running);
    void showInformation(const QString& title, const QString& message);
    void showWarning(const QString& title, const QString& message);
    void setResultState(const QString& name, bool passed, bool hasData);
    void resetResultStates();

    // Demo fixed parameters
    static constexpr double kDemoStartGHz = 3.4;
    static constexpr double kDemoStopGHz = 3.6;
    static constexpr int kDemoPoints = 201;
    static constexpr double kDemoThresholdDb = -10.0;

    // UI elements
    QLabel* m_s11Label = nullptr;
    QLabel* m_s22Label = nullptr;
    QLabel* m_statusLabel = nullptr;
    QPushButton* m_startButton = nullptr;
    QPushButton* m_stopButton = nullptr;
    QPushButton* m_resetButton = nullptr;
    QPushButton* m_calButton = nullptr;

    // Backend
    librevna::headless::HostCore m_hostCore;
    std::vector<librevna::headless::DiscoveredDevice> m_discoveredDevices;
    std::vector<std::filesystem::path> m_calibrationFiles;
    int m_selectedDeviceIndex = -1;
    QString m_connectedSerial;
    std::filesystem::path m_calibrationDirectory;
    std::filesystem::path m_activeCalibrationPath;
    std::thread m_sweepThread;
    std::atomic<bool> m_cancelRequested{false};
    std::atomic<bool> m_sweepInProgress{false};
};

} // namespace ui::views
