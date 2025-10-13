#include "MainWindow.hpp"


#include <QAbstractItemView>
#include <QButtonGroup>
#include <QCheckBox>
#include <QtCharts/QAbstractAxis>
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
#include <QScrollArea>
#include <QGraphicsView>
#include <QGestureEvent>
#include <QMouseEvent>
#include <QPanGesture>
#include <QPinchGesture>
#include <QPainter>
#include <QPixmap>
#include <QPen>
#include <QPointF>
#include <QRubberBand>
#include <QVector>
#include <QWheelEvent>
#include <QSize>
#include <QSizePolicy>
#include <QSpinBox>
#include <QStackedWidget>
#include <QToolButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QStyle>
#include <QSignalBlocker>
#include <QStringList>
#include <QFileDialog>
#include <QListWidgetItem>

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <chrono>
#include <filesystem>
#include <functional>
#include <limits>
#include <sstream>
#include <utility>
#include <system_error>
#include <vector>

namespace
{

QIcon makeSeriesIcon(const QColor &color, Qt::PenStyle style)
{
    QPixmap pixmap(28, 12);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    QPen pen(color);
    pen.setWidthF(2.5);
    pen.setStyle(style);
    painter.setPen(pen);

    const qreal margin = 3.0;
    const QPointF startPoint(margin, pixmap.height() / 2.0);
    const QPointF endPoint(pixmap.width() - margin, pixmap.height() / 2.0);
    painter.drawLine(startPoint, endPoint);
    painter.end();

    return QIcon(pixmap);
}

class InteractiveChartView : public QChartView
{
public:
    using ResetCallback = std::function<void()>;

    explicit InteractiveChartView(QChart *chart, QWidget *parent = nullptr)
        : QChartView(chart, parent)
    {
        setRubberBand(QChartView::NoRubberBand);
        setDragMode(QGraphicsView::ScrollHandDrag);
        setInteractive(true);
        setAttribute(Qt::WA_AcceptTouchEvents, true);
        grabGesture(Qt::PinchGesture);
        grabGesture(Qt::PanGesture);
        viewport()->setCursor(Qt::OpenHandCursor);
    }

    void setResetCallback(ResetCallback callback)
    {
        m_resetCallback = std::move(callback);
    }

protected:
    bool event(QEvent *event) override
    {
        if (event->type() == QEvent::Gesture) {
            if (handleGesture(static_cast<QGestureEvent *>(event))) {
                return true;
            }
        }
        return QChartView::event(event);
    }

    void wheelEvent(QWheelEvent *event) override
    {
        if (!chart()) {
            QChartView::wheelEvent(event);
            return;
        }

        const QPointF delta = !event->pixelDelta().isNull() ? event->pixelDelta() : event->angleDelta();
        const qreal dy = delta.y();
        const qreal dx = delta.x();

        if (event->modifiers().testFlag(Qt::ControlModifier)) {
            const qreal effectiveDelta = std::abs(dy) > std::numeric_limits<qreal>::epsilon() ? dy : dx;
            if (std::abs(effectiveDelta) > std::numeric_limits<qreal>::epsilon()) {
                const qreal factor = effectiveDelta > 0 ? 1.1 : 0.9;
                chart()->zoom(factor);
                event->accept();
                return;
            }
        } else if (event->modifiers().testFlag(Qt::ShiftModifier)) {
            qreal horizontal = std::abs(dy) > std::numeric_limits<qreal>::epsilon() ? dy : dx;
            if (std::abs(horizontal) > std::numeric_limits<qreal>::epsilon()) {
                const qreal scrollFactor = 0.6;
                chart()->scroll(-horizontal * scrollFactor, 0.0);
                event->accept();
                return;
            }
        }

        QChartView::wheelEvent(event);
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton && chart()) {
            m_leftPanActive = true;
            m_rubberActive = false;
            m_lastPanPosition = event->pos();
            viewport()->setCursor(Qt::ClosedHandCursor);
            event->accept();
            return;
        }

        if (event->button() == Qt::RightButton && chart()) {
            if (!m_rubberBand) {
                m_rubberBand = new QRubberBand(QRubberBand::Rectangle, viewport());
            }
            m_rubberOrigin = event->pos();
            m_rubberActive = true;
            m_rubberBand->setGeometry(QRect(m_rubberOrigin, QSize()));
            m_rubberBand->show();
            event->accept();
            return;
        }
        QChartView::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (m_leftPanActive && chart()) {
            const QPoint currentPos = event->pos();
            const QPoint delta = currentPos - m_lastPanPosition;
            if (!delta.isNull()) {
                chart()->scroll(-delta.x(), delta.y());
                m_lastPanPosition = currentPos;
            }
            event->accept();
            return;
        }

        if (m_rubberActive && m_rubberBand) {
            m_rubberBand->setGeometry(QRect(m_rubberOrigin, event->pos()).normalized());
            event->accept();
            return;
        }
        QChartView::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton && m_leftPanActive) {
            m_leftPanActive = false;
            viewport()->setCursor(Qt::OpenHandCursor);
            event->accept();
            return;
        }

        if (event->button() == Qt::RightButton && m_rubberActive) {
            if (m_rubberBand) {
                m_rubberBand->hide();
                const QRect selection = QRect(m_rubberOrigin, event->pos()).normalized();
                applyRubberBandZoom(selection);
            }
            m_rubberActive = false;
            event->accept();
            return;
        }
        QChartView::mouseReleaseEvent(event);
    }

    void mouseDoubleClickEvent(QMouseEvent *event) override
    {
        if (m_resetCallback) {
            m_resetCallback();
            event->accept();
            return;
        }
        QChartView::mouseDoubleClickEvent(event);
    }

