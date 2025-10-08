#include "MainWindow.hpp"

#include <QAbstractItemView>
#include <QButtonGroup>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QListWidget>
#include <QMainWindow>
#include <QMessageBox>
#include <QMetaObject>
#include <QPushButton>
#include <QSize>
#include <QSizePolicy>
#include <QSpinBox>
#include <QStackedWidget>
#include <QToolButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QStringList>
#include <QFileDialog>
#include <QListWidgetItem>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <sstream>
#include <utility>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setupUi();
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUi()
{
    setWindowTitle(QStringLiteral("S Parameter Test System"));
    resize(1200, 800);

    setStyleSheet(QStringLiteral(R"(
        QMainWindow {
            background-color: #f8f8fb;
        }
        QLabel#TitleLabel {
            font-size: 26px;
            font-weight: 650;
            color: #1c1c28;
        }
        QLabel#SubtitleLabel {
            font-size: 14px;
            color: #6f7285;
        }
        QFrame[panel="true"] {
            background: #ffffff;
            border: 1px solid #e6e8f0;
            border-radius: 18px;
        }
        QFrame[role="viewToggleContainer"] {
            background: #e7e9ff;
            border-radius: 28px;
            padding: 8px;
        }
        QLabel[role="badge"] {
            background: #eef1ff;
            color: #4540ff;
            padding: 4px 14px;
            border-radius: 14px;
            font-weight: 600;
        }
        QLabel[role="badge"][state="warning"] {
            background: #fff4db;
            color: #ad7300;
        }
        QLabel[role="badge"][state="error"] {
            background: #fdecef;
            color: #d12b57;
        }
        QPushButton {
            background: #181823;
            color: #ffffff;
            border-radius: 12px;
            min-height: 46px;
            padding: 10px 24px;
            font-size: 16px;
            font-weight: 600;
        }
        QPushButton::icon {
            subcontrol-origin: padding;
            subcontrol-position: left center;
            margin-right: 10px;
        }
        QPushButton#SecondaryButton {
            background: #ffffff;
            color: #181823;
            border: 1px solid #daddeb;
        }
        QPushButton#ActionButton {
            background: #181823;
        }
        QPushButton#StopButton {
            background: #ffd9e2;
            color: #ca1f4a;
        }
        QPushButton[role="segmented"] {
            background: transparent;
            color: #6f7285;
            border: none;
            border-radius: 20px;
            min-height: 44px;
            padding: 8px 24px;
            font-size: 17px;
            font-weight: 600;
        }
        QPushButton[role="segmented"]::icon {
            subcontrol-origin: padding;
            subcontrol-position: left center;
            margin-right: 12px;
        }
        QPushButton[role="segmented"]:checked {
            background: #ffffff;
            color: #181823;
        }
        QPushButton[role="segmented"]:hover {
            background: rgba(255, 255, 255, 0.7);
        }
        QPushButton:disabled {
            background: #d8dbe8;
            color: #7f8399;
        }
        QToolButton {
            background: #f4f5ff;
            border: 1px solid transparent;
            border-radius: 14px;
            padding: 6px 16px;
            min-height: 36px;
            font-size: 15px;
            font-weight: 600;
        }
        QToolButton:checked {
            background: #181823;
            color: #ffffff;
        }
        QLabel[role="hint"] {
            background: #fff6db;
            border: 1px solid #ffe4aa;
            color: #a16a00;
            border-radius: 14px;
            padding: 12px 16px;
        }
        QListWidget {
            background: #ffffff;
            border: 1px solid #e9ecf5;
            border-radius: 16px;
        }
        QListWidget::item {
            padding: 8px;
        }
        QLabel[role="cardTitle"] {
            font-weight: 700;
            font-size: 15px;
        }
        QLabel[role="cardSubtitle"] {
            color: #787c92;
        }
        QFrame[role="chartArea"] {
            background: #fcfdff;
            border: 1px dashed #d7daeb;
            border-radius: 16px;
            min-height: 180px;
        }
        QWidget[role="metaChip"] {
            background: #ffffff;
            border: 1px solid #e3e6f5;
            border-radius: 22px;
        }
        QWidget[role="metaChip"] QLabel[role="cardTitle"] {
            font-size: 16px;
        }
        QLabel[role="cardBadge"] {
            background: #eef1f9;
            color: #7f8399;
            border-radius: 12px;
            padding: 4px 12px;
            font-weight: 600;
        }
    )"));

    auto *central = new QWidget(this);
    auto *rootLayout = new QVBoxLayout(central);
    rootLayout->setSpacing(28);
    rootLayout->setContentsMargins(32, 28, 32, 32);

    auto *titleLabel = new QLabel(QStringLiteral("S Parameter Test System"), central);
    titleLabel->setObjectName(QStringLiteral("TitleLabel"));

    auto *subtitleLabel = new QLabel(QStringLiteral("Professional touchpad GUI for S parameter measurements and analysis"), central);
    subtitleLabel->setObjectName(QStringLiteral("SubtitleLabel"));

    rootLayout->addWidget(titleLabel);
    rootLayout->setAlignment(titleLabel, Qt::AlignHCenter);
    rootLayout->addWidget(subtitleLabel);
    rootLayout->setAlignment(subtitleLabel, Qt::AlignHCenter);

    auto *infoRow = new QHBoxLayout;
    infoRow->setSpacing(24);
    infoRow->addWidget(createCalibrationPanel(), 1);
    infoRow->addWidget(createTestControlPanel(), 1);

    rootLayout->addLayout(infoRow);

    rootLayout->addWidget(createViewTogglePanel(central), 1);

    setCentralWidget(central);

    appendStatusMessage(QStringLiteral("S Parameter Test System initialized"));
}

