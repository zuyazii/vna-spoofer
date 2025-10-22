// Copyright (c) 2025 vna-spoofer
// SPDX-License-Identifier: MIT

#pragma once

#include "../widgets/SeriesCheck.hpp"

#include <QHash>
#include <QLocale>
#include <QWidget>

class QComboBox;
class QLabel;
class QPushButton;

namespace ui::views {

class BottomPanel : public QWidget {
    Q_OBJECT

public:
    explicit BottomPanel(QWidget* parent = nullptr);

    void setSeriesState(const QHash<QString, bool>& visibility);
    void setSeriesColors(const QHash<QString, QColor>& colors);
    void setMagnitudeVisible(bool on);
    void setPhaseVisible(bool on);

    void applyTranslations(const QHash<QString, QString>& strings);
    void applyLocale(const QLocale& locale);

signals:
    void languageChanged(const QLocale& locale);
    void seriesVisibilityChanged(const QString& name, bool on);
    void seriesColorChanged(const QString& name, const QColor& color);
    void magnitudeToggled(bool on);
    void phaseToggled(bool on);

private:
    void buildUi();
    void bindSignals();
    void updateTexts();
    QString trKey(const QString& key, const QString& fallback) const;

    QComboBox* m_languageCombo = nullptr;
    QLabel* m_languageLabel = nullptr;
    QPushButton* m_magnitudeToggle = nullptr;
    QPushButton* m_phaseToggle = nullptr;

    QHash<QString, widgets::SeriesCheck*> m_seriesChecks;

    QHash<QString, QString> m_strings;
    QLocale m_locale;
};

} // namespace ui::views