private:
    bool handleGesture(QGestureEvent *gestureEvent)
    {
        if (auto *pinch = static_cast<QPinchGesture *>(gestureEvent->gesture(Qt::PinchGesture))) {
            if (chart() && (pinch->changeFlags() & QPinchGesture::ScaleFactorChanged)) {
                const qreal factor = pinch->scaleFactor();
                if (factor > 0.0 && std::abs(factor - 1.0) > std::numeric_limits<qreal>::epsilon()) {
                    chart()->zoom(factor);
                }
            }
            gestureEvent->accept(pinch);
            return true;
        }
        if (auto *pan = static_cast<QPanGesture *>(gestureEvent->gesture(Qt::PanGesture))) {
            if (chart()) {
                const QPointF delta = pan->delta();
                chart()->scroll(-delta.x(), delta.y());
            }
            gestureEvent->accept(pan);
            return true;
        }
        return false;
    }

    void applyRubberBandZoom(const QRect &selection)
    {
        if (!chart()) {
            return;
        }

        QRectF plot = chart()->plotArea();
        if (plot.width() <= 0.0 || plot.height() <= 0.0) {
            return;
        }

        QRectF selected = QRectF(selection).intersected(plot);
        if (selected.width() < 5.0 || selected.height() < 5.0) {
            return;
        }

        const auto horizontalAxes = chart()->axes(Qt::Horizontal);
        if (!horizontalAxes.isEmpty()) {
            if (auto *axis = qobject_cast<QValueAxis *>(horizontalAxes.first())) {
                const double span = axis->max() - axis->min();
                if (span > std::numeric_limits<double>::epsilon()) {
                    const double leftRatio = (selected.left() - plot.left()) / plot.width();
                    const double rightRatio = (selected.right() - plot.left()) / plot.width();
                    const double newMin = axis->min() + std::clamp(leftRatio, 0.0, 1.0) * span;
                    const double newMax = axis->min() + std::clamp(rightRatio, 0.0, 1.0) * span;
                    if (newMax - newMin > std::numeric_limits<double>::epsilon()) {
                        axis->setRange(newMin, newMax);
                    }
                }
            }
        }

        const auto verticalAxes = chart()->axes(Qt::Vertical);
        for (QAbstractAxis *abstractAxis : verticalAxes) {
            if (auto *axis = qobject_cast<QValueAxis *>(abstractAxis)) {
                const double span = axis->max() - axis->min();
                if (span <= std::numeric_limits<double>::epsilon()) {
                    continue;
                }
                const double topRatio = (selected.top() - plot.top()) / plot.height();
                const double bottomRatio = (selected.bottom() - plot.top()) / plot.height();
                const double clampedTop = std::clamp(topRatio, 0.0, 1.0);
                const double clampedBottom = std::clamp(bottomRatio, 0.0, 1.0);
                const double valueTop = axis->min() + (1.0 - clampedTop) * span;
                const double valueBottom = axis->min() + (1.0 - clampedBottom) * span;
                if (std::abs(valueTop - valueBottom) > std::numeric_limits<double>::epsilon()) {
                    const double newMin = std::min(valueTop, valueBottom);
                    const double newMax = std::max(valueTop, valueBottom);
                    axis->setRange(newMin, newMax);
                }
            }
        }
    }

    ResetCallback m_resetCallback;
    QRubberBand *m_rubberBand = nullptr;
    QPoint m_rubberOrigin;
    bool m_rubberActive = false;
    bool m_leftPanActive = false;
    QPoint m_lastPanPosition;
};

struct ParameterOutcome
{
    std::string name;
    double worstDb = -300.0;
    double failFrequencyHz = 0.0;
    bool pass = true;
};

struct SweepEvaluationSummary
{
    bool overallPass = true;
    std::array<ParameterOutcome, 4> parameters{};
};

constexpr double kDefaultThresholdDb = -10.0;

SweepEvaluationSummary computeSweepSummary(const std::vector<librevna::headless::VNAMeasurement> &measurements,
                                           double thresholdDb)
{
    SweepEvaluationSummary summary{};
    summary.parameters = {{{"S11", -300.0, 0.0, true},
                           {"S12", -300.0, 0.0, true},
                           {"S21", -300.0, 0.0, true},
                           {"S22", -300.0, 0.0, true}}};

    for (const auto &measurement : measurements) {
        for (auto &parameter : summary.parameters) {
            const auto value = measurement.get(parameter.name);
            const double magnitude = std::abs(value);
            const double db = magnitude <= 0.0 ? -300.0 : 20.0 * std::log10(magnitude);

            if (db > parameter.worstDb) {
                parameter.worstDb = db;
                parameter.failFrequencyHz = measurement.frequency;
            }
            if (db > thresholdDb) {
                parameter.pass = false;
                summary.overallPass = false;
            }
        }
    }

    return summary;
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setupUi();
}

MainWindow::~MainWindow()
{
    m_cancelRequested = true;
    ensureSweepThreadFinished();
    if (m_hostCore.is_connected()) {
        m_hostCore.disconnect();
    }
}

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
        #MainCentral QPushButton {
            background: #181823;
            color: #ffffff;
            border-radius: 12px;
            min-height: 46px;
            padding: 10px 24px;
            font-size: 16px;
            font-weight: 600;
        }
        #MainCentral QPushButton::icon {
            subcontrol-origin: padding;
            subcontrol-position: left center;
            margin-right: 10px;
        }
        #MainCentral QPushButton#SecondaryButton {
            background: #ffffff;
            color: #181823;
            border: 1px solid #daddeb;
        }
        #MainCentral QPushButton#ActionButton {
            background: #181823;
        }
        #MainCentral QPushButton#StopButton {
            background: #ffd9e2;
            color: #ca1f4a;
        }
        #MainCentral QPushButton[role="segmented"] {
            background: transparent;
            color: #6f7285;
            border: none;
            border-radius: 20px;
            min-height: 44px;
            padding: 8px 24px;
            font-size: 17px;
            font-weight: 600;
        }
        #MainCentral QPushButton[role="segmented"]::icon {
            subcontrol-origin: padding;
            subcontrol-position: left center;
            margin-right: 12px;
        }
        #MainCentral QPushButton[role="segmented"]:checked {
            background: #ffffff;
            color: #181823;
        }
        #MainCentral QPushButton[role="segmented"]:hover {
            background: rgba(255, 255, 255, 0.7);
        }
        #MainCentral QPushButton:disabled {
            background: #d8dbe8;
            color: #7f8399;
        }
        #MainCentral QToolButton {
            background: #f4f5ff;
            border: 1px solid transparent;
            border-radius: 14px;
            padding: 6px 16px;
            min-height: 36px;
            font-size: 15px;
            font-weight: 600;
        }
        #MainCentral QToolButton:checked {
            background: #181823;
            color: #ffffff;
        }
        QLabel[role="hint"] {
            background: #fff6db;
            border: 1px solid #ffe4aa;
            color: #a16a00;
            border-radius: 14px;
            padding: 12px 16px;
            font-size: 16px;
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
            min-height: 320px;
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
        QLabel[role="cardBadge"][state="inactive"],
        QLabel[role="cardBadge"][state="pending"] {
            background: #eef1f9;
            color: #7f8399;
        }
        QLabel[role="cardBadge"][state="pass"] {
            background: #eefaf1;
            color: #1d8a43;
        }
        QLabel[role="cardBadge"][state="fail"] {
            background: #fdecef;
            color: #d12b57;
        }
    )"));

    auto *scrollArea = new QScrollArea(this);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto *central = new QWidget(scrollArea);
    central->setObjectName(QStringLiteral("MainCentral"));
    central->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

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

    scrollArea->setWidget(central);
    setCentralWidget(scrollArea);

    m_calibrationDirectory = findCalibrationDirectory();
    refreshCalibrationList();
    updateCalibrationStatus(QStringLiteral("Not Loaded"),
                            QStringLiteral("background:#fff4db; color:#ad7300; padding:4px 14px; border-radius:14px; font-weight:600;"));
    updateDeviceStatus(QStringLiteral("No Device Detected"),
                       QStringLiteral("background:#fff4db; color:#ad7300; padding:4px 14px; border-radius:14px; font-weight:600;"));

    QTimer::singleShot(0, this, &MainWindow::onScanDevices);

    m_activeParameters.clear();
    resetCharts();
    appendStatusMessage(QStringLiteral("S Parameter Test System initialized"));
}

