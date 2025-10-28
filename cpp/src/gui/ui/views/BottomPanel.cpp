#include "BottomPanel.hpp"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace ui::views {

namespace {

const struct LanguageEntry {
    const char* key;
    const char* label;
    QLocale locale;
} kLanguages[] = {
    {"lang.en", "English", QLocale(QLocale::English, QLocale::UnitedStates)},
    {"lang.zh_Hant", "\u7E41\u9AD4\u4E2D\u6587", QLocale(QStringLiteral("zh_Hant"))},
    {"lang.zh_Hans", "\u7B80\u4F53\u4E2D\u6587", QLocale(QStringLiteral("zh_Hans"))}
};

const QStringList kSeriesOrder = {
    QStringLiteral("S11"),
    QStringLiteral("S12"),
    QStringLiteral("S21"),
    QStringLiteral("S22")
};

} // namespace

BottomPanel::BottomPanel(QWidget* parent)
    : QWidget(parent) {
    setObjectName(QStringLiteral("BottomPanel"));
    setProperty("type", "card");
    setAttribute(Qt::WA_StyledBackground, true);
    setMinimumHeight(380);
    buildUi();
    bindSignals();
}

void BottomPanel::setSeriesEnabled(const QHash<QString, bool>& enabled) {
    for (auto it = enabled.constBegin(); it != enabled.constEnd(); ++it) {
        const bool on = it.value();
        m_enabledSeries.insert(it.key(), on);
        if (m_magnitudeChecks.contains(it.key())) {
            widgets::SeriesCheck* check = m_magnitudeChecks.value(it.key());
            {
                QSignalBlocker blocker(check);
                if (!on) {
                    check->setChecked(false);
                }
            }
            check->setEnabled(on && m_magnitudeVisibleGlobal);
        }
        if (m_phaseChecks.contains(it.key())) {
            widgets::SeriesCheck* check = m_phaseChecks.value(it.key());
            {
                QSignalBlocker blocker(check);
                if (!on) {
                    check->setChecked(false);
                }
            }
            check->setEnabled(on && m_phaseVisibleGlobal);
        }
    }
    syncMagnitudeHeader();
    syncPhaseHeader();
}

void BottomPanel::setMagnitudeState(const QHash<QString, bool>& state) {
    m_blockMagnitude = true;
    for (auto it = state.constBegin(); it != state.constEnd(); ++it) {
        if (m_magnitudeChecks.contains(it.key())) {
            const bool allowed = m_enabledSeries.value(it.key(), true);
            const bool value = allowed ? it.value() : false;
            widgets::SeriesCheck* check = m_magnitudeChecks.value(it.key());
            QSignalBlocker blocker(check);
            check->setChecked(value);
            check->setEnabled(allowed && m_magnitudeVisibleGlobal);
        }
    }
    m_blockMagnitude = false;
    syncMagnitudeHeader();
}

void BottomPanel::setPhaseState(const QHash<QString, bool>& state) {
    m_blockPhase = true;
    for (auto it = state.constBegin(); it != state.constEnd(); ++it) {
        if (m_phaseChecks.contains(it.key())) {
            const bool allowed = m_enabledSeries.value(it.key(), true);
            const bool value = allowed ? it.value() : false;
            widgets::SeriesCheck* check = m_phaseChecks.value(it.key());
            QSignalBlocker blocker(check);
            check->setChecked(value);
            check->setEnabled(allowed && m_phaseVisibleGlobal);
        }
    }
    m_blockPhase = false;
    syncPhaseHeader();
}

void BottomPanel::setSeriesColors(const QHash<QString, QColor>& magnitudeColors,
                                  const QHash<QString, QColor>& phaseColors) {
    for (auto it = magnitudeColors.constBegin(); it != magnitudeColors.constEnd(); ++it) {
        if (m_magnitudeChecks.contains(it.key())) {
            m_magnitudeChecks.value(it.key())->setColor(it.value());
        }
    }
    for (auto it = phaseColors.constBegin(); it != phaseColors.constEnd(); ++it) {
        updatePhaseColor(it.key(), it.value());
    }
}