QWidget *MainWindow::createCalibrationPanel()
{
    auto *frame = new QFrame(this);
    frame->setProperty("panel", true);

    auto *layout = new QVBoxLayout(frame);
    layout->setSpacing(16);
    layout->setContentsMargins(24, 24, 24, 24);

    auto *headerLayout = new QHBoxLayout;
    headerLayout->setSpacing(12);

    auto *title = new QLabel(QStringLiteral("Calibration Control"), frame);
    title->setProperty("role", QStringLiteral("cardTitle"));

    m_calibrationStatusBadge = new QLabel(QStringLiteral("Ready"), frame);
    m_calibrationStatusBadge->setProperty("role", QStringLiteral("badge"));

    headerLayout->addWidget(title);
    headerLayout->addStretch();
    headerLayout->addWidget(m_calibrationStatusBadge);

    layout->addLayout(headerLayout);

    m_loadCalibrationButton = new QPushButton(QStringLiteral("Load Calibration"), frame);
    m_loadCalibrationButton->setObjectName(QStringLiteral("ActionButton"));
    m_loadCalibrationButton->setIcon(QIcon(QStringLiteral(":/icons/calibration_load.svg")));
    m_loadCalibrationButton->setIconSize(QSize(20, 20));
    connect(m_loadCalibrationButton, &QPushButton::clicked, this, &MainWindow::onLoadCalibration);

    m_setupCalibrationButton = new QPushButton(QStringLiteral("Setup New"), frame);
    m_setupCalibrationButton->setObjectName(QStringLiteral("SecondaryButton"));
    m_setupCalibrationButton->setIcon(QIcon(QStringLiteral(":/icons/calibration_setup.svg")));
    m_setupCalibrationButton->setIconSize(QSize(20, 20));
    connect(m_setupCalibrationButton, &QPushButton::clicked, this, &MainWindow::onSetupCalibration);

    auto *buttonRow = new QHBoxLayout;
    buttonRow->setSpacing(12);
    buttonRow->addWidget(m_loadCalibrationButton, 1);
    buttonRow->addWidget(m_setupCalibrationButton, 1);

    layout->addLayout(buttonRow);

    auto *placeholder = new QFrame(frame);
    placeholder->setProperty("role", QStringLiteral("chartArea"));

    layout->addWidget(placeholder, 1);

    return frame;
}

