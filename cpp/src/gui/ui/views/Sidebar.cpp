#include "Sidebar.hpp"

#include "../../ui/theme/DesignTokens.hpp"

#include <QAbstractItemView>
#include <QAbstractSpinBox>
#include <QCursor>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QItemSelectionModel>
#include <QListView>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStringListModel>
#include <QToolButton>
#include <QVBoxLayout>
#include <QSize>

namespace ui::views {

namespace {

constexpr double kStartMin = 0.001;
constexpr double kStartMax = 18.000;
constexpr double kEndMin = 0.001;
constexpr double kEndMax = 40.000;

constexpr double kThresholdMin = -120.0;
constexpr double kThresholdMax = 20.0;

constexpr int kSectionSpacing = 36;

} // namespace

Sidebar::Sidebar(QWidget* parent)
    : QWidget(parent) {
    setObjectName(QStringLiteral("Sidebar"));
    setMinimumWidth(280);
    setMaximumWidth(QWIDGETSIZE_MAX);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_StyledBackground, true);
    setProperty("type", "card");

    buildUi();
    bindSignals();
}

void Sidebar::setDevices(const QStringList& devices) {
    if (!m_deviceModel) {
        m_deviceModel = new QStringListModel(this);
        m_deviceList->setModel(m_deviceModel);
    }

    QStringList listContents;
    if (devices.isEmpty()) {
        m_devicePlaceholderActive = true;
        if (m_devicePlaceholderText.isEmpty()) {
            m_devicePlaceholderText =
                trKey(QStringLiteral("sidebar.noDevice"), tr("No VNA Detected"));
        }
        listContents.append(m_devicePlaceholderText);
        m_deviceList->setEnabled(false);
    } else {
        m_devicePlaceholderActive = false;
        listContents = devices;
        m_deviceList->setEnabled(true);
    }

    m_deviceModel->setStringList(listContents);
    m_deviceList->setCurrentIndex(m_deviceModel->index(0));
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

void Sidebar::setSelectedDevice(int index) {
    if (!m_deviceList || !m_deviceModel) {
        return;
    }
    if (m_devicePlaceholderActive) {
        return;
    }
    const int rowCount = m_deviceModel->rowCount();
    if (index < 0 || index >= rowCount) {
        if (auto* selection = m_deviceList->selectionModel()) {
            selection->clearSelection();
            selection->setCurrentIndex(QModelIndex(), QItemSelectionModel::Clear);
        }
        return;
    }
    const QModelIndex modelIndex = m_deviceModel->index(index, 0);
    if (!modelIndex.isValid()) {
        return;
    }
    if (auto* selection = m_deviceList->selectionModel()) {
        selection->setCurrentIndex(modelIndex, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    }
    m_deviceList->scrollTo(modelIndex, QAbstractItemView::PositionAtCenter);
}

void Sidebar::setSelectedCalibration(int index) {
    if (!m_calibrationList || !m_calibrationModel) {
        return;
    }
    const int rowCount = m_calibrationModel->rowCount();
    if (index < 0 || index >= rowCount) {
        if (auto* selection = m_calibrationList->selectionModel()) {
            selection->clearSelection();
            selection->setCurrentIndex(QModelIndex(), QItemSelectionModel::Clear);
        }
        return;
    }
    const QModelIndex modelIndex = m_calibrationModel->index(index, 0);
    if (!modelIndex.isValid()) {
        return;
    }
    if (auto* selection = m_calibrationList->selectionModel()) {
        selection->setCurrentIndex(modelIndex, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    }
    m_calibrationList->scrollTo(modelIndex, QAbstractItemView::PositionAtCenter);
}

void Sidebar::setSweepControlsEnabled(bool startEnabled, bool stopEnabled, bool resetEnabled) {
    if (m_startButton) {
        m_startButton->setEnabled(startEnabled);
    }
    if (m_stopButton) {
        m_stopButton->setEnabled(stopEnabled);
    }
    if (m_resetButton) {
        m_resetButton->setEnabled(resetEnabled);
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
    m_devicePlaceholderText = trKey(QStringLiteral("sidebar.noDevice"), tr("No VNA Detected"));
    if (m_devicePlaceholderActive && m_deviceModel) {
        m_deviceModel->setStringList(QStringList{m_devicePlaceholderText});
        m_deviceList->setCurrentIndex(m_deviceModel->index(0));
    }
    if (m_vnaScanButton) {
        const QString scanText = trKey(QStringLiteral("sidebar.scan"), tr("Find Devices"));
        m_vnaScanButton->setToolTip(scanText);
        m_vnaScanButton->setAccessibleName(scanText);
    }
    if (m_calibrationUploadButton) {
        const QString uploadText = trKey(QStringLiteral("sidebar.upload"), tr("Upload Calibration"));
        m_calibrationUploadButton->setToolTip(uploadText);
        m_calibrationUploadButton->setAccessibleName(uploadText);
    }
}

void Sidebar::applyLocale(const QLocale& locale) {
    m_locale = locale;
    updateSpinLocale();
}

void Sidebar::buildUi() {
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(16, 16, 16, 16);
    rootLayout->setSpacing(12);

    auto addSectionHeader = [this, rootLayout](const QString& key,
                                               const QString& fallback,
                                               QLabel** labelStore,
                                               QToolButton** buttonStore,
                                               const QIcon& icon,
                                               const QString& tooltip) {
        auto* container = new QWidget(this);
        container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        auto* layout = new QHBoxLayout(container);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(8);

        auto* label = new QLabel(fallback, container);
        label->setProperty("role", "subtitle");
        label->setAccessibleDescription(fallback);
        label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        *labelStore = label;
        layout->addWidget(label);
        layout->addStretch(1);

        if (buttonStore) {
            if (!icon.isNull()) {
                auto* button = new QToolButton(container);
                button->setAutoRaise(false);
                button->setToolButtonStyle(Qt::ToolButtonIconOnly);
                button->setIcon(icon);
                button->setIconSize(QSize(20, 20));
                button->setFixedSize(32, 32);
                button->setProperty("variant", "ghost");
                const QString tip = tooltip.isEmpty() ? fallback : tooltip;
                button->setToolTip(tip);
                button->setAccessibleName(tip);
                button->setCursor(Qt::PointingHandCursor);
                layout->addWidget(button);
                *buttonStore = button;
            } else {
                *buttonStore = nullptr;
            }
        }

        rootLayout->addWidget(container);
        return label;
    };

    auto addDivider = [this, rootLayout]() {
        rootLayout->addSpacing(kSectionSpacing / 2);
        auto* divider = new QFrame(this);
        divider->setObjectName(QStringLiteral("SidebarDivider"));
        divider->setFixedHeight(1);
        divider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        rootLayout->addWidget(divider);
        rootLayout->addSpacing(kSectionSpacing / 2);
    };

    auto makeListSection = [this, rootLayout, addSectionHeader, addDivider](const QString& key,
                                                                const QString& fallback,
                                                                QListView** listStore,
                                                                QLabel** labelStore,
                                                                QToolButton** buttonStore,
                                                                const QString& objectName,
                                                                const QIcon& icon,
                                                                const QString& tooltip) {
        addSectionHeader(key, fallback, labelStore, buttonStore, icon, tooltip);

        auto* frame = new QFrame(this);
        frame->setProperty("type", "card");
        frame->setFrameShape(QFrame::NoFrame);
        frame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

        auto* frameLayout = new QVBoxLayout(frame);
        frameLayout->setContentsMargins(0, 0, 0, 0);

        auto* list = new QListView(frame);
        list->setObjectName(objectName);
        list->setEditTriggers(QAbstractItemView::NoEditTriggers);
        list->setSelectionMode(QAbstractItemView::SingleSelection);
        list->setUniformItemSizes(true);
        list->setMinimumHeight(120);
        list->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        frameLayout->addWidget(list);

        *listStore = list;
        rootLayout->addWidget(frame);
        addDivider();
    };

    const QIcon scanIcon(QStringLiteral(":/ui/theme/icons/scan.svg"));
    const QIcon uploadIcon(QStringLiteral(":/ui/theme/icons/upload.svg"));

    makeListSection(QStringLiteral("sidebar.vna"), tr("VNA"), &m_deviceList, &m_vnaLabel,
                    &m_vnaScanButton, QStringLiteral("DeviceListView"), scanIcon, tr("Find Devices"));
    makeListSection(QStringLiteral("sidebar.calibration"), tr("Calibration"), &m_calibrationList,
                    &m_calibrationLabel, &m_calibrationUploadButton, QStringLiteral("CalibrationListView"), uploadIcon,
                    tr("Upload Calibration"));

    addSectionHeader(QStringLiteral("sidebar.frequency"), tr("Frequency Range"), &m_frequencyLabel,
                     nullptr, QIcon(), QString());

    auto* frequencyRow = new QHBoxLayout;
    frequencyRow->setSpacing(12);

    const int kControlHeight = 40;
    auto createSpinControl = [this, kControlHeight](QAbstractSpinBox* spin,
                                                    const QString& accessibleBase) -> QWidget* {
        spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
        spin->setFixedHeight(kControlHeight);
        auto* container = new QWidget(this);
        container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        auto* layout = new QHBoxLayout(container);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(10);
        layout->addWidget(spin, 1);
        layout->setAlignment(spin, Qt::AlignVCenter);

        auto* buttonStrip = new QWidget(container);
        buttonStrip->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        auto* buttonLayout = new QHBoxLayout(buttonStrip);
        buttonLayout->setContentsMargins(0, 0, 0, 0);
        buttonLayout->setSpacing(8);
        buttonLayout->setAlignment(Qt::AlignCenter);

        auto makeButton = [&](const QString& iconPath, const QString& suffix,
                              auto stepFn) {
            auto* button = new QToolButton(buttonStrip);
            button->setIcon(QIcon(iconPath));
            button->setIconSize(QSize(16, 16));
            button->setFixedSize(kControlHeight, kControlHeight);
            button->setProperty("variant", "ghost");
            button->setAutoRepeat(true);
            button->setAutoRepeatDelay(250);
            button->setAutoRepeatInterval(60);
            button->setAccessibleName(accessibleBase + suffix);
            QObject::connect(button, &QToolButton::clicked, spin, stepFn);
            buttonLayout->addWidget(button);
            return button;
        };

        makeButton(QStringLiteral(":/ui/theme/icons/spin_up.svg"), tr(" increase"),
                   &QAbstractSpinBox::stepUp);
        makeButton(QStringLiteral(":/ui/theme/icons/spin_down.svg"), tr(" decrease"),
                   &QAbstractSpinBox::stepDown);

        layout->addWidget(buttonStrip, 0, Qt::AlignVCenter);
        return container;
    };

    m_startSpin = new QDoubleSpinBox(this);
    m_startSpin->setDecimals(3);
    m_startSpin->setRange(kStartMin, kStartMax);
    m_startSpin->setValue(1.000);
    m_startSpin->setSingleStep(0.010);
    m_startSpin->setSuffix(QStringLiteral(" GHz"));
    m_startSpin->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    frequencyRow->addWidget(createSpinControl(m_startSpin, tr("Start frequency")));

    auto* dash = new QLabel(QStringLiteral("-"), this);
    dash->setAlignment(Qt::AlignCenter);
    frequencyRow->addWidget(dash);

    m_endSpin = new QDoubleSpinBox(this);
    m_endSpin->setDecimals(3);
    m_endSpin->setRange(kEndMin, kEndMax);
    m_endSpin->setValue(6.000);
    m_endSpin->setSingleStep(0.010);
    m_endSpin->setSuffix(QStringLiteral(" GHz"));
    m_endSpin->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    frequencyRow->addWidget(createSpinControl(m_endSpin, tr("Stop frequency")));

    frequencyRow->setStretch(0, 1);
    frequencyRow->setStretch(2, 1);

    rootLayout->addLayout(frequencyRow);
    addDivider();

    addSectionHeader(QStringLiteral("sidebar.points"), tr("Points"), &m_pointsLabel, nullptr, QIcon(),
                     QString());

    m_pointsSpin = new QSpinBox(this);
    m_pointsSpin->setRange(1, 4096);
    m_pointsSpin->setValue(201);
    m_pointsSpin->setSingleStep(10);
    m_pointsSpin->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    rootLayout->addWidget(createSpinControl(m_pointsSpin, tr("Point count")));
    addDivider();

    addSectionHeader(QStringLiteral("sidebar.parameters"), tr("S-Parameters"), &m_parametersLabel,
                     nullptr, QIcon(), QString());

    const QStringList params{QStringLiteral("S11"), QStringLiteral("S12"), QStringLiteral("S21"),
                             QStringLiteral("S22")};

    auto* paramWrapper = new QWidget(this);
    paramWrapper->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto* paramGrid = new QGridLayout(paramWrapper);
    paramGrid->setContentsMargins(0, 0, 0, 0);
    paramGrid->setHorizontalSpacing(8);
    paramGrid->setVerticalSpacing(12);

    const int columnCount = params.size();
    int index = 0;
    for (const QString& param : params) {
        auto* cell = new QWidget(this);
        cell->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        auto* cellLayout = new QVBoxLayout(cell);
        cellLayout->setContentsMargins(0, 0, 0, 0);
        cellLayout->setSpacing(6);

        auto* button = new QPushButton(param, cell);
        button->setCheckable(true);
        button->setChecked(true);
        button->setMinimumSize(56, 56);
        button->setMaximumHeight(64);
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
        spin->setMinimumWidth(60);
        spin->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        spin->setValue(-20.000);
        spin->setMinimumHeight(32);
        spin->setProperty("paramName", param);
        thresholdRow->addWidget(spin);

        cellLayout->addLayout(thresholdRow);

        auto* caption = new QLabel(tr("threshold"), cell);
        caption->setAlignment(Qt::AlignHCenter);
        caption->setProperty("role", "caption");
        cellLayout->addWidget(caption);

        const int row = index / columnCount;
        const int column = index % columnCount;
        paramGrid->addWidget(cell, row, column);
        ++index;

        m_paramButtons.insert(param, button);
        m_thresholds.insert(param, spin);
        m_thresholdCaptions.insert(param, caption);
    }

    for (int c = 0; c < columnCount; ++c) {
        paramGrid->setColumnStretch(c, 1);
    }
    rootLayout->addWidget(paramWrapper);
    addDivider();

    auto* buttonRow = new QHBoxLayout;
    buttonRow->setSpacing(12);

    m_startButton = new QPushButton(tr("Start"), this);
    m_startButton->setMinimumHeight(40);
    m_startButton->setProperty("variant", "start");
    buttonRow->addWidget(m_startButton);

    m_stopButton = new QPushButton(tr("Stop"), this);
    m_stopButton->setMinimumHeight(40);
    m_stopButton->setProperty("variant", "stop");
    buttonRow->addWidget(m_stopButton);

    m_resetButton = new QPushButton(tr("Reset"), this);
    m_resetButton->setMinimumHeight(40);
    m_resetButton->setProperty("variant", "reset");
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

    if (m_vnaScanButton) {
        connect(m_vnaScanButton, &QToolButton::clicked, this, &Sidebar::vnaScanRequested);
    }
    if (m_calibrationUploadButton) {
        connect(m_calibrationUploadButton, &QToolButton::clicked, this,
                &Sidebar::calibrationUploadRequested);
    }

    if (m_deviceList) {
        if (auto* selection = m_deviceList->selectionModel()) {
            connect(selection, &QItemSelectionModel::currentChanged, this,
                    [this](const QModelIndex& current, const QModelIndex&) {
                        emit deviceSelectionChanged(current.isValid() ? current.row() : -1);
                    });
        }
        connect(m_deviceList, &QListView::doubleClicked, this, [this](const QModelIndex& index) {
            if (index.isValid()) {
                emit deviceActivated(index.row());
            }
        });
    }

    if (m_calibrationList) {
        if (auto* selection = m_calibrationList->selectionModel()) {
            connect(selection, &QItemSelectionModel::currentChanged, this,
                    [this](const QModelIndex& current, const QModelIndex&) {
                        emit calibrationSelectionChanged(current.isValid() ? current.row() : -1);
                    });
        }
        connect(m_calibrationList, &QListView::doubleClicked, this,
                [this](const QModelIndex& index) {
                    if (index.isValid()) {
                        emit calibrationActivated(index.row());
                    }
                });
    }
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






