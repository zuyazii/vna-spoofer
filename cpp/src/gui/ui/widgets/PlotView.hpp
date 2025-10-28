// Copyright (c) 2025 vna-spoofer
// SPDX-License-Identifier: MIT

#pragma once

#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QtCore/QHash>
#include <QtCore/QPointF>
#include <QtCore/QVector>
#include <QtGui/QColor>

namespace ui::widgets {

class PlotView : public QChartView {
    Q_OBJECT

public:
    explicit PlotView(QWidget* parent = nullptr);

    void setSeriesColor(const QString& name, const QColor& color);
    void setSeriesMagnitudeColor(const QString& name, const QColor& color);
    void setSeriesPhaseColor(const QString& name, const QColor& color);
    void setSeriesVisible(const QString& name, bool on);
    void setSeriesMagnitudeVisible(const QString& name, bool on);
    void setSeriesPhaseVisible(const QString& name, bool on);
    void setShowMagnitude(bool on);
    void setShowPhase(bool on);
    void setSeriesData(const QString& name,
                       const QVector<QPointF>& magnitudePoints,
                       const QVector<QPointF>& phasePoints);
    void setFrequencyAxisRange(double minGHz, double maxGHz);
    void setMagnitudeAxisRange(double minDb, double maxDb);
    void setPhaseAxisRange(double minDeg, double maxDeg);
    void applyTranslations(const QHash<QString, QString>& strings);

signals:
    void rangeChanged(double startGHz, double endGHz);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    struct SeriesSet {
        QLineSeries* magnitude = nullptr;
        QLineSeries* phase = nullptr;
        QColor magnitudeColor;
        QColor phaseColor;
        bool enabled = true;
        bool magnitudeVisible = true;
        bool phaseVisible = true;
    };

    void initializeChart();
    void initializeSeries();
    void updateSeriesVisibility(const QString& name);
    void updateGlobalVisibility();
    void updateAxisTitles();
    void refreshSeriesPens(const QString& name);
    void clampFrequencyRange();

    QHash<QString, SeriesSet> m_series;
    QHash<QString, QString> m_strings;
    QValueAxis* m_axisFrequency = nullptr;
    QValueAxis* m_axisMagnitude = nullptr;
    QValueAxis* m_axisPhase = nullptr;
    bool m_showMagnitude = true;
    bool m_showPhase = true;
};

} // namespace ui::widgets
