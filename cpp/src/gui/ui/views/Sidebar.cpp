#include "Sidebar.hpp"

#include "../../ui/theme/DesignTokens.hpp"

#include <QAbstractItemView>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListView>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStringListModel>
#include <QVBoxLayout>

namespace ui::views {

namespace {

constexpr double kStartMin = 0.001;
constexpr double kStartMax = 18.000;
constexpr double kEndMin = 0.001;
constexpr double kEndMax = 40.000;

constexpr double kThresholdMin = -120.0;
constexpr double kThresholdMax = 20.0;

QString formatKey(const QString& base, const QString& entry) {
    return base + QStringLiteral(".") + entry;
}

} // namespace

Sidebar::Sidebar(QWidget* parent)
    : QWidget(parent) {
    setObjectName(QStringLiteral("Sidebar"));
    setMinimumWidth(260);
    setMaximumWidth(280);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_StyledBackground, true);

    buildUi();
    bindSignals();
}

void Sidebar::setDevices(const QStringList& devices) {
    if (!m_deviceModel) {
        m_deviceModel = new QStringListModel(this);
        m_deviceList->setModel(m_deviceModel);
    }
    m_deviceModel->setStringList(devices);
    if (!devices.isEmpty()) {
        m_deviceList->setCurrentIndex(m_deviceModel->index(0));
    }
}

void Sidebar::setCalibrations(const QStringList& calibrations) {
    if (!m_calibrationModel) {
        m_calibrationModel = new QStringListModel(this);
        m_calibrationList->setModel(m_calibrationModel);
    }
    m_calibrationModel->setStringList(calibrations);
    if (!calibrations.isEmpty()) {
        m_calibrationList->setCurrentIndex(m_calibrationModel->index(0));
    }
}

void Sidebar::setSeriesSelection(const QHash<QString, bool>& selection) {
    for (auto it = m_paramButtons.begin(); it != m_paramButtons.end(); ++it) {
        QPushButton* button = it.value();
        const bool value = selection.value(it.key(), true);
        QSignalBlocker blocker(button);
        button->setChecked(value);
        if (m_thresholds.contains(it.key())) {
            m_thresholds.value(it.key())->setEnabled(value);
        }
    }
}

void Sidebar::setThresholds(const QHash<QString, double>& thresholds) {
    for (auto it = thresholds.constBegin(); it != thresholds.constEnd(); ++it) {
        if (m_thresholds.contains(it.key())) {
            QDoubleSpinBox* spin = m_thresholds.value(it.key());
            QSignalBlocker blocker(spin);
            spin->setValue(it.value());
        }
    }
}

void Sidebar::setFrequencyRange(double startGHz, double endGHz) {
    if (m_startSpin) {
        m_startSpin->setValue(startGHz);
    }
    if (m_endSpin) {
        m_endSpin->setValue(endGHz);
    }
}

void Sidebar::setPointCount(int points) {
    if (m_pointsSpin) {
        m_pointsSpin->setValue(points);
    }
}

void Sidebar::setThresholdValue(const QString& name, double value) {
    if (m_thresholds.contains(name)) {
        QDoubleSpinBox* spin = m_thresholds.value(name);
        QSignalBlocker blocker(spin);
        spin->setValue(value);
    }
}

void Sidebar::applyTranslations(const QHash<QString, QString>& strings) {
    m_strings = strings;
    updateSectionTitles();
    for (auto it = m_thresholdCaptions.begin(); it != m_thresholdCaptions.end(); ++it) {
        if (it.value()) {
            it.value()->setText(tr("threshold"));
        }
    }
    m_startButton->setText(trKey(QStringLiteral("actions.start"), tr("Start")));
    m_stopButton->setText(trKey(QStringLiteral("actions.stop"), tr("Stop")));
    m_resetButton->setText(trKey(QStringLiteral("actions.reset"), tr("Reset")));
}

void Sidebar::applyLocale(const QLocale& locale) {
    m_locale = locale;
    updateSpinLocale();
}