QWidget *MainWindow::createCalibrationPanel()
{
    auto *frame = new QFrame(this);
    frame->setProperty("panel", true);
    frame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    auto *layout = new QVBoxLayout(frame);
    layout->setSpacing(16);
    layout->setContentsMargins(24, 24, 24, 24);

    auto *headerLayout = new QHBoxLayout;
    headerLayout->setSpacing(12);

    auto *title = new QLabel(QStringLiteral("Calibration & Device Control"), frame);
    title->setProperty("role", QStringLiteral("cardTitle"));

    m_deviceStatusBadge = new QLabel(QStringLiteral("No Device"), frame);
    m_deviceStatusBadge->setProperty("role", QStringLiteral("badge"));

    headerLayout->addWidget(title);
    headerLayout->addStretch();

    layout->addLayout(headerLayout);

    auto *primaryButtonRow = new QHBoxLayout;
    primaryButtonRow->setSpacing(12);

    m_scanDevicesButton = new QPushButton(QStringLiteral("Scan Devices"), frame);
    m_scanDevicesButton->setObjectName(QStringLiteral("SecondaryButton"));
    connect(m_scanDevicesButton, &QPushButton::clicked, this, &MainWindow::onScanDevices);

    m_loadCalibrationButton = new QPushButton(QStringLiteral("Load External"), frame);
    m_loadCalibrationButton->setObjectName(QStringLiteral("SecondaryButton"));
    connect(m_loadCalibrationButton, &QPushButton::clicked, this, &MainWindow::onLoadCalibration);

    primaryButtonRow->addWidget(m_scanDevicesButton, 1);
    primaryButtonRow->addWidget(m_loadCalibrationButton, 1);

    layout->addLayout(primaryButtonRow);

    auto *secondaryButtonRow = new QHBoxLayout;
    secondaryButtonRow->setSpacing(12);

    m_connectDeviceButton = new QPushButton(QStringLiteral("Connect"), frame);
    m_connectDeviceButton->setObjectName(QStringLiteral("ActionButton"));
    m_connectDeviceButton->setEnabled(false);
    connect(m_connectDeviceButton, &QPushButton::clicked, this, [this]() {
        if (!m_deviceList) {
            return;
        }
        const auto selectedItems = m_deviceList->selectedItems();
        if (selectedItems.isEmpty()) {
            return;
        }
        const auto index = selectedItems.first()->data(Qt::UserRole).toInt();
        if (index < 0 || index >= static_cast<int>(m_discoveredDevices.size())) {
            return;
        }
        connectToDevice(m_discoveredDevices[static_cast<std::size_t>(index)]);
    });

    m_setupCalibrationButton = new QPushButton(QStringLiteral("Setup New"), frame);
    m_setupCalibrationButton->setObjectName(QStringLiteral("ActionButton"));
    connect(m_setupCalibrationButton, &QPushButton::clicked, this, &MainWindow::onSetupCalibration);

    secondaryButtonRow->addWidget(m_connectDeviceButton, 1);
    secondaryButtonRow->addWidget(m_setupCalibrationButton, 1);

    layout->addLayout(secondaryButtonRow);

    auto *selectionRow = new QHBoxLayout;
    selectionRow->setSpacing(16);

    auto *deviceColumn = new QVBoxLayout;
    deviceColumn->setSpacing(8);

    auto *deviceHeaderRow = new QHBoxLayout;
    deviceHeaderRow->setSpacing(12);

    auto *deviceListLabel = new QLabel(QStringLiteral("Detected LibreVNA Devices"), frame);
    deviceListLabel->setProperty("role", QStringLiteral("cardSubtitle"));

    deviceHeaderRow->addWidget(deviceListLabel);
    deviceHeaderRow->addStretch();
    deviceHeaderRow->addWidget(m_deviceStatusBadge);

    deviceColumn->addLayout(deviceHeaderRow);

    m_deviceList = new QListWidget(frame);
    m_deviceList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_deviceList->setUniformItemSizes(true);
    connect(m_deviceList, &QListWidget::itemSelectionChanged, this, &MainWindow::onDeviceSelectionChanged);
    connect(m_deviceList, &QListWidget::itemActivated, this, &MainWindow::onDeviceActivated);
    deviceColumn->addWidget(m_deviceList, 1);

    auto *calibrationColumn = new QVBoxLayout;
    calibrationColumn->setSpacing(8);

    auto *calibrationHeaderRow = new QHBoxLayout;
    calibrationHeaderRow->setSpacing(12);

    auto *calibrationLabel = new QLabel(QStringLiteral("Calibration Files"), frame);
    calibrationLabel->setProperty("role", QStringLiteral("cardSubtitle"));

    m_calibrationStatusBadge = new QLabel(QStringLiteral("Not Loaded"), frame);
    m_calibrationStatusBadge->setProperty("role", QStringLiteral("badge"));

    calibrationHeaderRow->addWidget(calibrationLabel);
    calibrationHeaderRow->addStretch();
    calibrationHeaderRow->addWidget(m_calibrationStatusBadge);

    calibrationColumn->addLayout(calibrationHeaderRow);

    m_calibrationList = new QListWidget(frame);
    m_calibrationList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_calibrationList->setUniformItemSizes(true);
    connect(m_calibrationList, &QListWidget::itemActivated, this, &MainWindow::onCalibrationActivated);
    calibrationColumn->addWidget(m_calibrationList, 1);

    selectionRow->addLayout(deviceColumn, 1);
    selectionRow->addLayout(calibrationColumn, 1);

    layout->addLayout(selectionRow, 1);

    return frame;
}

