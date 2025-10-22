// Copyright (c) 2025 vna-spoofer
// SPDX-License-Identifier: MIT

#pragma once

#include "../widgets/PlotView.hpp"
#include "BottomPanel.hpp"
#include "Sidebar.hpp"

#include <QHash>
#include <QLocale>
#include <QWidget>

class QSplitter;
class QToolButton;
class QScrollArea;

namespace ui::views {

class MainView : public QWidget {
    Q_OBJECT

public:
    explicit MainView(QWidget* parent = nullptr);

    void setSeriesColor(const QString& name, const QColor& color);
    void setSeriesVisible(const QString& name, bool visible);
    void setShowMagnitude(bool on);
    void setShowPhase(bool on);

    void setFrequencyRange(double startGHz, double endGHz);
    void setPointCount(int points);
    void setThreshold(const QString& name, double value);

signals:
    void startRequested();
    void stopRequested();
    void resetRequested();
    void rangeChanged(double startGHz, double endGHz);
    void pointsChanged(int points);
    void seriesToggled(const QString& name, bool on);
    void seriesColorChanged(const QString& name, const QColor& color);
    void thresholdChanged(const QString& name, double value);
    void magnitudeShown(bool on);
    void phaseShown(bool on);
    void languageChanged(const QLocale& locale);
    void presetsRequested();

private:
    void buildUi();
    void setupConnections();
    void initSeriesState();
    void applyLocale(const QLocale& locale);
    void applyTranslations(const QHash<QString, QString>& strings);
    void updateVisibilityUi();
    void updateColorsUi();
    void toggleSidebarVisibility();
    void toggleBottomPanelVisibility();

    QHash<QString, QString> loadStrings(const QString& path) const;
    QString resolveLocaleKey(const QLocale& locale) const;
    void onLanguageSelected(const QLocale& locale);

    int m_lastSidebarWidth = 260;
    bool m_sidebarCollapsed = false;
    bool m_bottomCollapsed = false;

    Sidebar* m_sidebar = nullptr;
    QScrollArea* m_sidebarScroll = nullptr;
    BottomPanel* m_bottomPanel = nullptr;
    QWidget* m_bottomWrapper = nullptr;
    widgets::PlotView* m_plot = nullptr;
    QSplitter* m_mainSplitter = nullptr;
    QToolButton* m_sidebarToggle = nullptr;
    QToolButton* m_bottomToggle = nullptr;
    QToolButton* m_presetsButton = nullptr;

    QHash<QString, bool> m_seriesVisibility;
    QHash<QString, QColor> m_seriesColors;

    QLocale m_currentLocale{QLocale::English};
    QHash<QString, QString> m_baseStrings;
    QHash<QString, QString> m_activeStrings;
};

} // namespace ui::views