void BottomPanel::setMagnitudeVisible(bool on) {
    if (!m_magnitudeHeader) {
        return;
    }
    m_magnitudeVisibleGlobal = on;
    QSignalBlocker blocker(m_magnitudeHeader);
    m_magnitudeHeader->setCheckState(on ? Qt::Checked : Qt::Unchecked);
    for (widgets::SeriesCheck* check : m_magnitudeChecks) {
        const QString name = check->seriesName();
        const bool allowed = m_enabledSeries.value(name, true);
        check->setEnabled(on && allowed);
    }
}

void BottomPanel::setPhaseVisible(bool on) {
    if (!m_phaseHeader) {
        return;
    }
    m_phaseVisibleGlobal = on;
    QSignalBlocker blocker(m_phaseHeader);
    m_phaseHeader->setCheckState(on ? Qt::Checked : Qt::Unchecked);
    for (widgets::SeriesCheck* check : m_phaseChecks) {
        const QString name = check->seriesName();
        const bool allowed = m_enabledSeries.value(name, true);
        check->setEnabled(on && allowed);
    }
}

void BottomPanel::applyTranslations(const QHash<QString, QString>& strings) {
    m_strings = strings;
    updateTexts();
}

void BottomPanel::applyLocale(const QLocale& locale) {
    m_locale = locale;
}

void BottomPanel::buildUi() {
    auto* rootLayout = new QHBoxLayout(this);
    rootLayout->setContentsMargins(24, 24, 24, 24);
    rootLayout->setSpacing(32);

    auto createColumn = [this, rootLayout](QWidget** columnWidget, int minimumWidth) {
        auto* widget = new QWidget(this);
        widget->setMinimumWidth(minimumWidth);
        widget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        auto* layout = new QVBoxLayout(widget);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(10);
        rootLayout->addWidget(widget, 0);
        if (columnWidget) {
            *columnWidget = widget;
        }
        return layout;
    };

    QWidget* languageWidget = nullptr;
    QVBoxLayout* languageLayout = createColumn(&languageWidget, 160);

    m_languageLabel = new QLabel(tr("Language"), languageWidget);
    m_languageLabel->setProperty("role", "subtitle");
    languageLayout->addWidget(m_languageLabel);

    for (const auto& entry : kLanguages) {
        auto* check = new QCheckBox(QString::fromUtf8(entry.label), languageWidget);
        check->setTristate(false);
        check->setChecked(false);
        languageLayout->addWidget(check);
        m_languageChecks.insert(QString::fromUtf8(entry.key), check);
        check->setProperty("languageKey", QString::fromUtf8(entry.key));
        check->setProperty("role", "subtitle");
    }
    languageLayout->addStretch(1);

    QWidget* magnitudeWidget = nullptr;
    QVBoxLayout* magnitudeLayout = createColumn(&magnitudeWidget, 200);
    m_magnitudeHeader = new QCheckBox(tr("Magnitude"), magnitudeWidget);
    m_magnitudeHeader->setTristate(true);
    m_magnitudeHeader->setCheckState(Qt::Checked);
    m_magnitudeHeader->setProperty("role", "subtitle");
    magnitudeLayout->addWidget(m_magnitudeHeader);

    for (const QString& name : kSeriesOrder) {
        auto* check = new widgets::SeriesCheck(name, magnitudeWidget);
        check->setChecked(true);
        magnitudeLayout->addWidget(check);
        m_magnitudeChecks.insert(name, check);
    }
    magnitudeLayout->addStretch(1);

    QWidget* phaseWidget = nullptr;
    QVBoxLayout* phaseLayout = createColumn(&phaseWidget, 200);
    m_phaseHeader = new QCheckBox(tr("Phase"), phaseWidget);
    m_phaseHeader->setTristate(true);
    m_phaseHeader->setCheckState(Qt::Checked);
    m_phaseHeader->setProperty("role", "subtitle");
    phaseLayout->addWidget(m_phaseHeader);

    for (const QString& name : kSeriesOrder) {
        auto* check = new widgets::SeriesCheck(name, phaseWidget);
        check->setChecked(true);
        phaseLayout->addWidget(check);
        m_phaseChecks.insert(name, check);
    }
    phaseLayout->addStretch(1);

    // Default to English when available.
    const QString defaultLanguage = QStringLiteral("lang.en");
    if (m_languageChecks.contains(defaultLanguage)) {
        setLanguageSelection(defaultLanguage);
    } else if (!m_languageChecks.isEmpty()) {
        setLanguageSelection(m_languageChecks.cbegin().key());
    }

    rootLayout->addStretch(1);
}

