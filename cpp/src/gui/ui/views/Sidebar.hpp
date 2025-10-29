// Copyright (c) 2025 vna-spoofer
// SPDX-License-Identifier: MIT

#pragma once

#include <QHash>
#include <QLocale>
#include <QStringList>
#include <QWidget>

class QListView;
class QLabel;
class QStringListModel;
class QPushButton;
class QDoubleSpinBox;
class QSpinBox;
class QToolButton;

namespace ui::views {

class Sidebar : public QWidget {
    Q_OBJECT

public:
    explicit Sidebar(QWidget* parent = nullptr);

    void setDevices(const QStringList& devices);
    void setCalibrations(const QStringList& calibrations);
    void setSeriesSelection(const QHash<QString, bool>& selection);
    void setThresholds(const QHash<QString, double>& thresholds);
    void setFrequencyRange(double startGHz, double endGHz);
    void setPointCount(int points);
    void setThresholdValue(const QString& name, double value);

    void applyTranslations(const QHash<QString, QString>& strings);
    void applyLocale(const QLocale& locale);
    void setSelectedDevice(int index);
    void setSelectedCalibration(int index);
    void setSweepControlsEnabled(bool startEnabled, bool stopEnabled, bool resetEnabled);

signals:
    void startRequested();
    void stopRequested();
    void resetRequested();
    void rangeChanged(double startGHz, double endGHz);
    void pointsChanged(int points);
    void parameterToggled(const QString& name, bool on);
    void thresholdChanged(const QString& name, double value);
    void vnaScanRequested();
    void calibrationUploadRequested();
    void deviceSelectionChanged(int index);
    void deviceActivated(int index);
    void calibrationSelectionChanged(int index);
    void calibrationActivated(int index);

private:
    void buildUi();
    void bindSignals();
    void updateSpinLocale();
    void updateSectionTitles();
    QString trKey(const QString& key, const QString& fallback) const;
    void setLabelText(const QString& key, QLabel* label, const QString& fallback);
    QListView* m_deviceList = nullptr;
    QListView* m_calibrationList = nullptr;
    QDoubleSpinBox* m_startSpin = nullptr;
    QDoubleSpinBox* m_endSpin = nullptr;
    QSpinBox* m_pointsSpin = nullptr;
    QHash<QString, QPushButton*> m_paramButtons;
    QHash<QString, QDoubleSpinBox*> m_thresholds;
    QHash<QString, QLabel*> m_thresholdCaptions;
    QPushButton* m_startButton = nullptr;
    QPushButton* m_stopButton = nullptr;
    QPushButton* m_resetButton = nullptr;
    QToolButton* m_vnaScanButton = nullptr;
    QToolButton* m_calibrationUploadButton = nullptr;

    QHash<QString, QString> m_strings;
    QLocale m_locale;

    QLabel* m_vnaLabel = nullptr;
    QLabel* m_calibrationLabel = nullptr;
    QLabel* m_frequencyLabel = nullptr;
    QLabel* m_pointsLabel = nullptr;
    QLabel* m_parametersLabel = nullptr;
    bool m_devicePlaceholderActive = false;
    QString m_devicePlaceholderText;

    QStringListModel* m_deviceModel = nullptr;
    QStringListModel* m_calibrationModel = nullptr;
};

} // namespace ui::views