QWidget *MainWindow::createTestControlPanel()
{
    auto *frame = new QFrame(this);
    frame->setProperty("panel", true);
    frame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

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
    container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
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
    panel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
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
    panel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
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
    frame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

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
    badge->setProperty("state", QStringLiteral("inactive"));

    headerRow->addWidget(title);
    headerRow->addWidget(subtitle);
    headerRow->addStretch();
    headerRow->addWidget(badge);

    layout->addLayout(headerRow);

    auto *chartArea = new QFrame(frame);
    chartArea->setProperty("role", QStringLiteral("chartArea"));
    chartArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto *chartLayout = new QVBoxLayout(chartArea);
    chartLayout->setContentsMargins(0, 0, 0, 0);
    chartLayout->setSpacing(0);

    auto *chart = new QChart;
    chart->setBackgroundRoundness(0.0);
    chart->setBackgroundVisible(false);
    chart->legend()->hide();

    auto *axisFrequency = new QValueAxis(chart);
    axisFrequency->setTitleText(QStringLiteral("Frequency (GHz)"));
    axisFrequency->setLabelFormat(QStringLiteral("%.2f"));
    axisFrequency->setRange(0.0, 1.0);
    chart->addAxis(axisFrequency, Qt::AlignBottom);

    auto *axisMagnitude = new QValueAxis(chart);
    axisMagnitude->setTitleText(QStringLiteral("Magnitude (dB)"));
    axisMagnitude->setLabelFormat(QStringLiteral("%.1f"));
    axisMagnitude->setRange(-100.0, 10.0);
    chart->addAxis(axisMagnitude, Qt::AlignLeft);

    auto *axisPhase = new QValueAxis(chart);
    axisPhase->setTitleText(QStringLiteral("Phase (deg)"));
    axisPhase->setLabelFormat(QStringLiteral("%.0f"));
    axisPhase->setRange(-180.0, 180.0);
    chart->addAxis(axisPhase, Qt::AlignRight);

    auto *magnitudeSeries = new QLineSeries(chart);
    magnitudeSeries->setName(QStringLiteral("Magnitude (dB)"));
    QPen magnitudePen(QColor(QStringLiteral("#4540ff")));
    magnitudePen.setWidthF(2.0);
    magnitudeSeries->setPen(magnitudePen);
    chart->addSeries(magnitudeSeries);
    magnitudeSeries->attachAxis(axisFrequency);
    magnitudeSeries->attachAxis(axisMagnitude);

    auto *phaseSeries = new QLineSeries(chart);
    phaseSeries->setName(QStringLiteral("Phase (deg)"));
    QPen phasePen(QColor(QStringLiteral("#1d8a43")));
    phasePen.setWidthF(1.5);
    phasePen.setStyle(Qt::DashLine);
    phaseSeries->setPen(phasePen);
    chart->addSeries(phaseSeries);
    phaseSeries->attachAxis(axisFrequency);
    phaseSeries->attachAxis(axisPhase);

    auto *chartView = new InteractiveChartView(chart, chartArea);
    chartView->setRenderHint(QPainter::Antialiasing);
    chartView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    chartView->setMinimumHeight(320);
    chartView->setFocusPolicy(Qt::StrongFocus);
    chartLayout->addWidget(chartView);

    layout->addWidget(chartArea, 1);

    auto *legendRow = new QHBoxLayout;
    legendRow->setSpacing(12);

    auto *magnitudeToggle = new QCheckBox(QStringLiteral("Magnitude"), frame);
    magnitudeToggle->setChecked(true);
    magnitudeToggle->setProperty("role", QStringLiteral("cardSubtitle"));
    magnitudeToggle->setIcon(makeSeriesIcon(QColor(QStringLiteral("#4540ff")), Qt::SolidLine));
    magnitudeToggle->setIconSize(QSize(28, 12));

    auto *phaseToggle = new QCheckBox(QStringLiteral("Phase"), frame);
    phaseToggle->setChecked(true);
    phaseToggle->setProperty("role", QStringLiteral("cardSubtitle"));
    phaseToggle->setIcon(makeSeriesIcon(QColor(QStringLiteral("#1d8a43")), Qt::DashLine));
    phaseToggle->setIconSize(QSize(28, 12));

    legendRow->addWidget(magnitudeToggle);
    legendRow->addWidget(phaseToggle);
    legendRow->addStretch();

    layout->addLayout(legendRow);

    QObject::connect(magnitudeToggle, &QCheckBox::toggled, this, [magnitudeSeries, axisMagnitude](bool checked) {
        if (magnitudeSeries) {
            magnitudeSeries->setVisible(checked);
        }
        if (axisMagnitude) {
            axisMagnitude->setVisible(checked);
        }
    });

    QObject::connect(phaseToggle, &QCheckBox::toggled, this, [phaseSeries, axisPhase](bool checked) {
        if (phaseSeries) {
            phaseSeries->setVisible(checked);
        }
        if (axisPhase) {
            axisPhase->setVisible(checked);
        }
    });

    ChartComponents components;
    components.badge = badge;
    components.chart = chart;
    components.view = chartView;
    components.magnitudeSeries = magnitudeSeries;
    components.phaseSeries = phaseSeries;
    components.axisFrequency = axisFrequency;
    components.axisMagnitude = axisMagnitude;
    components.axisPhase = axisPhase;
    components.baseFrequencyMin = 0.0;
    components.baseFrequencyMax = 1.0;
    components.baseMagnitudeMin = -100.0;
    components.baseMagnitudeMax = 10.0;
    components.basePhaseMin = -180.0;
    components.basePhaseMax = 180.0;
    components.magnitudeToggle = magnitudeToggle;
    components.phaseToggle = phaseToggle;

    m_chartComponents.insert(parameterId, components);
    chartView->setResetCallback([this, parameterId]() {
        resetChartToBaseline(parameterId);
    });

    return frame;
}

void MainWindow::refreshCalibrationList()
{
    if (!m_calibrationList) {
        return;
    }

    m_calibrationList->clear();

    if (m_calibrationDirectory.empty() || !std::filesystem::exists(m_calibrationDirectory)) {
        auto *item = new QListWidgetItem(QStringLiteral("Calibration folder not found"), m_calibrationList);
        item->setFlags(Qt::NoItemFlags);
        return;
    }

    std::vector<std::filesystem::directory_entry> entries;
    for (const auto &entry : std::filesystem::directory_iterator(m_calibrationDirectory)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        const auto extension = QString::fromStdString(entry.path().extension().string()).toLower();
        if (extension != QStringLiteral(".cal")) {
            continue;
        }
        entries.push_back(entry);
    }

    std::sort(entries.begin(), entries.end(), [](const auto &lhs, const auto &rhs) {
        return lhs.path().filename().string() < rhs.path().filename().string();
    });

    const auto canonicalOr = [](const std::filesystem::path &path) {
        std::error_code ec;
        auto result = std::filesystem::weakly_canonical(path, ec);
        if (ec) {
            return path;
        }
        return result;
    };

    const auto activePath = m_activeCalibrationPath.empty()
                                ? std::filesystem::path{}
                                : canonicalOr(m_activeCalibrationPath);

    if (entries.empty()) {
        auto *item = new QListWidgetItem(QStringLiteral("No calibration files found"), m_calibrationList);
        item->setFlags(Qt::NoItemFlags);
        return;
    }

    int selectedRow = -1;
    for (std::size_t index = 0; index < entries.size(); ++index) {
        const auto &entry = entries[index];
        const auto canonicalPath = canonicalOr(entry.path());
        const QString displayName = QString::fromStdString(entry.path().filename().string());
        const QString fullPath = QString::fromStdString(canonicalPath.string());

        auto *item = new QListWidgetItem(displayName, m_calibrationList);
        item->setData(Qt::UserRole, fullPath);
        item->setToolTip(fullPath);

        if (!activePath.empty() && canonicalPath == activePath) {
            selectedRow = static_cast<int>(index);
        }
    }

    if (selectedRow >= 0) {
        m_calibrationList->setCurrentRow(selectedRow);
    }
}

std::filesystem::path MainWindow::findCalibrationDirectory() const
{
    std::vector<std::filesystem::path> candidates;

    try {
        const auto appDir = std::filesystem::path(QCoreApplication::applicationDirPath().toStdString());
        candidates.push_back(appDir / "calibration");
        candidates.push_back(appDir.parent_path() / "calibration");
    } catch (...) {
        // ignore failures retrieving application directory
    }

    candidates.push_back(std::filesystem::current_path() / "calibration");

    for (const auto &candidate : candidates) {
        if (!candidate.empty() && std::filesystem::exists(candidate) && std::filesystem::is_directory(candidate)) {
            std::error_code ec;
            const auto canonical = std::filesystem::weakly_canonical(candidate, ec);
            if (ec) {
                return candidate;
            }
            return canonical;
        }
    }

    return {};
}

void MainWindow::updateDeviceStatus(const QString &status, const QString &styleSheet)
{
    if (!m_deviceStatusBadge) {
        return;
    }

    m_deviceStatusBadge->setText(status);
    m_deviceStatusBadge->setStyleSheet(styleSheet);
}