void BottomPanel::bindSignals() {
    for (const auto& entry : kLanguages) {
        const QString key = QString::fromUtf8(entry.key);
        if (!m_languageChecks.contains(key)) {
            continue;
        }
        QCheckBox* check = m_languageChecks.value(key);
        connect(check, &QCheckBox::toggled, this, [this, key, locale = entry.locale](bool on) {
            if (m_blockLanguage) {
                return;
            }
            if (!on) {
                if (m_activeLanguageKey == key) {
                    QSignalBlocker blocker(m_languageChecks.value(key));
                    m_languageChecks.value(key)->setChecked(true);
                }
                return;
            }
            m_blockLanguage = true;
            for (auto it = m_languageChecks.begin(); it != m_languageChecks.end(); ++it) {
                if (it.key() == key) {
                    continue;
                }
                QSignalBlocker blocker(it.value());
                it.value()->setChecked(false);
            }
            m_activeLanguageKey = key;
            m_blockLanguage = false;
            emit languageChanged(locale);
        });
    }

    if (m_magnitudeHeader) {
        connect(m_magnitudeHeader, &QCheckBox::checkStateChanged, this, [this](Qt::CheckState state) {
            if (m_blockMagnitude) {
                return;
            }
            const bool on = state != Qt::Unchecked;
            m_magnitudeVisibleGlobal = on;
            m_blockMagnitude = true;
            for (auto it = m_magnitudeChecks.begin(); it != m_magnitudeChecks.end(); ++it) {
                widgets::SeriesCheck* check = it.value();
                const bool allowed = m_enabledSeries.value(it.key(), true);
                QSignalBlocker blocker(check);
                check->setChecked(allowed ? on : false);
                check->setEnabled(on && allowed);
            }
            m_blockMagnitude = false;
            for (auto it = m_magnitudeChecks.begin(); it != m_magnitudeChecks.end(); ++it) {
                const bool allowed = m_enabledSeries.value(it.key(), true);
                emit seriesMagnitudeToggled(it.key(), allowed ? on : false);
            }
            emit magnitudeAllToggled(on);
            syncMagnitudeHeader();
        });
    }

    if (m_phaseHeader) {
        connect(m_phaseHeader, &QCheckBox::checkStateChanged, this, [this](Qt::CheckState state) {
            if (m_blockPhase) {
                return;
            }
            const bool on = state != Qt::Unchecked;
            m_phaseVisibleGlobal = on;
            m_blockPhase = true;
            for (auto it = m_phaseChecks.begin(); it != m_phaseChecks.end(); ++it) {
                widgets::SeriesCheck* check = it.value();
                const bool allowed = m_enabledSeries.value(it.key(), true);
                QSignalBlocker blocker(check);
                check->setChecked(allowed ? on : false);
                check->setEnabled(on && allowed);
            }
            m_blockPhase = false;
            for (auto it = m_phaseChecks.begin(); it != m_phaseChecks.end(); ++it) {
                const bool allowed = m_enabledSeries.value(it.key(), true);
                emit seriesPhaseToggled(it.key(), allowed ? on : false);
            }
            emit phaseAllToggled(on);
            syncPhaseHeader();
        });
    }

    for (auto it = m_magnitudeChecks.begin(); it != m_magnitudeChecks.end(); ++it) {
        widgets::SeriesCheck* check = it.value();
        connect(check, &widgets::SeriesCheck::toggled, this, [this, name = it.key()](bool on) {
            if (!m_blockMagnitude) {
                emit seriesMagnitudeToggled(name, on);
            }
            syncMagnitudeHeader();
        });
        connect(check, &widgets::SeriesCheck::colorPicked, this,
                [this, name = it.key()](const QColor& color) {
                    emit magnitudeColorChanged(name, color);
                });
    }

    for (auto it = m_phaseChecks.begin(); it != m_phaseChecks.end(); ++it) {
        widgets::SeriesCheck* check = it.value();
        connect(check, &widgets::SeriesCheck::toggled, this, [this, name = it.key()](bool on) {
            if (!m_blockPhase) {
                emit seriesPhaseToggled(name, on);
            }
            syncPhaseHeader();
        });
        connect(check, &widgets::SeriesCheck::colorPicked, this,
                [this, name = it.key()](const QColor& color) {
                    emit phaseColorChanged(name, color);
                    updatePhaseColor(name, color);
                });
    }
}

