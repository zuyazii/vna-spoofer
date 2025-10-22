#include "BottomPanel.hpp"

#include <QColor>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVariant>
#include <QVBoxLayout>

namespace ui::views {

namespace {

const struct LanguageEntry {
    const char* key;
    const char* label;
    QLocale locale;
} kLanguages[] = {
    {"lang.en", "English", QLocale(QLocale::English, QLocale::UnitedStates)},
    {"lang.zh_Hant", "繁體中文", QLocale(QStringLiteral("zh_Hant"))},
    {"lang.zh_Hans", "简体中文", QLocale(QStringLiteral("zh_Hans"))}
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
    buildUi();
    bindSignals();
}

void BottomPanel::setSeriesState(const QHash<QString, bool>& visibility) {
    for (auto it = m_seriesChecks.begin(); it != m_seriesChecks.end(); ++it) {
        it.value()->setChecked(visibility.value(it.key(), true));
    }
}

void BottomPanel::setSeriesColors(const QHash<QString, QColor>& colors) {
    for (auto it = colors.constBegin(); it != colors.constEnd(); ++it) {
        if (m_seriesChecks.contains(it.key())) {
            m_seriesChecks.value(it.key())->setColor(it.value());
        }
    }
}

void BottomPanel::setMagnitudeVisible(bool on) {
    if (m_magnitudeToggle) {
        m_magnitudeToggle->setChecked(on);
    }
}

void BottomPanel::setPhaseVisible(bool on) {
    if (m_phaseToggle) {
        m_phaseToggle->setChecked(on);
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
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(16, 16, 16, 16);
    rootLayout->setSpacing(16);

    auto* topRow = new QHBoxLayout;
    topRow->setSpacing(12);

    m_languageLabel = new QLabel(tr("Language"), this);
    m_languageLabel->setProperty("role", "subtitle");
    topRow->addWidget(m_languageLabel);

    m_languageCombo = new QComboBox(this);
    m_languageCombo->setMinimumWidth(160);
    int index = 0;
    for (const auto& entry : kLanguages) {
        m_languageCombo->addItem(QString::fromUtf8(entry.label), QVariant::fromValue(entry.locale));
        m_languageCombo->setItemData(index, QString::fromUtf8(entry.key), Qt::UserRole + 1);
        ++index;
    }
    topRow->addWidget(m_languageCombo);

    topRow->addStretch(1);

    m_magnitudeToggle = new QPushButton(tr("Magnitude"), this);
    m_magnitudeToggle->setCheckable(true);
    m_magnitudeToggle->setChecked(true);
    m_magnitudeToggle->setMinimumSize(120, 32);
    topRow->addWidget(m_magnitudeToggle);

    m_phaseToggle = new QPushButton(tr("Phase"), this);
    m_phaseToggle->setCheckable(true);
    m_phaseToggle->setChecked(true);
    m_phaseToggle->setMinimumSize(120, 32);
    topRow->addWidget(m_phaseToggle);

    rootLayout->addLayout(topRow);

    auto* seriesLayout = new QVBoxLayout;
    seriesLayout->setSpacing(8);

    for (const QString& name : kSeriesOrder) {
        auto* check = new widgets::SeriesCheck(name, this);
        check->setChecked(true);
        seriesLayout->addWidget(check);
        m_seriesChecks.insert(name, check);
    }

    rootLayout->addLayout(seriesLayout);
}

void BottomPanel::bindSignals() {
    connect(m_languageCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (index < 0) {
            return;
        }
        const QVariant data = m_languageCombo->itemData(index);
        if (!data.isValid()) {
            return;
        }
        const QLocale locale = data.value<QLocale>();
        emit languageChanged(locale);
    });

    if (m_magnitudeToggle) {
        connect(m_magnitudeToggle, &QPushButton::toggled, this, &BottomPanel::magnitudeToggled);
    }
    if (m_phaseToggle) {
        connect(m_phaseToggle, &QPushButton::toggled, this, &BottomPanel::phaseToggled);
    }

    for (auto it = m_seriesChecks.begin(); it != m_seriesChecks.end(); ++it) {
        connect(it.value(), &widgets::SeriesCheck::toggled, this,
                [this, name = it.key()](bool on) { emit seriesVisibilityChanged(name, on); });
        connect(it.value(), &widgets::SeriesCheck::colorPicked, this,
                [this, name = it.key()](const QColor& color) { emit seriesColorChanged(name, color); });
    }
}

void BottomPanel::updateTexts() {
    if (!m_languageCombo) {
        return;
    }
    if (m_languageLabel) {
        m_languageLabel->setText(trKey(QStringLiteral("panel.language"), tr("Language")));
    }

    for (int i = 0; i < m_languageCombo->count(); ++i) {
        const QString key = m_languageCombo->itemData(i, Qt::UserRole + 1).toString();
        m_languageCombo->setItemText(i, trKey(QStringLiteral("panel.") + key, m_languageCombo->itemText(i)));
    }

    if (m_magnitudeToggle) {
        m_magnitudeToggle->setText(trKey(QStringLiteral("panel.magnitude"), tr("Magnitude")));
    }
    if (m_phaseToggle) {
        m_phaseToggle->setText(trKey(QStringLiteral("panel.phase"), tr("Phase")));
    }
}

QString BottomPanel::trKey(const QString& key, const QString& fallback) const {
    return m_strings.value(key, fallback);
}

} // namespace ui::views