void MainWindow::connectToDevice(const librevna::headless::DiscoveredDevice &device)
{
    ensureSweepThreadFinished();

    const QString label = QString::fromStdString(device.label);
    const QString serial = device.serial.empty()
                               ? QStringLiteral("<no-serial>")
                               : QString::fromStdString(device.serial);

    appendStatusMessage(QStringLiteral("Connecting to %1 (%2)...").arg(label, serial));

    if (m_hostCore.is_connected()) {
        m_hostCore.disconnect();
    }

    if (!m_hostCore.connect(device.serial)) {
        updateDeviceStatus(QStringLiteral("Connection Failed"),
                           QStringLiteral("background:#fdecef; color:#d12b57; padding:4px 14px; border-radius:14px; font-weight:600;"));
        const QString message = QString::fromStdString(m_hostCore.last_error_message());
        appendStatusMessage(QStringLiteral("Connection failed: %1").arg(message));
        QMessageBox::warning(this,
                             QStringLiteral("Connection failed"),
                             QStringLiteral("Unable to connect to %1.\n%2").arg(label, message));
        return;
    }

    m_connectedSerial = serial;
    updateDeviceStatus(QStringLiteral("Connected"),
                       QStringLiteral("background:#eefaf1; color:#1d8a43; padding:4px 14px; border-radius:14px; font-weight:600;"));

    appendStatusMessage(QStringLiteral("Connected to %1 (%2)").arg(label, serial));
    if (m_testHintLabel) {
        if (!m_activeCalibrationPath.empty()) {
            m_testHintLabel->setText(QStringLiteral("Device ready. Calibration loaded. You can start a sweep."));
        } else {
            m_testHintLabel->setText(QStringLiteral("Device connected. Load a calibration file to begin."));
        }
    }
}

void MainWindow::applyCalibrationFromPath(const QString &path)
{
    if (path.isEmpty()) {
        return;
    }

    std::filesystem::path calPath(path.toStdString());
    if (!std::filesystem::exists(calPath)) {
        QMessageBox::warning(this,
                             QStringLiteral("Calibration load failed"),
                             QStringLiteral("The selected calibration file does not exist:\n%1").arg(path));
        return;
    }

    {
        std::error_code ec;
        const auto canonical = std::filesystem::weakly_canonical(calPath, ec);
        if (!ec) {
            calPath = canonical;
        }
    }

    if (!m_hostCore.load_calibration(calPath)) {
        updateCalibrationStatus(QStringLiteral("Load Failed"),
                                QStringLiteral("background:#fdecef; color:#d12b57; padding:4px 14px; border-radius:14px; font-weight:600;"));
        const QString message = QString::fromStdString(m_hostCore.last_error_message());
        appendStatusMessage(QStringLiteral("Failed to load calibration: %1").arg(message));
        QMessageBox::warning(this,
                             QStringLiteral("Calibration load failed"),
                             QStringLiteral("Unable to load calibration file.\n%1").arg(message));
        return;
    }

    const auto parentDir = calPath.parent_path();
    if (!parentDir.empty() && std::filesystem::exists(parentDir) && std::filesystem::is_directory(parentDir)) {
        std::error_code ec;
        const auto canonicalParent = std::filesystem::weakly_canonical(parentDir, ec);
        m_calibrationDirectory = ec ? parentDir : canonicalParent;
    }

    m_activeCalibrationPath = calPath;
    refreshCalibrationList();
    updateCalibrationStatus(QStringLiteral("Loaded"),
                            QStringLiteral("background:#eefaf1; color:#1d8a43; padding:4px 14px; border-radius:14px; font-weight:600;"));
    appendStatusMessage(QStringLiteral("Calibration loaded: %1").arg(path));

    if (m_testHintLabel) {
        if (m_hostCore.is_connected()) {
            m_testHintLabel->setText(QStringLiteral("Calibration loaded. You can start a test when ready."));
        } else {
            m_testHintLabel->setText(QStringLiteral("Calibration loaded. Connect a device to start testing."));
        }
    }
}

void MainWindow::ensureSweepThreadFinished()
{
    if (m_sweepThread.joinable()) {
        if (m_sweepThread.get_id() == std::this_thread::get_id()) {
            m_sweepThread.detach();
        } else {
            m_sweepThread.join();
        }
    }
    m_sweepThread = std::thread();
    m_sweepInProgress = false;
}

void MainWindow::onScanDevices()
{
    if (!m_deviceList) {
        return;
    }

    if (m_scanDevicesButton) {
        m_scanDevicesButton->setEnabled(false);
    }

    appendStatusMessage(QStringLiteral("Scanning for LibreVNA devices..."));

    std::string errorMessage;
    auto devices = librevna::headless::discover_devices(errorMessage);

    if (m_scanDevicesButton) {
        m_scanDevicesButton->setEnabled(true);
    }

    if (!errorMessage.empty()) {
        updateDeviceStatus(QStringLiteral("Scan Failed"),
                           QStringLiteral("background:#fdecef; color:#d12b57; padding:4px 14px; border-radius:14px; font-weight:600;"));
        appendStatusMessage(QStringLiteral("Device scan failed: %1").arg(QString::fromStdString(errorMessage)));
        QMessageBox::warning(this,
                             QStringLiteral("Device scan failed"),
                             QStringLiteral("Unable to enumerate LibreVNA devices.\n%1").arg(QString::fromStdString(errorMessage)));
        return;
    }

    m_discoveredDevices = std::move(devices);

    QSignalBlocker blocker(m_deviceList);
    m_deviceList->clear();

    if (m_discoveredDevices.empty()) {
        auto *item = new QListWidgetItem(QStringLiteral("No devices detected"), m_deviceList);
        item->setFlags(Qt::NoItemFlags);
        updateDeviceStatus(QStringLiteral("No Devices"),
                           QStringLiteral("background:#fff4db; color:#ad7300; padding:4px 14px; border-radius:14px; font-weight:600;"));
        appendStatusMessage(QStringLiteral("No LibreVNA devices detected."));
        m_connectedSerial.clear();
        if (m_connectDeviceButton) {
            m_connectDeviceButton->setEnabled(false);
        }
        return;
    }

    int selectedRow = -1;
    for (std::size_t index = 0; index < m_discoveredDevices.size(); ++index) {
        const auto &device = m_discoveredDevices[index];
        const QString label = QString::fromStdString(device.label);
        const QString serial = device.serial.empty()
                                   ? QStringLiteral("<no-serial>")
                                   : QString::fromStdString(device.serial);

        const QString text = QStringLiteral("%1 - %2").arg(label, serial);

        auto *item = new QListWidgetItem(text, m_deviceList);
        item->setData(Qt::UserRole, static_cast<int>(index));
        item->setToolTip(QStringLiteral("VID:PID %1:%2\nSerial: %3")
                             .arg(QString::number(device.vendor_id, 16).rightJustified(4, QLatin1Char('0')).toUpper(),
                                  QString::number(device.product_id, 16).rightJustified(4, QLatin1Char('0')).toUpper(),
                                  serial));

        if (!m_connectedSerial.isEmpty() && serial.compare(m_connectedSerial, Qt::CaseInsensitive) == 0) {
            selectedRow = static_cast<int>(index);
        }
    }

    blocker.unblock();

    if (selectedRow >= 0) {
        m_deviceList->setCurrentRow(selectedRow);
        updateDeviceStatus(QStringLiteral("Connected"),
                           QStringLiteral("background:#eefaf1; color:#1d8a43; padding:4px 14px; border-radius:14px; font-weight:600;"));
    } else if (m_hostCore.is_connected()) {
        updateDeviceStatus(QStringLiteral("Connected"),
                           QStringLiteral("background:#eefaf1; color:#1d8a43; padding:4px 14px; border-radius:14px; font-weight:600;"));
    } else {
        updateDeviceStatus(QStringLiteral("Device Detected"),
                           QStringLiteral("background:#eef1ff; color:#4540ff; padding:4px 14px; border-radius:14px; font-weight:600;"));
    }

    if (!m_hostCore.is_connected() && selectedRow == -1) {
        m_deviceList->setCurrentRow(0);
    }

    if (m_connectDeviceButton) {
        m_connectDeviceButton->setEnabled(!m_discoveredDevices.empty());
    }

    appendStatusMessage(QStringLiteral("Found %1 LibreVNA device(s)")
                        .arg(static_cast<qulonglong>(m_discoveredDevices.size())));
    onDeviceSelectionChanged();
}