QWidget *MainWindow::createTestControlPanel()
{
    auto *frame = new QFrame(this);
    frame->setProperty("panel", true);

    auto *layout = new QVBoxLayout(frame);
    layout->setSpacing(18);
    layout->setContentsMargins(24, 24, 24, 24);

    auto *headerLayout = new QHBoxLayout;
    headerLayout->setSpacing(12);

    auto *title = new QLabel(QStringLiteral("S Parameter Test Control"), frame);
    title->setProperty("role", QStringLiteral("cardTitle"));

    m_testStateBadge = new QLabel(QStringLiteral("Ready"), frame);
    m_testStateBadge->setProperty("role", QStringLiteral("badge"));

    headerLayout->addWidget(title);
    headerLayout->addStretch();
    headerLayout->addWidget(m_testStateBadge);

    layout->addLayout(headerLayout);

    auto *formLayout = new QGridLayout;
    formLayout->setHorizontalSpacing(16);
    formLayout->setVerticalSpacing(12);

    auto addSpinBoxRow = [&](int row, const QString &labelText, QWidget *editor) {
        auto *label = new QLabel(labelText, frame);
        label->setProperty("role", QStringLiteral("cardSubtitle"));
        formLayout->addWidget(label, row, 0);
        formLayout->addWidget(editor, row, 1);
    };

    m_startFrequencySpin = new QDoubleSpinBox(frame);
    m_startFrequencySpin->setSuffix(QStringLiteral(" GHz"));
    m_startFrequencySpin->setDecimals(3);
    m_startFrequencySpin->setRange(0.001, 40.0);
    m_startFrequencySpin->setValue(1.0);
    m_startFrequencySpin->setSingleStep(0.1);

    m_stopFrequencySpin = new QDoubleSpinBox(frame);
    m_stopFrequencySpin->setSuffix(QStringLiteral(" GHz"));
    m_stopFrequencySpin->setDecimals(3);
    m_stopFrequencySpin->setRange(0.001, 40.0);
    m_stopFrequencySpin->setValue(6.0);
    m_stopFrequencySpin->setSingleStep(0.1);

    m_pointsSpin = new QSpinBox(frame);
    m_pointsSpin->setRange(1, 2001);
    m_pointsSpin->setValue(201);
    m_pointsSpin->setSingleStep(10);

    addSpinBoxRow(0, QStringLiteral("Start Frequency"), m_startFrequencySpin);
    addSpinBoxRow(1, QStringLiteral("Stop Frequency"), m_stopFrequencySpin);
    addSpinBoxRow(2, QStringLiteral("Number of Points"), m_pointsSpin);

    formLayout->setColumnStretch(1, 1);

    layout->addLayout(formLayout);

    auto *parameterLabel = new QLabel(QStringLiteral("S Parameters to Test"), frame);
    parameterLabel->setProperty("role", QStringLiteral("cardSubtitle"));
    layout->addWidget(parameterLabel);

    auto *parameterRow = new QHBoxLayout;
    parameterRow->setSpacing(12);

    const QStringList parameterIds = {QStringLiteral("S11"), QStringLiteral("S12"), QStringLiteral("S21"), QStringLiteral("S22")};
    for (const auto &id : parameterIds) {
        auto *button = new QToolButton(frame);
        button->setText(id);
        button->setCheckable(true);
        button->setChecked(id == QStringLiteral("S11"));
        connect(button, &QToolButton::toggled, this, &MainWindow::onSParameterToggled);
        parameterRow->addWidget(button);
        m_parameterButtons.append(button);
    }
    parameterRow->addStretch();

    layout->addLayout(parameterRow);

    auto *actionsRow = new QHBoxLayout;
    actionsRow->setSpacing(12);

    m_startButton = new QPushButton(QStringLiteral("Start Test"), frame);
    m_startButton->setObjectName(QStringLiteral("ActionButton"));
    m_startButton->setIcon(QIcon(QStringLiteral(":/icons/start.svg")));
    m_startButton->setIconSize(QSize(20, 20));
    connect(m_startButton, &QPushButton::clicked, this, &MainWindow::onStartTest);

    m_stopButton = new QPushButton(QStringLiteral("Stop"), frame);
    m_stopButton->setObjectName(QStringLiteral("StopButton"));
    m_stopButton->setEnabled(false);
    m_stopButton->setIcon(QIcon(QStringLiteral(":/icons/stop.svg")));
    m_stopButton->setIconSize(QSize(18, 18));
    connect(m_stopButton, &QPushButton::clicked, this, &MainWindow::onStopTest);

    m_resetButton = new QPushButton(QStringLiteral("Reset"), frame);
    m_resetButton->setObjectName(QStringLiteral("SecondaryButton"));
    m_resetButton->setIcon(QIcon(QStringLiteral(":/icons/reset.svg")));
    m_resetButton->setIconSize(QSize(18, 18));
    connect(m_resetButton, &QPushButton::clicked, this, &MainWindow::onResetTest);

    actionsRow->addWidget(m_startButton, 1);
    actionsRow->addWidget(m_stopButton, 1);
    actionsRow->addWidget(m_resetButton, 1);

    layout->addLayout(actionsRow);

    m_testHintLabel = new QLabel(QStringLiteral("Please complete calibration before starting tests"), frame);
    m_testHintLabel->setProperty("role", QStringLiteral("hint"));
    m_testHintLabel->setWordWrap(true);

    layout->addWidget(m_testHintLabel);

    return frame;
}