void Sidebar::buildUi() {
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(16, 16, 16, 16);
    rootLayout->setSpacing(16);

    auto makeSectionLabel = [this](const QString& key, const QString& fallback, QLabel** store) {
        auto* label = new QLabel(fallback, this);
        label->setProperty("role", "subtitle");
        label->setAccessibleDescription(fallback);
        *store = label;
        return label;
    };

    auto makeListSection = [this, rootLayout, makeSectionLabel](const QString& key,
                                                                const QString& fallback,
                                                                QListView** listStore,
                                                                QLabel** labelStore) {
        auto* label = makeSectionLabel(key, fallback, labelStore);
        rootLayout->addWidget(label);

        auto* frame = new QFrame(this);
        frame->setProperty("type", "card");
        frame->setFrameShape(QFrame::NoFrame);

        auto* frameLayout = new QVBoxLayout(frame);
        frameLayout->setContentsMargins(0, 0, 0, 0);

        auto* list = new QListView(frame);
        list->setEditTriggers(QAbstractItemView::NoEditTriggers);
        list->setSelectionMode(QAbstractItemView::SingleSelection);
        list->setUniformItemSizes(true);
        list->setMinimumHeight(120);
        frameLayout->addWidget(list);

        *listStore = list;
        rootLayout->addWidget(frame);
    };

    makeListSection(QStringLiteral("sidebar.vna"), tr("VNA"), &m_deviceList, &m_vnaLabel);
    makeListSection(QStringLiteral("sidebar.calibration"), tr("Calibration"), &m_calibrationList,
                    &m_calibrationLabel);

    m_frequencyLabel = makeSectionLabel(QStringLiteral("sidebar.frequency"), tr("Frequency Range"),
                                        &m_frequencyLabel);
    rootLayout->addWidget(m_frequencyLabel);

    auto* frequencyRow = new QHBoxLayout;
    frequencyRow->setSpacing(12);

    m_startSpin = new QDoubleSpinBox(this);
    m_startSpin->setDecimals(3);
    m_startSpin->setRange(kStartMin, kStartMax);
    m_startSpin->setValue(1.000);
    m_startSpin->setSingleStep(0.010);
    m_startSpin->setSuffix(QStringLiteral(" GHz"));
    m_startSpin->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    frequencyRow->addWidget(m_startSpin);

    auto* dash = new QLabel(QStringLiteral("–"), this);
    dash->setAlignment(Qt::AlignCenter);
    frequencyRow->addWidget(dash);

    m_endSpin = new QDoubleSpinBox(this);
    m_endSpin->setDecimals(3);
    m_endSpin->setRange(kEndMin, kEndMax);
    m_endSpin->setValue(6.000);
    m_endSpin->setSingleStep(0.010);
    m_endSpin->setSuffix(QStringLiteral(" GHz"));
    m_endSpin->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    frequencyRow->addWidget(m_endSpin);

    frequencyRow->setStretch(0, 1);
    frequencyRow->setStretch(2, 1);

    rootLayout->addLayout(frequencyRow);

    m_pointsLabel = makeSectionLabel(QStringLiteral("sidebar.points"), tr("Points"), &m_pointsLabel);
    rootLayout->addWidget(m_pointsLabel);

    m_pointsSpin = new QSpinBox(this);
    m_pointsSpin->setRange(1, 4096);
    m_pointsSpin->setValue(201);
    m_pointsSpin->setSingleStep(10);
    m_pointsSpin->setFixedHeight(32);
    m_pointsSpin->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    rootLayout->addWidget(m_pointsSpin);

    m_parametersLabel = makeSectionLabel(QStringLiteral("sidebar.parameters"), tr("S-Parameters"),
                                         &m_parametersLabel);
    rootLayout->addWidget(m_parametersLabel);

    const QStringList params{QStringLiteral("S11"), QStringLiteral("S12"), QStringLiteral("S21"),
                             QStringLiteral("S22")};

    auto* paramGrid = new QGridLayout;
    paramGrid->setContentsMargins(0, 0, 0, 0);
    paramGrid->setHorizontalSpacing(16);
    paramGrid->setVerticalSpacing(12);

    int column = 0;
    for (const QString& param : params) {
        auto* cell = new QWidget(this);
        auto* cellLayout = new QVBoxLayout(cell);
        cellLayout->setContentsMargins(0, 0, 0, 0);
        cellLayout->setSpacing(6);

        auto* button = new QPushButton(param, cell);
        button->setCheckable(true);
        button->setChecked(true);
        button->setMinimumSize(80, 80);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        button->setProperty("role", "param");
        cellLayout->addWidget(button);

        auto* thresholdRow = new QHBoxLayout;
        thresholdRow->setContentsMargins(0, 0, 0, 0);
        thresholdRow->setSpacing(4);

        auto* spin = new QDoubleSpinBox(cell);
        spin->setDecimals(3);
        spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
        spin->setRange(kThresholdMin, kThresholdMax);
        spin->setSingleStep(0.100);
        spin->setSuffix(QStringLiteral(" dB"));
        spin->setAlignment(Qt::AlignCenter);
        spin->setFixedWidth(72);
        spin->setValue(-20.000);
        spin->setMinimumHeight(32);
        spin->setProperty("paramName", param);
        thresholdRow->addStretch(1);
        thresholdRow->addWidget(spin);
        thresholdRow->addStretch(1);
        cellLayout->addLayout(thresholdRow);

        auto* caption = new QLabel(tr("threshold"), cell);
        caption->setAlignment(Qt::AlignHCenter);
        caption->setProperty("role", "caption");
        cellLayout->addWidget(caption);

        paramGrid->addWidget(cell, 0, column++);

        m_paramButtons.insert(param, button);
        m_thresholds.insert(param, spin);
        m_thresholdCaptions.insert(param, caption);
    }

    for (int c = 0; c < paramGrid->columnCount(); ++c) {
        paramGrid->setColumnStretch(c, 1);
    }
    rootLayout->addLayout(paramGrid);

    auto* buttonRow = new QHBoxLayout;
    buttonRow->setSpacing(12);

    m_startButton = new QPushButton(tr("Start"), this);
    m_startButton->setMinimumHeight(40);
    buttonRow->addWidget(m_startButton);

    m_stopButton = new QPushButton(tr("Stop"), this);
    m_stopButton->setMinimumHeight(40);
    buttonRow->addWidget(m_stopButton);

    m_resetButton = new QPushButton(tr("Reset"), this);
    m_resetButton->setMinimumHeight(40);
    buttonRow->addWidget(m_resetButton);

    rootLayout->addLayout(buttonRow);
    rootLayout->addStretch(1);
}