void MainWindow::onDeviceSelectionChanged()
{
    if (!m_deviceList || !m_connectDeviceButton) {
        return;
    }

    const bool hasSelection = !m_deviceList->selectedItems().isEmpty();
    m_connectDeviceButton->setEnabled(hasSelection);
}

void MainWindow::onDeviceActivated(QListWidgetItem *item)
{
    if (!item) {
        return;
    }
    const int index = item->data(Qt::UserRole).toInt();
    if (index < 0 || index >= static_cast<int>(m_discoveredDevices.size())) {
        return;
    }
    connectToDevice(m_discoveredDevices[static_cast<std::size_t>(index)]);
}

void MainWindow::onCalibrationActivated(QListWidgetItem *item)
{
    if (!item) {
        return;
    }
    const QString path = item->data(Qt::UserRole).toString();
    if (path.isEmpty()) {
        return;
    }
    applyCalibrationFromPath(path);
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
    QString startDir;
    if (!m_calibrationDirectory.empty()) {
        startDir = QString::fromStdString(m_calibrationDirectory.string());
    } else {
        startDir = QDir::currentPath();
    }

    const QString selected = QFileDialog::getOpenFileName(this,
                                                          QStringLiteral("Select Calibration File"),
                                                          startDir,
                                                          QStringLiteral("Calibration Files (*.cal);;All Files (*.*)"));
    if (selected.isEmpty()) {
        return;
    }

    applyCalibrationFromPath(selected);
}

void MainWindow::onSetupCalibration()
{
    appendStatusMessage(QStringLiteral("Calibration setup requested"));
    QMessageBox::information(this,
                             QStringLiteral("Calibration Setup"),
                             QStringLiteral("Calibration capture is currently available from the CLI workflow.\n"
                                            "Use the command-line tools to generate a new calibration file, then select it here."));
}

void MainWindow::onStartTest()
{
    if (m_sweepInProgress) {
        appendStatusMessage(QStringLiteral("Sweep already in progress"));
        return;
    }

    ensureSweepThreadFinished();

    if (!m_hostCore.is_connected()) {
        appendStatusMessage(QStringLiteral("Cannot start sweep: no device connected"));
        QMessageBox::warning(this,
                             QStringLiteral("No device connected"),
                             QStringLiteral("Connect to a LibreVNA device before starting a sweep."));
        if (m_testHintLabel) {
            m_testHintLabel->setText(QStringLiteral("Connect a device to start a sweep."));
        }
        return;
    }

    if (m_activeCalibrationPath.empty()) {
        appendStatusMessage(QStringLiteral("Cannot start sweep: calibration not loaded"));
        QMessageBox::warning(this,
                             QStringLiteral("Calibration required"),
                             QStringLiteral("Load a calibration file before starting a sweep."));
        if (m_testHintLabel) {
            m_testHintLabel->setText(QStringLiteral("Load a calibration file to continue."));
        }
        return;
    }

    QStringList selectedParams;
    for (auto *button : m_parameterButtons) {
        if (button->isChecked()) {
            selectedParams.append(button->text());
        }
    }
    if (selectedParams.isEmpty()) {
        if (m_testHintLabel) {
            m_testHintLabel->setText(QStringLiteral("Select at least one S parameter before starting the test."));
        }
        return;
    }

    const double startGHz = m_startFrequencySpin->value();
    const double stopGHz = m_stopFrequencySpin->value();
    if (stopGHz <= startGHz) {
        QMessageBox::warning(this,
                             QStringLiteral("Invalid sweep range"),
                             QStringLiteral("Stop frequency must be greater than start frequency."));
        return;
    }

    librevna::headless::SweepConfiguration configuration;
    configuration.start_frequency_hz = startGHz * 1e9;
    configuration.stop_frequency_hz = stopGHz * 1e9;
    configuration.points = static_cast<std::uint32_t>(m_pointsSpin->value());
    configuration.if_bandwidth_hz = 1000.0;
    configuration.power_dbm = 0.0;
    configuration.timeout_ms = 15000.0;
    configuration.excited_ports = {1, 2};

    m_activeParameters.clear();
    for (const auto &param : selectedParams) {
        m_activeParameters.insert(param);
    }

    const auto calibrationPath = m_activeCalibrationPath;
    const QString parameterSummary = selectedParams.join(QStringLiteral(", "));

    prepareChartsForSweep();

    appendStatusMessage(QStringLiteral("Starting sweep: %1 GHz -> %2 GHz (%3 points) [%4]")
                        .arg(startGHz, 0, 'f', 3)
                        .arg(stopGHz, 0, 'f', 3)
                        .arg(static_cast<qulonglong>(configuration.points))
                        .arg(parameterSummary));

    updateTestState(QStringLiteral("Running"));
    updateControlsForRunning(true);
    m_cancelRequested = false;
    m_sweepInProgress = true;

    if (m_testHintLabel) {
        m_testHintLabel->setText(QStringLiteral("Sweep in progress. This may take a moment..."));
    }

    m_sweepThread = std::thread([this, configuration, calibrationPath, parameterSummary]() {
        auto results = m_hostCore.run_sweep(configuration);
        const std::string lastError = m_hostCore.last_error_message();
        const bool cancelled = m_cancelRequested.load();
        const std::size_t resultCount = results.size();

        const auto summary = computeSweepSummary(results, kDefaultThresholdDb);

        QMetaObject::invokeMethod(
            this,
            [this,
             cancelled,
             summary,
             resultCount,
             lastError,
             startHz = configuration.start_frequency_hz,
             stopHz = configuration.stop_frequency_hz,
             parameterSummary,
             results = std::move(results)]() {
                if (m_sweepThread.joinable()) {
                    m_sweepThread.join();
                    m_sweepThread = std::thread();
                }

                m_sweepInProgress = false;
                m_cancelRequested = false;
                updateControlsForRunning(false);

                const double startGHzLocal = startHz / 1e9;
                const double stopGHzLocal = stopHz / 1e9;

                if (cancelled) {
                    updateTestState(QStringLiteral("Cancelled"));
                    appendStatusMessage(QStringLiteral("Sweep cancelled after %.3f GHz -> %.3f GHz")
                                        .arg(startGHzLocal, 0, 'f', 3)
                                        .arg(stopGHzLocal, 0, 'f', 3));
                    resetCharts();
                    setAllChartBadges(QStringLiteral("Cancelled"), QStringLiteral("inactive"));
                    if (m_testHintLabel) {
                        m_testHintLabel->setText(QStringLiteral("Sweep cancelled. Adjust settings and start again."));
                    }
                    return;
                }

                if (resultCount == 0) {
                    updateTestState(QStringLiteral("Error"));
                    const QString errorMsg = QString::fromStdString(lastError.empty() ? "No data returned" : lastError);
                    appendStatusMessage(QStringLiteral("Sweep failed: %1").arg(errorMsg));
                    QMessageBox::warning(this,
                                         QStringLiteral("Sweep failed"),
                                         QStringLiteral("Sweep did not produce any data.\n%1").arg(errorMsg));
                    resetCharts();
                    setAllChartBadges(QStringLiteral("Error"), QStringLiteral("fail"));
                    if (m_testHintLabel) {
                        m_testHintLabel->setText(QStringLiteral("Sweep failed. Check connections and try again."));
                    }
                    return;
                }

                bool pass = summary.overallPass;
                if (!m_activeParameters.isEmpty()) {
                    pass = true;
                    for (const auto &parameter : summary.parameters) {
                        const QString parameterId = QString::fromStdString(parameter.name);
                        if (!m_activeParameters.contains(parameterId)) {
                            continue;
                        }
                        if (!parameter.pass) {
                            pass = false;
                            break;
                        }
                    }
                }
                updateTestState(pass ? QStringLiteral("Pass") : QStringLiteral("Fail"));
                const QString paramText = parameterSummary.isEmpty()
                                              ? QStringLiteral("S-parameters")
                                              : parameterSummary;
                appendStatusMessage(QStringLiteral("Sweep completed: %1 (%2 points) [%3]")
                                    .arg(pass ? QStringLiteral("PASS") : QStringLiteral("FAIL"))
                                    .arg(static_cast<qulonglong>(resultCount))
                                    .arg(paramText));

                updateChartsWithResults(results);
                for (const auto &parameter : summary.parameters) {
                    const QString parameterId = QString::fromStdString(parameter.name);
                    if (!m_activeParameters.contains(parameterId)) {
                        continue;
                    }
                    const QString badgeText = parameter.pass ? QStringLiteral("PASS") : QStringLiteral("FAIL");
                    const QString badgeState = parameter.pass ? QStringLiteral("pass") : QStringLiteral("fail");
                    if (auto it = m_chartComponents.find(parameterId); it != m_chartComponents.end()) {
                        setChartBadgeState(it.value().badge, badgeText, badgeState);
                    }
                }

                if (m_testHintLabel) {
                    m_testHintLabel->setText(pass
                                                 ? QStringLiteral("Sweep completed successfully. Review the results.")
                                                 : QStringLiteral("Sweep failed thresholds. Review the results."));
                }
            },
            Qt::QueuedConnection);
    });
}