QWidget *MainWindow::createViewTogglePanel(QWidget *parent)
{
    auto *container = new QWidget(parent);
    auto *containerLayout = new QVBoxLayout(container);
    containerLayout->setContentsMargins(0, 0, 0, 0);
    containerLayout->setSpacing(18);

    auto *toggleFrame = new QFrame(container);
    toggleFrame->setProperty("role", QStringLiteral("viewToggleContainer"));

    auto *toggleLayout = new QHBoxLayout(toggleFrame);
    toggleLayout->setContentsMargins(6, 6, 6, 6);
    toggleLayout->setSpacing(12);

    m_chartsToggleButton = new QPushButton(QIcon(QStringLiteral(":/icons/charts.svg")), QStringLiteral("S Parameter Charts"), toggleFrame);
    m_chartsToggleButton->setProperty("role", QStringLiteral("segmented"));
    m_chartsToggleButton->setCheckable(true);
    m_chartsToggleButton->setChecked(true);
    m_chartsToggleButton->setIconSize(QSize(20, 20));
    m_chartsToggleButton->setMinimumHeight(48);
    m_chartsToggleButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    m_statusToggleButton = new QPushButton(QIcon(QStringLiteral(":/icons/status.svg")), QStringLiteral("System Status"), toggleFrame);
    m_statusToggleButton->setProperty("role", QStringLiteral("segmented"));
    m_statusToggleButton->setCheckable(true);
    m_statusToggleButton->setIconSize(QSize(20, 20));
    m_statusToggleButton->setMinimumHeight(48);
    m_statusToggleButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    m_viewToggleGroup = new QButtonGroup(this);
    m_viewToggleGroup->setExclusive(true);
    m_viewToggleGroup->addButton(m_chartsToggleButton, 0);
    m_viewToggleGroup->addButton(m_statusToggleButton, 1);

    connect(m_chartsToggleButton, &QPushButton::toggled, this, [this](bool checked) {
        if (checked && m_contentStack) {
            m_contentStack->setCurrentIndex(0);
        }
    });

    connect(m_statusToggleButton, &QPushButton::toggled, this, [this](bool checked) {
        if (checked && m_contentStack) {
            m_contentStack->setCurrentIndex(1);
        }
    });

    toggleLayout->addWidget(m_chartsToggleButton);
    toggleLayout->addWidget(m_statusToggleButton);

    containerLayout->addWidget(toggleFrame);

    m_contentStack = new QStackedWidget(container);
    m_contentStack->addWidget(createChartsPanel());
    m_contentStack->addWidget(createStatusPanel());
    containerLayout->addWidget(m_contentStack, 1);

    connect(m_viewToggleGroup, QOverload<int>::of(&QButtonGroup::idClicked), this, [this](int id) {
        if (m_contentStack) {
            m_contentStack->setCurrentIndex(id);
        }
    });

    return container;
}

QWidget *MainWindow::createChartsPanel()
{
    auto *panel = new QWidget(this);
    auto *layout = new QGridLayout(panel);
    layout->setContentsMargins(8, 16, 8, 16);
    layout->setHorizontalSpacing(18);
    layout->setVerticalSpacing(18);

    struct ChartDescriptor {
        QString id;
        QString description;
    };

    const QList<ChartDescriptor> charts = {
        {QStringLiteral("S11"), QStringLiteral("Input Return Loss")},
        {QStringLiteral("S12"), QStringLiteral("Reverse Transmission")},
        {QStringLiteral("S21"), QStringLiteral("Forward Gain")},
        {QStringLiteral("S22"), QStringLiteral("Output Return Loss")}
    };

    for (int index = 0; index < charts.size(); ++index) {
        auto *card = createChartCard(charts[index].id, charts[index].description);
        int row = index / 2;
        int column = index % 2;
        layout->addWidget(card, row, column);
    }

    layout->setRowStretch(0, 1);
    layout->setRowStretch(1, 1);
    layout->setColumnStretch(0, 1);
    layout->setColumnStretch(1, 1);

    return panel;
}