void Sidebar::bindSignals() {
    connect(m_startSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [this](double) { emit rangeChanged(m_startSpin->value(), m_endSpin->value()); });
    connect(m_endSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [this](double) { emit rangeChanged(m_startSpin->value(), m_endSpin->value()); });

    connect(m_pointsSpin, qOverload<int>(&QSpinBox::valueChanged), this, &Sidebar::pointsChanged);

    for (auto it = m_paramButtons.begin(); it != m_paramButtons.end(); ++it) {
        connect(it.value(), &QPushButton::toggled, this, [this, param = it.key()](bool on) {
            if (m_thresholds.contains(param)) {
                m_thresholds.value(param)->setEnabled(on);
            }
            emit parameterToggled(param, on);
        });
    }

    for (auto it = m_thresholds.begin(); it != m_thresholds.end(); ++it) {
        connect(it.value(), qOverload<double>(&QDoubleSpinBox::valueChanged), this,
                [this, param = it.key()](double value) {
                    emit thresholdChanged(param, value);
                });
    }

    connect(m_startButton, &QPushButton::clicked, this, &Sidebar::startRequested);
    connect(m_stopButton, &QPushButton::clicked, this, &Sidebar::stopRequested);
    connect(m_resetButton, &QPushButton::clicked, this, &Sidebar::resetRequested);
}

void Sidebar::updateSpinLocale() {
    const QList<QDoubleSpinBox*> doubles{m_startSpin, m_endSpin};
    for (QDoubleSpinBox* spin : doubles) {
        spin->setLocale(m_locale);
        spin->setDecimals(3);
    }
    for (QDoubleSpinBox* spin : m_thresholds) {
        spin->setLocale(m_locale);
        spin->setDecimals(3);
        spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    }
}

void Sidebar::updateSectionTitles() {
    setLabelText(QStringLiteral("sidebar.vna"), m_vnaLabel, tr("VNA"));
    setLabelText(QStringLiteral("sidebar.calibration"), m_calibrationLabel, tr("Calibration"));
    setLabelText(QStringLiteral("sidebar.frequency"), m_frequencyLabel, tr("Frequency Range"));
    setLabelText(QStringLiteral("sidebar.points"), m_pointsLabel, tr("Points"));
    setLabelText(QStringLiteral("sidebar.parameters"), m_parametersLabel, tr("S-Parameters"));
}

QString Sidebar::trKey(const QString& key, const QString& fallback) const {
    return m_strings.value(key, fallback);
}

void Sidebar::setLabelText(const QString& key, QLabel* label, const QString& fallback) {
    if (!label) {
        return;
    }
    const QString text = trKey(key, fallback);
    label->setText(text);
    label->setAccessibleDescription(text);
}

} // namespace ui::views