void MainWindow::onStopTest()
{
    if (!m_sweepInProgress) {
        return;
    }

    m_cancelRequested = true;
    appendStatusMessage(QStringLiteral("Sweep stop requested"));
    updateTestState(QStringLiteral("Stopping"));

    if (m_testHintLabel) {
        m_testHintLabel->setText(QStringLiteral("Attempting to stop the sweep..."));
    }
}

void MainWindow::onResetTest()
{
    if (m_sweepInProgress) {
        return;
    }

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
    resetCharts();
    m_activeParameters.clear();

    if (m_testHintLabel) {
        if (m_hostCore.is_connected() && !m_activeCalibrationPath.empty()) {
            m_testHintLabel->setText(QStringLiteral("Ready for new sweep with default parameters."));
        } else {
            m_testHintLabel->setText(QStringLiteral("Connect a device and load calibration before starting tests."));
        }
    }
}

void MainWindow::onSParameterToggled(bool /*checked*/)
{
    QStringList selected;
    for (auto *button : m_parameterButtons) {
        if (button->isChecked()) {
            selected.append(button->text());
        }
    }

    if (!m_testHintLabel) {
        return;
    }

    if (selected.isEmpty()) {
        m_testHintLabel->setText(QStringLiteral("Select at least one S parameter before starting the test."));
    } else if (m_hostCore.is_connected() && !m_activeCalibrationPath.empty()) {
        m_testHintLabel->setText(QStringLiteral("Ready to sweep %1").arg(selected.join(QStringLiteral(", "))));
    } else if (!m_hostCore.is_connected()) {
        m_testHintLabel->setText(QStringLiteral("Connect a device to sweep %1")
                                     .arg(selected.join(QStringLiteral(", "))));
    } else {
        m_testHintLabel->setText(QStringLiteral("Load calibration to sweep %1")
                                     .arg(selected.join(QStringLiteral(", "))));
    }
}

void MainWindow::prepareChartsForSweep()
{
    const double startGHz = m_startFrequencySpin ? m_startFrequencySpin->value() : 0.0;
    const double stopGHz = m_stopFrequencySpin ? m_stopFrequencySpin->value() : 0.0;
    const double lower = std::min(startGHz, stopGHz);
    const double upper = (stopGHz > startGHz) ? stopGHz : (lower + 1.0);

    for (auto it = m_chartComponents.begin(); it != m_chartComponents.end(); ++it) {
        const QString parameterId = it.key();
        const bool isActive = m_activeParameters.contains(parameterId);
        auto &components = it.value();
        if (components.magnitudeSeries) {
            components.magnitudeSeries->clear();
        }
        if (components.phaseSeries) {
            components.phaseSeries->clear();
        }
        components.baseFrequencyMin = lower;
        components.baseFrequencyMax = upper;
        components.baseMagnitudeMin = -100.0;
        components.baseMagnitudeMax = 10.0;
        components.basePhaseMin = -180.0;
        components.basePhaseMax = 180.0;
        resetChartToBaseline(parameterId);

        if (isActive) {
            setChartBadgeState(components.badge, QStringLiteral("Pending"), QStringLiteral("pending"));
        } else {
            setChartBadgeState(components.badge, QStringLiteral("Inactive"), QStringLiteral("inactive"));
        }
    }
}

void MainWindow::resetCharts()
{
    const double startGHz = m_startFrequencySpin ? m_startFrequencySpin->value() : 0.0;
    const double stopGHz = m_stopFrequencySpin ? m_stopFrequencySpin->value() : 0.0;
    const double lower = std::min(startGHz, stopGHz);
    const double upper = (stopGHz > startGHz) ? stopGHz : (lower + 1.0);

    for (auto it = m_chartComponents.begin(); it != m_chartComponents.end(); ++it) {
        const QString parameterId = it.key();
        auto &components = it.value();
        if (components.magnitudeSeries) {
            components.magnitudeSeries->clear();
        }
        if (components.phaseSeries) {
            components.phaseSeries->clear();
        }
        components.baseFrequencyMin = lower;
        components.baseFrequencyMax = upper;
        components.baseMagnitudeMin = -100.0;
        components.baseMagnitudeMax = 10.0;
        components.basePhaseMin = -180.0;
        components.basePhaseMax = 180.0;
        resetChartToBaseline(parameterId);
        setChartBadgeState(components.badge, QStringLiteral("Inactive"), QStringLiteral("inactive"));
    }
    m_latestMeasurements.clear();
}