QWidget *MainWindow::createStatusPanel()
{
    auto *panel = new QWidget(this);
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);

    auto *headerRow = new QHBoxLayout;
    headerRow->setSpacing(12);

    auto *title = new QLabel(QStringLiteral("System Status"), panel);
    title->setProperty("role", QStringLiteral("cardTitle"));

    headerRow->addWidget(title);
    headerRow->addStretch();

    layout->addLayout(headerRow);

    auto *metaRow = new QHBoxLayout;
    metaRow->setSpacing(20);

    auto makeMetaChip = [&](const QIcon &icon,
                            const QString &caption,
                            const QString &valueText,
                            const QString &valueObjectName) {
        auto *chip = new QWidget(panel);
        chip->setProperty("role", QStringLiteral("metaChip"));

        auto *chipLayout = new QHBoxLayout(chip);
        chipLayout->setContentsMargins(16, 10, 16, 10);
        chipLayout->setSpacing(12);

        auto *iconLabel = new QLabel(chip);
        iconLabel->setPixmap(icon.pixmap(22, 22));
        iconLabel->setFixedSize(22, 22);
        iconLabel->setAlignment(Qt::AlignCenter);
        chipLayout->addWidget(iconLabel);

        auto *textLayout = new QVBoxLayout;
        textLayout->setContentsMargins(0, 0, 0, 0);
        textLayout->setSpacing(2);

        auto *captionLabel = new QLabel(caption, chip);
        captionLabel->setProperty("role", QStringLiteral("cardSubtitle"));
        textLayout->addWidget(captionLabel);

        auto *valueLabel = new QLabel(valueText, chip);
        valueLabel->setProperty("role", QStringLiteral("cardTitle"));
        if (!valueObjectName.isEmpty()) {
            valueLabel->setObjectName(valueObjectName);
        }
        textLayout->addWidget(valueLabel);

        chipLayout->addLayout(textLayout);

        return chip;
    };

    metaRow->addWidget(makeMetaChip(QIcon(QStringLiteral(":/icons/last-update.svg")),
                                    QStringLiteral("Last Update"),
                                    QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss")),
                                    QStringLiteral("LastUpdateValue")));

    metaRow->addWidget(makeMetaChip(QIcon(QStringLiteral(":/icons/status.svg")),
                                    QStringLiteral("Total Messages"),
                                    QStringLiteral("1"),
                                    QStringLiteral("MessageCountValue")));

    metaRow->addStretch();

    layout->addLayout(metaRow);

    auto *activityLabel = new QLabel(QStringLiteral("Activity Log"), panel);
    activityLabel->setProperty("role", QStringLiteral("cardSubtitle"));
    layout->addWidget(activityLabel);

    m_activityLog = new QListWidget(panel);
    layout->addWidget(m_activityLog, 1);

    return panel;
}

QWidget *MainWindow::createChartCard(const QString &parameterId, const QString &description)
{
    auto *frame = new QFrame(this);
    frame->setProperty("panel", true);

    auto *layout = new QVBoxLayout(frame);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(12);

    auto *headerRow = new QHBoxLayout;
    headerRow->setSpacing(12);

    auto *title = new QLabel(parameterId, frame);
    title->setProperty("role", QStringLiteral("cardTitle"));

    auto *subtitle = new QLabel(description, frame);
    subtitle->setProperty("role", QStringLiteral("cardSubtitle"));

    auto *badge = new QLabel(QStringLiteral("Inactive"), frame);
    badge->setProperty("role", QStringLiteral("cardBadge"));

    headerRow->addWidget(title);
    headerRow->addWidget(subtitle);
    headerRow->addStretch();
    headerRow->addWidget(badge);

    layout->addLayout(headerRow);

    auto *chartArea = new QFrame(frame);
    chartArea->setProperty("role", QStringLiteral("chartArea"));

    layout->addWidget(chartArea, 1);

    auto *legendRow = new QHBoxLayout;
    legendRow->setSpacing(12);

    auto *magnitude = new QLabel(QStringLiteral("o Magnitude"), frame);
    magnitude->setProperty("role", QStringLiteral("cardSubtitle"));

    auto *phase = new QLabel(QStringLiteral("o Phase"), frame);
    phase->setProperty("role", QStringLiteral("cardSubtitle"));

    legendRow->addWidget(magnitude);
    legendRow->addWidget(phase);
    legendRow->addStretch();

    layout->addLayout(legendRow);

    return frame;
}