void BottomPanel::updateTexts() {
    if (m_languageLabel) {
        m_languageLabel->setText(trKey(QStringLiteral("panel.language"), tr("Language")));
    }
    if (m_magnitudeHeader) {
        m_magnitudeHeader->setText(trKey(QStringLiteral("panel.magnitude"), tr("Magnitude")));
    }
    if (m_phaseHeader) {
        m_phaseHeader->setText(trKey(QStringLiteral("panel.phase"), tr("Phase")));
    }

    for (const auto& entry : kLanguages) {
        const QString key = QString::fromUtf8(entry.key);
        if (!m_languageChecks.contains(key)) {
            continue;
        }
        const QString translationKey = QStringLiteral("panel.%1").arg(key);
        m_languageChecks.value(key)->setText(trKey(translationKey, QString::fromUtf8(entry.label)));
    }
}

QString BottomPanel::trKey(const QString& key, const QString& fallback) const {
    return m_strings.value(key, fallback);
}

void BottomPanel::updatePhaseColor(const QString& name, const QColor& color) {
    if (!m_phaseChecks.contains(name)) {
        return;
    }
    m_phaseChecks.value(name)->setColor(color);
}
void BottomPanel::setLanguageSelection(const QString& key) {
    if (!m_languageChecks.contains(key)) {
        return;
    }
    m_blockLanguage = true;
    for (auto it = m_languageChecks.begin(); it != m_languageChecks.end(); ++it) {
        QSignalBlocker blocker(it.value());
        it.value()->setChecked(it.key() == key);
    }
    m_activeLanguageKey = key;
    m_blockLanguage = false;
    for (const auto& entry : kLanguages) {
        if (QString::fromUtf8(entry.key) == key) {
            emit languageChanged(entry.locale);
            break;
        }
    }
}

void BottomPanel::syncMagnitudeHeader() {
    if (!m_magnitudeHeader) {
        return;
    }
    int checkedCount = 0;
    int enabledCount = 0;
    for (widgets::SeriesCheck* check : m_magnitudeChecks) {
        if (!check->isEnabled()) {
            continue;
        }
        ++enabledCount;
        if (check->isChecked()) {
            ++checkedCount;
        }
    }
    m_blockMagnitude = true;
    if (enabledCount == 0 || checkedCount == 0) {
        m_magnitudeHeader->setCheckState(Qt::Unchecked);
    } else if (checkedCount == enabledCount) {
        m_magnitudeHeader->setCheckState(Qt::Checked);
    } else {
        m_magnitudeHeader->setCheckState(Qt::PartiallyChecked);
    }
    m_blockMagnitude = false;
}

void BottomPanel::syncPhaseHeader() {
    if (!m_phaseHeader) {
        return;
    }
    int checkedCount = 0;
    int enabledCount = 0;
    for (widgets::SeriesCheck* check : m_phaseChecks) {
        if (!check->isEnabled()) {
            continue;
        }
        ++enabledCount;
        if (check->isChecked()) {
            ++checkedCount;
        }
    }
    m_blockPhase = true;
    if (enabledCount == 0 || checkedCount == 0) {
        m_phaseHeader->setCheckState(Qt::Unchecked);
    } else if (checkedCount == enabledCount) {
        m_phaseHeader->setCheckState(Qt::Checked);
    } else {
        m_phaseHeader->setCheckState(Qt::PartiallyChecked);
    }
    m_blockPhase = false;
}

} // namespace ui::views