void MainWindow::updateChartsWithResults(const std::vector<librevna::headless::VNAMeasurement> &results)
{
    if (results.empty()) {
        return;
    }

    if (m_activeParameters.isEmpty()) {
        m_latestMeasurements.clear();
        return;
    }

    m_latestMeasurements = results;

    QHash<QString, QVector<QPointF>> magnitudeData;
    QHash<QString, QVector<QPointF>> phaseData;
    magnitudeData.reserve(m_activeParameters.size());
    phaseData.reserve(m_activeParameters.size());

    struct Range {
        double min = std::numeric_limits<double>::max();
        double max = std::numeric_limits<double>::lowest();
    };

    QHash<QString, Range> magnitudeRanges;
    QHash<QString, Range> phaseRanges;

    std::vector<std::pair<QString, std::string>> trackedParameters;
    trackedParameters.reserve(m_activeParameters.size());
    for (auto it = m_chartComponents.cbegin(); it != m_chartComponents.cend(); ++it) {
        if (!m_activeParameters.contains(it.key())) {
            continue;
        }
        trackedParameters.emplace_back(it.key(), it.key().toStdString());
    }
    if (trackedParameters.empty()) {
        return;
    }

    double minFrequency = std::numeric_limits<double>::max();
    double maxFrequency = std::numeric_limits<double>::lowest();
    constexpr double kRadToDeg = 180.0 / 3.14159265358979323846;

    for (const auto &measurement : results) {
        const double frequencyGHz = measurement.frequency / 1e9;
        minFrequency = std::min(minFrequency, frequencyGHz);
        maxFrequency = std::max(maxFrequency, frequencyGHz);

        for (const auto &entry : trackedParameters) {
            const auto iter = measurement.parameters.find(entry.second);
            if (iter == measurement.parameters.end()) {
                continue;
            }

            const auto &value = iter->second;
            const double magnitude = std::abs(value);
            const double magnitudeDb = magnitude <= 0.0 ? -300.0 : 20.0 * std::log10(magnitude);
            const double phaseDeg = std::atan2(value.imag(), value.real()) * kRadToDeg;

            auto &magPoints = magnitudeData[entry.first];
            magPoints.append(QPointF(frequencyGHz, magnitudeDb));

            auto &phasePoints = phaseData[entry.first];
            phasePoints.append(QPointF(frequencyGHz, phaseDeg));

            auto &magRange = magnitudeRanges[entry.first];
            magRange.min = std::min(magRange.min, magnitudeDb);
            magRange.max = std::max(magRange.max, magnitudeDb);

            auto &phaseRange = phaseRanges[entry.first];
            phaseRange.min = std::min(phaseRange.min, phaseDeg);
            phaseRange.max = std::max(phaseRange.max, phaseDeg);
        }
    }

    double frequencyMin = minFrequency;
    double frequencyMax = maxFrequency;
    if (!std::isfinite(frequencyMin) || !std::isfinite(frequencyMax) || frequencyMin > frequencyMax) {
        frequencyMin = 0.0;
        frequencyMax = 1.0;
    }
    if (frequencyMax - frequencyMin < 1e-6) {
        const double span = std::max(0.01, std::abs(frequencyMin) * 0.05);
        frequencyMin -= span;
        frequencyMax += span;
    }

    for (auto it = m_chartComponents.begin(); it != m_chartComponents.end(); ++it) {
        const QString &parameterId = it.key();
        auto &components = it.value();
        const bool isActive = m_activeParameters.contains(parameterId);

        if (!isActive) {
            if (components.magnitudeSeries) {
                components.magnitudeSeries->clear();
            }
            if (components.phaseSeries) {
                components.phaseSeries->clear();
            }
            resetChartToBaseline(parameterId);
            setChartBadgeState(components.badge, QStringLiteral("Inactive"), QStringLiteral("inactive"));
            continue;
        }

        const auto magnitudePoints = magnitudeData.value(parameterId);
        const auto phasePoints = phaseData.value(parameterId);

        if (components.magnitudeSeries) {
            components.magnitudeSeries->replace(magnitudePoints);
        }
        if (components.phaseSeries) {
            components.phaseSeries->replace(phasePoints);
        }

        if (components.axisFrequency && minFrequency <= maxFrequency) {
            components.axisFrequency->setRange(frequencyMin, frequencyMax);
            components.baseFrequencyMin = frequencyMin;
            components.baseFrequencyMax = frequencyMax;
        }

        if (components.axisMagnitude) {
            const auto range = magnitudeRanges.value(parameterId);
            if (!magnitudePoints.isEmpty() && range.min <= range.max) {
                const double padding = 3.0;
                const double minValue = range.min - padding;
                const double maxValue = range.max + padding;
                components.axisMagnitude->setRange(minValue, maxValue);
                components.baseMagnitudeMin = minValue;
                components.baseMagnitudeMax = maxValue;
            } else {
                components.axisMagnitude->setRange(-100.0, 10.0);
                components.baseMagnitudeMin = -100.0;
                components.baseMagnitudeMax = 10.0;
            }
        }

        if (components.axisPhase) {
            const auto range = phaseRanges.value(parameterId);
            if (!phasePoints.isEmpty() && range.min <= range.max) {
                const double padding = 10.0;
                const double minValue = range.min - padding;
                const double maxValue = range.max + padding;
                components.axisPhase->setRange(minValue, maxValue);
                components.basePhaseMin = minValue;
                components.basePhaseMax = maxValue;
            } else {
                components.axisPhase->setRange(-180.0, 180.0);
                components.basePhaseMin = -180.0;
                components.basePhaseMax = 180.0;
            }
        }
    }
}

void MainWindow::setChartBadgeState(QLabel *badge, const QString &text, const QString &state)
{
    if (!badge) {
        return;
    }

    badge->setText(text);
    if (!state.isEmpty()) {
        badge->setProperty("state", state);
        if (auto *style = badge->style()) {
            style->unpolish(badge);
            style->polish(badge);
        }
    }
    badge->update();
}

void MainWindow::setAllChartBadges(const QString &text, const QString &state)
{
    const bool applyToAll = (state == QStringLiteral("inactive") && text == QStringLiteral("Inactive"));
    for (auto it = m_chartComponents.begin(); it != m_chartComponents.end(); ++it) {
        if (!applyToAll && !m_activeParameters.contains(it.key())) {
            continue;
        }
        setChartBadgeState(it.value().badge, text, state);
    }
}

void MainWindow::resetChartToBaseline(const QString &parameterId)
{
    auto it = m_chartComponents.find(parameterId);
    if (it == m_chartComponents.end()) {
        return;
    }

    auto &components = it.value();
    if (components.chart) {
        components.chart->zoomReset();
    }
    if (components.axisFrequency) {
        components.axisFrequency->setRange(components.baseFrequencyMin, components.baseFrequencyMax);
    }
    if (components.axisMagnitude) {
        components.axisMagnitude->setRange(components.baseMagnitudeMin, components.baseMagnitudeMax);
    }
    if (components.axisPhase) {
        components.axisPhase->setRange(components.basePhaseMin, components.basePhaseMax);
    }
}