void MainWindow::appendStatusMessage(const QString &message)
{
    if (!m_activityLog) {
        return;
    }

    const QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    m_activityLog->addItem(QStringLiteral("%1  %2").arg(timestamp, message));

    if (auto *valueLabel = findChild<QLabel *>(QStringLiteral("MessageCountValue"))) {
        valueLabel->setText(QString::number(m_activityLog->count()));
    }
    if (auto *lastUpdate = findChild<QLabel *>(QStringLiteral("LastUpdateValue"))) {
        lastUpdate->setText(timestamp);
    }
}

void MainWindow::updateCalibrationStatus(const QString &status, const QString &styleSheet)
{
    if (!m_calibrationStatusBadge) {
        return;
    }

    m_calibrationStatusBadge->setText(status);
    m_calibrationStatusBadge->setStyleSheet(styleSheet);
}

void MainWindow::updateTestState(const QString &state)
{
    if (!m_testStateBadge) {
        return;
    }

    m_testStateBadge->setText(state);
}

void MainWindow::updateControlsForRunning(bool running)
{
    m_startButton->setEnabled(!running);
    m_stopButton->setEnabled(running);
    m_resetButton->setEnabled(!running);
    m_loadCalibrationButton->setEnabled(!running);
    m_setupCalibrationButton->setEnabled(!running);

    for (auto *button : m_parameterButtons) {
        button->setEnabled(!running);
    }
}

void MainWindow::onLoadCalibration()
{
    appendStatusMessage(QStringLiteral("Load calibration action triggered"));
    updateCalibrationStatus(QStringLiteral("Loaded"), QStringLiteral("background:#eefaf1; color:#1d8a43; padding:4px 14px; border-radius:14px; font-weight:600;"));
    m_testHintLabel->setText(QStringLiteral("Calibration loaded. You can start a test when ready."));
}

void MainWindow::onSetupCalibration()
{
    appendStatusMessage(QStringLiteral("Calibration setup requested"));
    updateCalibrationStatus(QStringLiteral("Pending"), QStringLiteral("background:#fff4db; color:#a16a00; padding:4px 14px; border-radius:14px; font-weight:600;"));
    m_testHintLabel->setText(QStringLiteral("Follow the calibration wizard to complete setup."));
}

void MainWindow::onStartTest()
{
    bool parameterSelected = false;
    for (auto *button : m_parameterButtons) {
        if (button->isChecked()) {
            parameterSelected = true;
            break;
        }
    }

    if (!parameterSelected) {
        m_testHintLabel->setText(QStringLiteral("Select at least one S parameter before starting the test."));
        return;
    }

    appendStatusMessage(QStringLiteral("Test started (%1 points, %2 GHz -> %3 GHz)")
                        .arg(m_pointsSpin->value())
                        .arg(m_startFrequencySpin->value(), 0, 'f', 3)
                        .arg(m_stopFrequencySpin->value(), 0, 'f', 3));

    updateTestState(QStringLiteral("Running"));
    updateControlsForRunning(true);
    m_testHintLabel->setText(QStringLiteral("Sweep in progress... You can stop the test at any time."));
}

void MainWindow::onStopTest()
{
    appendStatusMessage(QStringLiteral("Test stop requested"));
    updateTestState(QStringLiteral("Stopping"));
    updateControlsForRunning(false);
    m_testHintLabel->setText(QStringLiteral("Test stopped. Review the charts or run another sweep."));
}

void MainWindow::onResetTest()
{
    for (auto *button : m_parameterButtons) {
        button->setChecked(false);
    }
    if (!m_parameterButtons.isEmpty()) {
        m_parameterButtons.front()->setChecked(true);
    }

    m_startFrequencySpin->setValue(1.0);
    m_stopFrequencySpin->setValue(6.0);
    m_pointsSpin->setValue(201);

    appendStatusMessage(QStringLiteral("Parameters reset to defaults"));
    updateTestState(QStringLiteral("Ready"));
    updateControlsForRunning(false);
    m_testHintLabel->setText(QStringLiteral("Please complete calibration before starting tests"));
}

void MainWindow::onSParameterToggled(bool /*checked*/)
{
    QStringList selected;
    for (auto *button : m_parameterButtons) {
        if (button->isChecked()) {
            selected.append(button->text());
        }
    }

    if (selected.isEmpty()) {
        m_testHintLabel->setText(QStringLiteral("Select at least one S parameter before starting the test."));
    } else {
        m_testHintLabel->setText(QStringLiteral("Ready to sweep: %1").arg(selected.join(QStringLiteral(", "))));
    }
}
