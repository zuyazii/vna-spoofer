#include "MainWindow.hpp"

#include <QAbstractItemView>
#include <QButtonGroup>
#include <QComboBox>
#include <QCheckBox>
#include <QtCharts/QAbstractAxis>
#include <QCoreApplication>
#include <QColorDialog>
#include <QGuiApplication>
#include <QScreen>
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
#include <QProcess>
#include <QFile>
#include <QEvent>
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
#include <QWindow>
#include <QScroller>
#ifdef Q_OS_WIN
#include <windows.h>
#include <shobjidl.h>
#include <objbase.h>

#ifndef __ITipInvocation_INTERFACE_DEFINED__
struct ITipInvocation : public IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE Toggle(HWND hwnd) = 0;
    virtual HRESULT STDMETHODCALLTYPE Show(HWND hwnd) = 0;
    virtual HRESULT STDMETHODCALLTYPE Hide() = 0;
};
#endif

#ifndef CLSID_UIHostNoLaunch
static const CLSID CLSID_UIHostNoLaunch = {0x4ce576fa, 0x83dc, 0x4f88, {0x95, 0x6f, 0x1f, 0x10, 0x47, 0x1f, 0xf2, 0x3c}};
#endif

#ifndef IID_ITipInvocation
static const IID IID_ITipInvocation = {0x34745DDA, 0xB3C6, 0x4F16, {0xBE, 0x2C, 0xA9, 0x35, 0x21, 0x0D, 0x58, 0x8A}};
#endif
#endif

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <complex>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <limits>
#include <map>
#include <sstream>
#include <system_error>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

namespace
{

constexpr double kDefaultThresholdDb = -10.0;
constexpr double kMinThresholdDb = -200.0;
constexpr double kMaxThresholdDb = 50.0;

const QStringList &allParameterIds()
{
    static const QStringList ids = {
        QStringLiteral("S11"),
        QStringLiteral("S12"),
        QStringLiteral("S21"),
        QStringLiteral("S22")};
    return ids;
}

const QHash<QString, QString> &parameterDescriptions()
{
    static const QHash<QString, QString> descriptions = []() {
        QHash<QString, QString> map;
        map.insert(QStringLiteral("S11"), QStringLiteral("Input Return Loss"));
        map.insert(QStringLiteral("S12"), QStringLiteral("Reverse Transmission"));
        map.insert(QStringLiteral("S21"), QStringLiteral("Forward Gain"));
        map.insert(QStringLiteral("S22"), QStringLiteral("Output Return Loss"));
        return map;
    }();
    return descriptions;
}

const QHash<QString, double> &defaultThresholds()
{
    static const QHash<QString, double> defaults = []() {
        QHash<QString, double> map;
        for (const auto &id : allParameterIds()) {
            map.insert(id, kDefaultThresholdDb);
        }
        return map;
    }();
    return defaults;
}

std::map<std::string, double> toStdThresholdMap(const QHash<QString, double> &thresholds)
{
    std::map<std::string, double> converted;
    for (auto it = thresholds.constBegin(); it != thresholds.constEnd(); ++it) {
        converted.emplace(it.key().toStdString(), it.value());
    }
    return converted;
}

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

QIcon makeColorSwatchIcon(const QColor &color)
{
    const QSize iconSize(28, 18);
    QPixmap pixmap(iconSize);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(color);
    painter.setPen(Qt::NoPen);

    const QRectF rect(3.0, 3.0, iconSize.width() - 6.0, iconSize.height() - 6.0);
    painter.drawRoundedRect(rect, 5.0, 5.0);
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
    double thresholdDb = kDefaultThresholdDb;
};

struct SweepEvaluationSummary
{
    bool overallPass = true;
    std::array<ParameterOutcome, 4> parameters{};
};

SweepEvaluationSummary computeSweepSummary(const std::vector<librevna::headless::VNAMeasurement> &measurements,
                                           const std::map<std::string, double> &thresholdDbByParameter)
{
    SweepEvaluationSummary summary{};
    summary.parameters = {{{"S11", -300.0, 0.0, true, kDefaultThresholdDb},
                           {"S12", -300.0, 0.0, true, kDefaultThresholdDb},
                           {"S21", -300.0, 0.0, true, kDefaultThresholdDb},
                           {"S22", -300.0, 0.0, true, kDefaultThresholdDb}}};

    for (const auto &measurement : measurements) {
        for (auto &parameter : summary.parameters) {
            const auto mapIt = thresholdDbByParameter.find(parameter.name);
            const double thresholdDb = (mapIt != thresholdDbByParameter.end()) ? mapIt->second : kDefaultThresholdDb;
            parameter.thresholdDb = thresholdDb;
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
    initializeTranslations();
    m_translatableItems.clear();

    registerTranslatable(QStringLiteral("S Parameter Test System"),
                         [this](const QString &text) { this->setWindowTitle(text); });
    setWindowTitle(translateText(QStringLiteral("S Parameter Test System")));

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
            border-radius: 14px;
            min-height: 60px;
            padding: 14px 28px;
            font-size: 18px;
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
            border-radius: 16px;
            padding: 10px 20px;
            min-height: 48px;
            font-size: 16px;
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
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->viewport()->setAttribute(Qt::WA_AcceptTouchEvents, true);
    QScroller::grabGesture(scrollArea->viewport(), QScroller::TouchGesture);

    auto *central = new QWidget(scrollArea);
    central->setObjectName(QStringLiteral("MainCentral"));
    central->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    auto *rootLayout = new QVBoxLayout(central);
    rootLayout->setSpacing(20);
    rootLayout->setContentsMargins(16, 16, 16, 24);

    auto *titleLabel = new QLabel(translateText(QStringLiteral("S Parameter Test System")), central);
    titleLabel->setObjectName(QStringLiteral("TitleLabel"));
    titleLabel->setAlignment(Qt::AlignHCenter);
    registerTranslatable(QStringLiteral("S Parameter Test System"), [titleLabel](const QString &text) {
        titleLabel->setText(text);
    });

    auto *subtitleLabel = new QLabel(translateText(QStringLiteral("Professional touchpad GUI for S parameter measurements and analysis")), central);
    subtitleLabel->setObjectName(QStringLiteral("SubtitleLabel"));
    subtitleLabel->setAlignment(Qt::AlignHCenter);
    registerTranslatable(QStringLiteral("Professional touchpad GUI for S parameter measurements and analysis"),
                         [subtitleLabel](const QString &text) { subtitleLabel->setText(text); });

    auto *titleContainer = new QWidget(central);
    auto *titleLayout = new QVBoxLayout(titleContainer);
    titleLayout->setContentsMargins(0, 0, 0, 0);
    titleLayout->setSpacing(4);
    titleLayout->addWidget(titleLabel, 0, Qt::AlignHCenter);
    titleLayout->addWidget(subtitleLabel, 0, Qt::AlignHCenter);

    auto *languageWidget = new QWidget(central);
    languageWidget->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    auto *languageLayout = new QVBoxLayout(languageWidget);
    languageLayout->setContentsMargins(0, 0, 0, 0);
    languageLayout->setSpacing(4);

    auto *languageLabel = new QLabel(translateText(QStringLiteral("Language")), languageWidget);
    languageLabel->setProperty("role", QStringLiteral("cardSubtitle"));
    registerTranslatable(QStringLiteral("Language"), [languageLabel](const QString &text) {
        languageLabel->setText(text);
    });
    languageLayout->addWidget(languageLabel, 0, Qt::AlignRight);

    m_languageCombo = new QComboBox(languageWidget);
    m_languageCombo->addItem(QStringLiteral("English"));
    m_languageCombo->addItem(QStringLiteral("简体中文"));
    m_languageCombo->addItem(QStringLiteral("繁體中文"));
    m_languageCombo->setCurrentIndex(indexFromLanguage(m_currentLanguage));
    m_languageCombo->setMinimumHeight(48);
    QFont comboFont = m_languageCombo->font();
    comboFont.setPointSizeF(comboFont.pointSizeF() + 1);
    m_languageCombo->setFont(comboFont);
    connect(m_languageCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onLanguageSelectionChanged);
    languageLayout->addWidget(m_languageCombo, 0, Qt::AlignRight);

    auto *headerRow = new QHBoxLayout;
    headerRow->setSpacing(12);
    headerRow->addWidget(titleContainer, 1);
    headerRow->addWidget(languageWidget, 0, Qt::AlignTop);

    rootLayout->addLayout(headerRow);

    m_calibrationPanel = createCalibrationPanel();
    m_testControlPanel = createTestControlPanel();
    m_infoRowLayout = new QBoxLayout(QBoxLayout::LeftToRight);
    m_infoRowLayout->setSpacing(16);
    m_infoRowLayout->addWidget(m_calibrationPanel, 1);
    m_infoRowLayout->addWidget(m_testControlPanel, 1);

    rootLayout->addLayout(m_infoRowLayout);

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
    appendStatusMessage(translateText(QStringLiteral("S Parameter Test System initialized")));
    m_lastThresholds = defaultThresholds();
    updateResponsiveLayout(central->width());
}

QWidget *MainWindow::createCalibrationPanel()
{
    auto *frame = new QFrame(this);
    frame->setProperty("panel", true);
    frame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    auto *layout = new QVBoxLayout(frame);
    layout->setSpacing(16);
    layout->setContentsMargins(24, 24, 24, 24);

    const int buttonHeight = 64;
    auto enhanceButton = [&](QPushButton *button) {
        if (!button) {
            return;
        }
        button->setMinimumHeight(buttonHeight);
        QFont f = button->font();
        f.setPointSizeF(f.pointSizeF() + 2);
        button->setFont(f);
    };

    auto *headerLayout = new QHBoxLayout;
    headerLayout->setSpacing(12);

    auto *title = new QLabel(translateText(QStringLiteral("Calibration & Device Control")), frame);
    title->setProperty("role", QStringLiteral("cardTitle"));
    registerTranslatable(QStringLiteral("Calibration & Device Control"), [title](const QString &text) {
        title->setText(text);
    });

    m_deviceStatusBadge = new QLabel(translateText(QStringLiteral("No Device")), frame);
    m_deviceStatusBadge->setProperty("role", QStringLiteral("badge"));

    headerLayout->addWidget(title);
    headerLayout->addStretch();

    layout->addLayout(headerLayout);

    auto *primaryButtonRow = new QHBoxLayout;
    primaryButtonRow->setSpacing(12);

    m_scanDevicesButton = new QPushButton(translateText(QStringLiteral("Scan Devices")), frame);
    m_scanDevicesButton->setObjectName(QStringLiteral("SecondaryButton"));
    registerTranslatable(QStringLiteral("Scan Devices"), [btn = m_scanDevicesButton](const QString &text) {
        btn->setText(text);
    });
    connect(m_scanDevicesButton, &QPushButton::clicked, this, &MainWindow::onScanDevices);
    enhanceButton(m_scanDevicesButton);

    m_loadCalibrationButton = new QPushButton(translateText(QStringLiteral("Load External")), frame);
    m_loadCalibrationButton->setObjectName(QStringLiteral("SecondaryButton"));
    registerTranslatable(QStringLiteral("Load External"), [btn = m_loadCalibrationButton](const QString &text) {
        btn->setText(text);
    });
    connect(m_loadCalibrationButton, &QPushButton::clicked, this, &MainWindow::onLoadCalibration);
    enhanceButton(m_loadCalibrationButton);

    primaryButtonRow->addWidget(m_scanDevicesButton, 1);
    primaryButtonRow->addWidget(m_loadCalibrationButton, 1);

    layout->addLayout(primaryButtonRow);

    auto *secondaryButtonRow = new QHBoxLayout;
    secondaryButtonRow->setSpacing(12);

    m_connectDeviceButton = new QPushButton(translateText(QStringLiteral("Connect")), frame);
    m_connectDeviceButton->setObjectName(QStringLiteral("ActionButton"));
    m_connectDeviceButton->setEnabled(false);
    registerTranslatable(QStringLiteral("Connect"), [btn = m_connectDeviceButton](const QString &text) {
        btn->setText(text);
    });
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
    enhanceButton(m_connectDeviceButton);

    m_setupCalibrationButton = new QPushButton(translateText(QStringLiteral("Setup New")), frame);
    m_setupCalibrationButton->setObjectName(QStringLiteral("ActionButton"));
    registerTranslatable(QStringLiteral("Setup New"), [btn = m_setupCalibrationButton](const QString &text) {
        btn->setText(text);
    });
    connect(m_setupCalibrationButton, &QPushButton::clicked, this, &MainWindow::onSetupCalibration);
    enhanceButton(m_setupCalibrationButton);

    secondaryButtonRow->addWidget(m_connectDeviceButton, 1);
    secondaryButtonRow->addWidget(m_setupCalibrationButton, 1);

    layout->addLayout(secondaryButtonRow);

    auto *selectionRow = new QHBoxLayout;
    selectionRow->setSpacing(16);

    auto *deviceColumn = new QVBoxLayout;
    deviceColumn->setSpacing(8);

    auto *deviceHeaderRow = new QHBoxLayout;
    deviceHeaderRow->setSpacing(12);

    auto *deviceListLabel = new QLabel(translateText(QStringLiteral("Detected LibreVNA Devices")), frame);
    deviceListLabel->setProperty("role", QStringLiteral("cardSubtitle"));
    registerTranslatable(QStringLiteral("Detected LibreVNA Devices"), [deviceListLabel](const QString &text) {
        deviceListLabel->setText(text);
    });

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

    auto *calibrationLabel = new QLabel(translateText(QStringLiteral("Calibration Files")), frame);
    calibrationLabel->setProperty("role", QStringLiteral("cardSubtitle"));
    registerTranslatable(QStringLiteral("Calibration Files"), [calibrationLabel](const QString &text) {
        calibrationLabel->setText(text);
    });

    m_calibrationStatusBadge = new QLabel(translateText(QStringLiteral("Not Loaded")), frame);
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

    const int buttonHeight = 64;
    const int spinHeight = 56;
    auto enhanceButton = [&](QPushButton *button) {
        if (!button) {
            return;
        }
        button->setMinimumHeight(buttonHeight);
        QFont f = button->font();
        f.setPointSizeF(f.pointSizeF() + 2);
        button->setFont(f);
    };

    auto enhanceToolButton = [&](QToolButton *button) {
        if (!button) {
            return;
        }
        button->setMinimumHeight(52);
        QFont f = button->font();
        f.setPointSizeF(f.pointSizeF() + 1);
        button->setFont(f);
    };

    auto *headerLayout = new QHBoxLayout;
    headerLayout->setSpacing(12);

    auto *title = new QLabel(translateText(QStringLiteral("S Parameter Test Control")), frame);
    title->setProperty("role", QStringLiteral("cardTitle"));
    registerTranslatable(QStringLiteral("S Parameter Test Control"), [title](const QString &text) {
        title->setText(text);
    });

    m_testStateBadge = new QLabel(translateText(QStringLiteral("Ready")), frame);
    m_testStateBadge->setProperty("role", QStringLiteral("badge"));

    headerLayout->addWidget(title);
    headerLayout->addStretch();
    headerLayout->addWidget(m_testStateBadge);

    layout->addLayout(headerLayout);

    auto *formLayout = new QGridLayout;
    formLayout->setHorizontalSpacing(16);
    formLayout->setVerticalSpacing(12);

    auto addSpinBoxRow = [&](int row, const QString &labelKey, QWidget *editor) {
        auto *label = new QLabel(translateText(labelKey), frame);
        label->setProperty("role", QStringLiteral("cardSubtitle"));
        registerTranslatable(labelKey, [label](const QString &text) {
            label->setText(text);
        });
        formLayout->addWidget(label, row, 0);
        formLayout->addWidget(editor, row, 1);
    };

    m_startFrequencySpin = new QDoubleSpinBox(frame);
    m_startFrequencySpin->setSuffix(QStringLiteral(" GHz"));
    m_startFrequencySpin->setDecimals(3);
    m_startFrequencySpin->setRange(0.001, 40.0);
    m_startFrequencySpin->setValue(1.0);
    m_startFrequencySpin->setSingleStep(0.1);
    m_startFrequencySpin->setMinimumHeight(spinHeight);
    m_startFrequencySpin->setButtonSymbols(QAbstractSpinBox::PlusMinus);
    registerTouchInputWidget(m_startFrequencySpin);

    m_stopFrequencySpin = new QDoubleSpinBox(frame);
    m_stopFrequencySpin->setSuffix(QStringLiteral(" GHz"));
    m_stopFrequencySpin->setDecimals(3);
    m_stopFrequencySpin->setRange(0.001, 40.0);
    m_stopFrequencySpin->setValue(6.0);
    m_stopFrequencySpin->setSingleStep(0.1);
    m_stopFrequencySpin->setMinimumHeight(spinHeight);
    m_stopFrequencySpin->setButtonSymbols(QAbstractSpinBox::PlusMinus);
    registerTouchInputWidget(m_stopFrequencySpin);

    m_pointsSpin = new QSpinBox(frame);
    m_pointsSpin->setRange(1, 2001);
    m_pointsSpin->setValue(201);
    m_pointsSpin->setSingleStep(10);
    m_pointsSpin->setMinimumHeight(spinHeight);
    m_pointsSpin->setButtonSymbols(QAbstractSpinBox::PlusMinus);
    registerTouchInputWidget(m_pointsSpin);

    addSpinBoxRow(0, QStringLiteral("Start Frequency"), m_startFrequencySpin);
    addSpinBoxRow(1, QStringLiteral("Stop Frequency"), m_stopFrequencySpin);
    addSpinBoxRow(2, QStringLiteral("Number of Points"), m_pointsSpin);

    formLayout->setColumnStretch(1, 1);

    layout->addLayout(formLayout);

    auto *parameterLabel = new QLabel(translateText(QStringLiteral("S Parameters to Test")), frame);
    parameterLabel->setProperty("role", QStringLiteral("cardSubtitle"));
    registerTranslatable(QStringLiteral("S Parameters to Test"), [parameterLabel](const QString &text) {
        parameterLabel->setText(text);
    });
    layout->addWidget(parameterLabel);

    auto *parameterRow = new QHBoxLayout;
    parameterRow->setSpacing(12);

    const QStringList parameterIds = allParameterIds();
    for (const auto &id : parameterIds) {
        auto *button = new QToolButton(frame);
        button->setText(id);
        button->setCheckable(true);
        button->setChecked(id == QStringLiteral("S11"));
        connect(button, &QToolButton::toggled, this, &MainWindow::onSParameterToggled);
        parameterRow->addWidget(button);
        m_parameterButtons.append(button);
        enhanceToolButton(button);
    }
    parameterRow->addStretch();

    layout->addLayout(parameterRow);

    auto *thresholdLabel = new QLabel(translateText(QStringLiteral("Thresholds (dB)")), frame);
    thresholdLabel->setProperty("role", QStringLiteral("cardSubtitle"));
    registerTranslatable(QStringLiteral("Thresholds (dB)"), [thresholdLabel](const QString &text) {
        thresholdLabel->setText(text);
    });
    layout->addWidget(thresholdLabel);

    auto *thresholdGrid = new QGridLayout;
    thresholdGrid->setHorizontalSpacing(16);
    thresholdGrid->setVerticalSpacing(8);

    int parameterIndex = 0;
    for (const auto &id : parameterIds) {
        auto *label = new QLabel(translateText(QStringLiteral("%1 Threshold")).arg(id), frame);
        label->setProperty("role", QStringLiteral("cardSubtitle"));
        registerTranslatable(QStringLiteral("%1 Threshold"), [label, id](const QString &text) {
            label->setText(text.arg(id));
        });

        auto *spin = new QDoubleSpinBox(frame);
        spin->setSuffix(QStringLiteral(" dB"));
        spin->setDecimals(1);
        spin->setRange(kMinThresholdDb, kMaxThresholdDb);
        spin->setSingleStep(0.5);
        spin->setKeyboardTracking(false);
        spin->setValue(defaultThresholds().value(id, kDefaultThresholdDb));
        spin->setMinimumHeight(spinHeight);
        spin->setButtonSymbols(QAbstractSpinBox::PlusMinus);
        registerTouchInputWidget(spin);

        m_thresholdEditors.insert(id, spin);

        const int row = parameterIndex / 2;
        const int column = (parameterIndex % 2) * 2;
        thresholdGrid->addWidget(label, row, column);
        thresholdGrid->addWidget(spin, row, column + 1);
        ++parameterIndex;
    }
    thresholdGrid->setColumnStretch(1, 1);
    thresholdGrid->setColumnStretch(3, 1);
    layout->addLayout(thresholdGrid);

    auto *actionsRow = new QHBoxLayout;
    actionsRow->setSpacing(12);

    m_startButton = new QPushButton(translateText(QStringLiteral("Start Test")), frame);
    m_startButton->setObjectName(QStringLiteral("ActionButton"));
    m_startButton->setIcon(QIcon(QStringLiteral(":/icons/start.svg")));
    m_startButton->setIconSize(QSize(20, 20));
    registerTranslatable(QStringLiteral("Start Test"), [btn = m_startButton](const QString &text) {
        btn->setText(text);
    });
    connect(m_startButton, &QPushButton::clicked, this, &MainWindow::onStartTest);
    enhanceButton(m_startButton);

    m_stopButton = new QPushButton(translateText(QStringLiteral("Stop")), frame);
    m_stopButton->setObjectName(QStringLiteral("StopButton"));
    m_stopButton->setEnabled(false);
    m_stopButton->setIcon(QIcon(QStringLiteral(":/icons/stop.svg")));
    m_stopButton->setIconSize(QSize(18, 18));
    registerTranslatable(QStringLiteral("Stop"), [btn = m_stopButton](const QString &text) {
        btn->setText(text);
    });
    connect(m_stopButton, &QPushButton::clicked, this, &MainWindow::onStopTest);
    enhanceButton(m_stopButton);

    m_resetButton = new QPushButton(translateText(QStringLiteral("Reset")), frame);
    m_resetButton->setObjectName(QStringLiteral("SecondaryButton"));
    m_resetButton->setIcon(QIcon(QStringLiteral(":/icons/reset.svg")));
    m_resetButton->setIconSize(QSize(18, 18));
    registerTranslatable(QStringLiteral("Reset"), [btn = m_resetButton](const QString &text) {
        btn->setText(text);
    });
    connect(m_resetButton, &QPushButton::clicked, this, &MainWindow::onResetTest);
    enhanceButton(m_resetButton);

    actionsRow->addWidget(m_startButton, 1);
    actionsRow->addWidget(m_stopButton, 1);
    actionsRow->addWidget(m_resetButton, 1);

    layout->addLayout(actionsRow);

    m_testHintLabel = new QLabel(QString(), frame);
    m_testHintLabel->setProperty("role", QStringLiteral("hint"));
    m_testHintLabel->setWordWrap(true);

    layout->addWidget(m_testHintLabel);
    setTestHint(QStringLiteral("Please complete calibration before starting tests"));

    updateThresholdEditorsEnabled();
    updateTestState(QStringLiteral("Ready"));

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

    m_chartsToggleButton = new QPushButton(QIcon(QStringLiteral(":/icons/charts.svg")),
                                           translateText(QStringLiteral("S Parameter Charts")),
                                           toggleFrame);
    m_chartsToggleButton->setProperty("role", QStringLiteral("segmented"));
    m_chartsToggleButton->setCheckable(true);
    m_chartsToggleButton->setChecked(true);
    m_chartsToggleButton->setIconSize(QSize(20, 20));
    m_chartsToggleButton->setMinimumHeight(48);
    m_chartsToggleButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    registerTranslatable(QStringLiteral("S Parameter Charts"), [btn = m_chartsToggleButton](const QString &text) {
        btn->setText(text);
    });

    m_statusToggleButton = new QPushButton(QIcon(QStringLiteral(":/icons/status.svg")),
                                           translateText(QStringLiteral("System Status")),
                                           toggleFrame);
    m_statusToggleButton->setProperty("role", QStringLiteral("segmented"));
    m_statusToggleButton->setCheckable(true);
    m_statusToggleButton->setIconSize(QSize(20, 20));
    m_statusToggleButton->setMinimumHeight(48);
    m_statusToggleButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    registerTranslatable(QStringLiteral("System Status"), [btn = m_statusToggleButton](const QString &text) {
        btn->setText(text);
    });

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
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(8, 16, 8, 16);
    layout->setSpacing(18);

    auto *card = createCombinedChartCard();
    layout->addWidget(card, 1);

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

    auto *title = new QLabel(translateText(QStringLiteral("System Status")), panel);
    title->setProperty("role", QStringLiteral("cardTitle"));
    registerTranslatable(QStringLiteral("System Status"), [title](const QString &text) {
        title->setText(text);
    });

    headerRow->addWidget(title);
    headerRow->addStretch();

    layout->addLayout(headerRow);

    auto *metaRow = new QHBoxLayout;
    metaRow->setSpacing(20);

    auto makeMetaChip = [&](const QIcon &icon,
                            const QString &captionKey,
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

        auto *captionLabel = new QLabel(translateText(captionKey), chip);
        captionLabel->setProperty("role", QStringLiteral("cardSubtitle"));
        registerTranslatable(captionKey, [captionLabel](const QString &text) {
            captionLabel->setText(text);
        });
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

    auto *activityLabel = new QLabel(translateText(QStringLiteral("Activity Log")), panel);
    activityLabel->setProperty("role", QStringLiteral("cardSubtitle"));
    registerTranslatable(QStringLiteral("Activity Log"), [activityLabel](const QString &text) {
        activityLabel->setText(text);
    });
    layout->addWidget(activityLabel);

    m_activityLog = new QListWidget(panel);
    layout->addWidget(m_activityLog, 1);

    return panel;
}

QWidget *MainWindow::createCombinedChartCard()
{
    m_parameterSeries.clear();

    auto *frame = new QFrame(this);
    frame->setProperty("panel", true);
    frame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    auto *layout = new QVBoxLayout(frame);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(16);

    auto *headerRow = new QHBoxLayout;
    headerRow->setSpacing(12);

    auto *title = new QLabel(translateText(QStringLiteral("S-Parameter Results")), frame);
    title->setProperty("role", QStringLiteral("cardTitle"));
    registerTranslatable(QStringLiteral("S-Parameter Results"), [title](const QString &text) {
        title->setText(text);
    });

    headerRow->addWidget(title);
    headerRow->addStretch();

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
    axisFrequency->setTitleText(translateText(QStringLiteral("Frequency (GHz)")));
    registerTranslatable(QStringLiteral("Frequency (GHz)"), [axisFrequency](const QString &text) {
        axisFrequency->setTitleText(text);
    });
    axisFrequency->setLabelFormat(QStringLiteral("%.2f"));
    axisFrequency->setRange(0.0, 1.0);
    chart->addAxis(axisFrequency, Qt::AlignBottom);

    auto *axisMagnitude = new QValueAxis(chart);
    axisMagnitude->setTitleText(translateText(QStringLiteral("Magnitude (dB)")));
    registerTranslatable(QStringLiteral("Magnitude (dB)"), [axisMagnitude](const QString &text) {
        axisMagnitude->setTitleText(text);
    });
    axisMagnitude->setLabelFormat(QStringLiteral("%.1f"));
    axisMagnitude->setRange(-100.0, 10.0);
    chart->addAxis(axisMagnitude, Qt::AlignLeft);

    auto *axisPhase = new QValueAxis(chart);
    axisPhase->setTitleText(translateText(QStringLiteral("Phase (deg)")));
    registerTranslatable(QStringLiteral("Phase (deg)"), [axisPhase](const QString &text) {
        axisPhase->setTitleText(text);
    });
    axisPhase->setLabelFormat(QStringLiteral("%.0f"));
    axisPhase->setRange(-180.0, 180.0);
    chart->addAxis(axisPhase, Qt::AlignRight);

    auto *chartView = new InteractiveChartView(chart, chartArea);
    chartView->setRenderHint(QPainter::Antialiasing);
    chartView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    chartView->setMinimumHeight(360);
    chartView->setFocusPolicy(Qt::StrongFocus);
    chartLayout->addWidget(chartView);

    layout->addWidget(chartArea, 1);

    auto *controlsFrame = new QFrame(frame);
    auto *controlsLayout = new QGridLayout(controlsFrame);
    controlsLayout->setContentsMargins(0, 0, 0, 0);
    controlsLayout->setHorizontalSpacing(18);
    controlsLayout->setVerticalSpacing(10);

    auto addHeaderLabel = [&](const QString &key, int column, Qt::Alignment alignment = Qt::AlignLeft) {
        auto *label = new QLabel(translateText(key), controlsFrame);
        label->setProperty("role", QStringLiteral("cardSubtitle"));
        registerTranslatable(key, [label](const QString &text) {
            label->setText(text);
        });
        controlsLayout->addWidget(label, 0, column, alignment);
    };

    addHeaderLabel(QStringLiteral("Parameter"), 0);
    addHeaderLabel(QStringLiteral("Magnitude"), 1);
    addHeaderLabel(QStringLiteral("Phase"), 2);
    addHeaderLabel(QStringLiteral("Status"), 3);

    struct DefaultColors
    {
        QColor magnitude;
        QColor phase;
    };

    const QHash<QString, DefaultColors> colorMap = {
        {QStringLiteral("S11"), {QColor(QStringLiteral("#4540ff")), QColor(QStringLiteral("#1d8a43"))}},
        {QStringLiteral("S12"), {QColor(QStringLiteral("#f97316")), QColor(QStringLiteral("#0ea5e9"))}},
        {QStringLiteral("S21"), {QColor(QStringLiteral("#d12b57")), QColor(QStringLiteral("#7c3aed"))}},
        {QStringLiteral("S22"), {QColor(QStringLiteral("#059669")), QColor(QStringLiteral("#db2777"))}}};

    const QStringList ids = allParameterIds();
    for (int index = 0; index < ids.size(); ++index) {
        const QString &parameterId = ids.at(index);
        const QString description = parameterDescriptions().value(parameterId, parameterId);
        const int row = index + 1;

        auto colorEntry = colorMap.value(parameterId, DefaultColors{QColor(QStringLiteral("#4540ff")), QColor(QStringLiteral("#1d8a43"))});

        auto *parameterWidget = new QWidget(controlsFrame);
        auto *parameterLayout = new QVBoxLayout(parameterWidget);
        parameterLayout->setContentsMargins(0, 0, 0, 0);
        parameterLayout->setSpacing(2);

        auto *parameterLabel = new QLabel(parameterId, parameterWidget);
        parameterLabel->setProperty("role", QStringLiteral("cardTitle"));
        parameterLayout->addWidget(parameterLabel);

        auto *parameterDescription = new QLabel(translateText(description), parameterWidget);
        parameterDescription->setProperty("role", QStringLiteral("cardSubtitle"));
        parameterDescription->setWordWrap(true);
        registerTranslatable(description, [parameterDescription](const QString &text) {
            parameterDescription->setText(text);
        });
        parameterLayout->addWidget(parameterDescription);
        controlsLayout->addWidget(parameterWidget, row, 0);

        auto buildSeriesControls = [&](const QString &toggleKey,
                                       const QColor &initialColor,
                                       Qt::PenStyle style,
                                       int column,
                                       auto seriesFactory) -> std::pair<QCheckBox *, QToolButton *> {
            auto *container = new QWidget(controlsFrame);
            auto *containerLayout = new QHBoxLayout(container);
            containerLayout->setContentsMargins(0, 0, 0, 0);
            containerLayout->setSpacing(8);

            auto *toggle = new QCheckBox(container);
            toggle->setChecked(true);
            toggle->setProperty("role", QStringLiteral("cardSubtitle"));
            toggle->setIcon(makeSeriesIcon(initialColor, style));
            toggle->setIconSize(QSize(32, 14));

            if (!toggleKey.isEmpty()) {
                toggle->setText(translateText(toggleKey));
                registerTranslatable(toggleKey, [toggle](const QString &text) {
                    toggle->setText(text);
                });
            }

            auto *colorButton = new QToolButton(container);
            colorButton->setIcon(makeColorSwatchIcon(initialColor));
            colorButton->setIconSize(QSize(28, 18));
            colorButton->setAutoRaise(true);
            colorButton->setToolTip(translateText(QStringLiteral("Select color")));
            registerTranslatable(QStringLiteral("Select color"), [colorButton](const QString &text) {
                colorButton->setToolTip(text);
            });

            containerLayout->addWidget(toggle);
            containerLayout->addWidget(colorButton);
            containerLayout->addStretch();
            controlsLayout->addWidget(container, row, column);

            seriesFactory(toggle, colorButton);
            return {toggle, colorButton};
        };

        ParameterSeriesControls parameterControls;

        auto magnitudeSeries = new QLineSeries(chart);
        magnitudeSeries->setName(QStringLiteral("%1 | Magnitude").arg(parameterId));
        auto phaseSeries = new QLineSeries(chart);
        phaseSeries->setName(QStringLiteral("%1 | Phase").arg(parameterId));

        auto thresholdSeries = new QLineSeries(chart);
        chart->addSeries(magnitudeSeries);
        chart->addSeries(phaseSeries);
        chart->addSeries(thresholdSeries);

        auto setupMagnitude = [&](QCheckBox *toggle, QToolButton *button) {
            QPen magnitudePen(colorEntry.magnitude);
            magnitudePen.setWidthF(2.0);
            magnitudeSeries->setPen(magnitudePen);
            magnitudeSeries->attachAxis(axisFrequency);
            magnitudeSeries->attachAxis(axisMagnitude);
            magnitudeSeries->setVisible(true);

            QPen thresholdPen(colorEntry.magnitude.lighter(130));
            thresholdPen.setWidthF(1.5);
            thresholdPen.setStyle(Qt::DotLine);
            thresholdSeries->setPen(thresholdPen);
            thresholdSeries->attachAxis(axisFrequency);
            thresholdSeries->attachAxis(axisMagnitude);
            thresholdSeries->setVisible(false);

            QObject::connect(toggle, &QCheckBox::toggled, this, [this, parameterId](bool checked) {
                auto it = m_parameterSeries.find(parameterId);
                if (it == m_parameterSeries.end()) {
                    return;
                }
                if (it->magnitudeSeries) {
                    it->magnitudeSeries->setVisible(checked);
                }
                if (it->thresholdSeries) {
                    const bool show = checked && it->thresholdSeries->count() > 0;
                    it->thresholdSeries->setVisible(show);
                }
                updateAxisVisibility();
            });

            QObject::connect(button, &QToolButton::clicked, this, [this, parameterId]() {
                auto it = m_parameterSeries.find(parameterId);
                if (it == m_parameterSeries.end() || !it->magnitudeSeries) {
                    return;
                }
                const QColor chosen = QColorDialog::getColor(it->magnitudeColor, this, translateText(QStringLiteral("Select magnitude color")));
                if (!chosen.isValid()) {
                    return;
                }
                it->magnitudeColor = chosen;
                QPen pen(chosen);
                pen.setWidthF(2.0);
                it->magnitudeSeries->setPen(pen);
                if (it->thresholdSeries) {
                    QPen thresholdPen(chosen.lighter(130));
                    thresholdPen.setWidthF(1.5);
                    thresholdPen.setStyle(Qt::DotLine);
                    it->thresholdSeries->setPen(thresholdPen);
                }
                if (it->magnitudeToggle) {
                    it->magnitudeToggle->setIcon(makeSeriesIcon(chosen, Qt::SolidLine));
                }
                if (it->magnitudeColorButton) {
                    it->magnitudeColorButton->setIcon(makeColorSwatchIcon(chosen));
                }
            });
        };

        auto setupPhase = [&](QCheckBox *toggle, QToolButton *button) {
            QPen phasePen(colorEntry.phase);
            phasePen.setWidthF(1.5);
            phasePen.setStyle(Qt::DashLine);
            phaseSeries->setPen(phasePen);
            phaseSeries->attachAxis(axisFrequency);
            phaseSeries->attachAxis(axisPhase);
            phaseSeries->setVisible(true);

            QObject::connect(toggle, &QCheckBox::toggled, this, [this, parameterId](bool checked) {
                auto it = m_parameterSeries.find(parameterId);
                if (it == m_parameterSeries.end()) {
                    return;
                }
                if (it->phaseSeries) {
                    it->phaseSeries->setVisible(checked);
                }
                updateAxisVisibility();
            });

            QObject::connect(button, &QToolButton::clicked, this, [this, parameterId]() {
                auto it = m_parameterSeries.find(parameterId);
                if (it == m_parameterSeries.end() || !it->phaseSeries) {
                    return;
                }
                const QColor chosen = QColorDialog::getColor(it->phaseColor, this, translateText(QStringLiteral("Select phase color")));
                if (!chosen.isValid()) {
                    return;
                }
                it->phaseColor = chosen;
                QPen pen(chosen);
                pen.setWidthF(1.5);
                pen.setStyle(Qt::DashLine);
                it->phaseSeries->setPen(pen);
                if (it->phaseToggle) {
                    it->phaseToggle->setIcon(makeSeriesIcon(chosen, Qt::DashLine));
                }
                if (it->phaseColorButton) {
                    it->phaseColorButton->setIcon(makeColorSwatchIcon(chosen));
                }
            });
        };

        buildSeriesControls(QStringLiteral("Show"), colorEntry.magnitude, Qt::SolidLine, 1, [&](QCheckBox *toggle, QToolButton *button) {
            parameterControls.magnitudeToggle = toggle;
            parameterControls.magnitudeColorButton = button;
            setupMagnitude(toggle, button);
        });

        buildSeriesControls(QStringLiteral("Show"), colorEntry.phase, Qt::DashLine, 2, [&](QCheckBox *toggle, QToolButton *button) {
            parameterControls.phaseToggle = toggle;
            parameterControls.phaseColorButton = button;
            setupPhase(toggle, button);
        });

        auto *badge = new QLabel(translateText(QStringLiteral("Inactive")), controlsFrame);
        badge->setProperty("role", QStringLiteral("cardBadge"));
        badge->setProperty("state", QStringLiteral("inactive"));
        registerTranslatable(QStringLiteral("Inactive"), [badge](const QString &text) {
            badge->setText(text);
        });
        controlsLayout->addWidget(badge, row, 3);

        parameterControls.badge = badge;
        parameterControls.magnitudeSeries = magnitudeSeries;
        parameterControls.phaseSeries = phaseSeries;
        parameterControls.thresholdSeries = thresholdSeries;
        parameterControls.magnitudeColor = colorEntry.magnitude;
        parameterControls.phaseColor = colorEntry.phase;

        m_parameterSeries.insert(parameterId, parameterControls);
    }

    layout->addWidget(controlsFrame);

    m_chartComponents.chart = chart;
    m_chartComponents.view = chartView;
    m_chartComponents.axisFrequency = axisFrequency;
    m_chartComponents.axisMagnitude = axisMagnitude;
    m_chartComponents.axisPhase = axisPhase;
    m_chartComponents.baseFrequencyMin = 0.0;
    m_chartComponents.baseFrequencyMax = 1.0;
    m_chartComponents.baseMagnitudeMin = -100.0;
    m_chartComponents.baseMagnitudeMax = 10.0;
    m_chartComponents.basePhaseMin = -180.0;
    m_chartComponents.basePhaseMax = 180.0;

    chartView->setResetCallback([this]() {
        resetChartToBaseline();
    });

    updateAxisVisibility();

    return frame;
}

void MainWindow::refreshCalibrationList()
{
    if (!m_calibrationList) {
        return;
    }

    m_calibrationList->clear();

    if (m_calibrationDirectory.empty() || !std::filesystem::exists(m_calibrationDirectory)) {
        auto *item = new QListWidgetItem(translateText(QStringLiteral("Calibration folder not found")), m_calibrationList);
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
        auto *item = new QListWidgetItem(translateText(QStringLiteral("No calibration files found")), m_calibrationList);
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
    m_currentDeviceStatusKey = status;
    m_currentDeviceStatusStyle = styleSheet;
    if (!m_deviceStatusBadge) {
        return;
    }

    m_deviceStatusBadge->setText(translateText(status));
    m_deviceStatusBadge->setStyleSheet(styleSheet);
}

void MainWindow::connectToDevice(const librevna::headless::DiscoveredDevice &device)
{
    ensureSweepThreadFinished();

    const QString label = QString::fromStdString(device.label);
    const QString serial = device.serial.empty()
                               ? QStringLiteral("<no-serial>")
                               : QString::fromStdString(device.serial);

    appendStatusMessage(translateText(QStringLiteral("Connecting to %1 (%2)...")).arg(label, serial));

    if (m_hostCore.is_connected()) {
        m_hostCore.disconnect();
    }

    if (!m_hostCore.connect(device.serial)) {
        updateDeviceStatus(QStringLiteral("Connection Failed"),
                           QStringLiteral("background:#fdecef; color:#d12b57; padding:4px 14px; border-radius:14px; font-weight:600;"));
        const QString message = QString::fromStdString(m_hostCore.last_error_message());
        appendStatusMessage(translateText(QStringLiteral("Connection failed: %1")).arg(message));
        QMessageBox::warning(this,
                             translateText(QStringLiteral("Connection Failed")),
                             translateText(QStringLiteral("Unable to connect to %1.\n%2")).arg(label, message));
        return;
    }

    m_connectedSerial = serial;
    updateDeviceStatus(QStringLiteral("Connected"),
                       QStringLiteral("background:#eefaf1; color:#1d8a43; padding:4px 14px; border-radius:14px; font-weight:600;"));

    appendStatusMessage(translateText(QStringLiteral("Connected to %1 (%2)")).arg(label, serial));
    if (!m_activeCalibrationPath.empty()) {
        setTestHint(QStringLiteral("Device ready. Calibration loaded. You can start a sweep."));
    } else {
        setTestHint(QStringLiteral("Device connected. Load a calibration file to begin."));
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
                             translateText(QStringLiteral("Calibration load failed")),
                             translateText(QStringLiteral("The selected calibration file does not exist:\n%1")).arg(path));
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
        appendStatusMessage(translateText(QStringLiteral("Failed to load calibration: %1")).arg(message));
        QMessageBox::warning(this,
                             translateText(QStringLiteral("Calibration load failed")),
                             translateText(QStringLiteral("Unable to load calibration file.\n%1")).arg(message));
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
    appendStatusMessage(translateText(QStringLiteral("Calibration loaded: %1")).arg(path));

    if (m_hostCore.is_connected()) {
        setTestHint(QStringLiteral("Calibration loaded. You can start a test when ready."));
    } else {
        setTestHint(QStringLiteral("Calibration loaded. Connect a device to start testing."));
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

    appendStatusMessage(translateText(QStringLiteral("Scanning for LibreVNA devices...")));

    std::string errorMessage;
    auto devices = librevna::headless::discover_devices(errorMessage);

    if (m_scanDevicesButton) {
        m_scanDevicesButton->setEnabled(true);
    }

    if (!errorMessage.empty()) {
        updateDeviceStatus(QStringLiteral("Scan Failed"),
                           QStringLiteral("background:#fdecef; color:#d12b57; padding:4px 14px; border-radius:14px; font-weight:600;"));
        const QString errorText = QString::fromStdString(errorMessage);
        appendStatusMessage(translateText(QStringLiteral("Device scan failed: %1")).arg(errorText));
        QMessageBox::warning(this,
                             translateText(QStringLiteral("Device scan failed")),
                             translateText(QStringLiteral("Unable to enumerate LibreVNA devices.\n%1")).arg(errorText));
        return;
    }

    m_discoveredDevices = std::move(devices);

    QSignalBlocker blocker(m_deviceList);
    m_deviceList->clear();

    if (m_discoveredDevices.empty()) {
        auto *item = new QListWidgetItem(translateText(QStringLiteral("No devices detected")), m_deviceList);
        item->setFlags(Qt::NoItemFlags);
        updateDeviceStatus(QStringLiteral("No Devices"),
                           QStringLiteral("background:#fff4db; color:#ad7300; padding:4px 14px; border-radius:14px; font-weight:600;"));
        appendStatusMessage(translateText(QStringLiteral("No LibreVNA devices detected.")));
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

        const QString text = translateText(QStringLiteral("%1 - %2")).arg(label, serial);

        auto *item = new QListWidgetItem(text, m_deviceList);
        item->setData(Qt::UserRole, static_cast<int>(index));
        item->setToolTip(translateText(QStringLiteral("VID:PID %1:%2\nSerial: %3"))
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

    appendStatusMessage(translateText(QStringLiteral("Found %1 LibreVNA device(s)"))
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

void MainWindow::onLanguageSelectionChanged(int index)
{
    setLanguage(languageFromIndex(index));
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
    m_currentCalibrationStatusKey = status;
    m_currentCalibrationStatusStyle = styleSheet;
    if (!m_calibrationStatusBadge) {
        return;
    }

    m_calibrationStatusBadge->setText(translateText(status));
    m_calibrationStatusBadge->setStyleSheet(styleSheet);
}

void MainWindow::updateTestState(const QString &state)
{
    m_currentTestStateKey = state;
    if (!m_testStateBadge) {
        return;
    }

    m_testStateBadge->setText(translateText(state));
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
    appendStatusMessage(translateText(QStringLiteral("Calibration setup requested")));
    QMessageBox::information(this,
                             translateText(QStringLiteral("Calibration Setup")),
                             translateText(QStringLiteral("Calibration capture is currently available from the CLI workflow.\n"
                                                            "Use the command-line tools to generate a new calibration file, then select it here.")));
}

void MainWindow::onStartTest()
{
    if (m_sweepInProgress) {
        appendStatusMessage(translateText(QStringLiteral("Sweep already in progress")));
        return;
    }

    ensureSweepThreadFinished();

    if (!m_hostCore.is_connected()) {
        appendStatusMessage(translateText(QStringLiteral("Cannot start sweep: no device connected")));
        QMessageBox::warning(this,
                             translateText(QStringLiteral("No device connected")),
                             translateText(QStringLiteral("Connect to a LibreVNA device before starting a sweep.")));
        setTestHint(QStringLiteral("Connect a device to start a sweep."));
        return;
    }

    if (m_activeCalibrationPath.empty()) {
        appendStatusMessage(translateText(QStringLiteral("Cannot start sweep: calibration not loaded")));
        QMessageBox::warning(this,
                             translateText(QStringLiteral("Calibration required")),
                             translateText(QStringLiteral("Load a calibration file before starting a sweep.")));
        setTestHint(QStringLiteral("Load a calibration file to continue."));
        return;
    }

    QStringList selectedParams;
    for (auto *button : m_parameterButtons) {
        if (button->isChecked()) {
            selectedParams.append(button->text());
        }
    }
    if (selectedParams.isEmpty()) {
        setTestHint(QStringLiteral("Select at least one S parameter before starting the test."));
        return;
    }

    const double startGHz = m_startFrequencySpin->value();
    const double stopGHz = m_stopFrequencySpin->value();
    if (stopGHz <= startGHz) {
        QMessageBox::warning(this,
                             translateText(QStringLiteral("Invalid sweep range")),
                             translateText(QStringLiteral("Stop frequency must be greater than start frequency.")));
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

    const auto thresholdValues = collectThresholds();
    const auto thresholdMap = toStdThresholdMap(thresholdValues);
    m_lastThresholds = thresholdValues;

    const auto calibrationPath = m_activeCalibrationPath;
    const QString parameterSummary = selectedParams.join(QStringLiteral(", "));
    const QStringList selectedParametersList = selectedParams;

    prepareChartsForSweep();

    appendStatusMessage(translateText(QStringLiteral("Starting sweep: %1 GHz -> %2 GHz (%3 points) [%4]"))
                        .arg(startGHz, 0, 'f', 3)
                        .arg(stopGHz, 0, 'f', 3)
                        .arg(static_cast<qulonglong>(configuration.points))
                        .arg(parameterSummary));

    updateTestState(QStringLiteral("Running"));
    updateControlsForRunning(true);
    m_cancelRequested = false;
    m_sweepInProgress = true;

    setTestHint(QStringLiteral("Sweep in progress. This may take a moment..."));

    m_sweepThread = std::thread([this,
                                 configuration,
                                 calibrationPath,
                                 parameterSummary,
                                 selectedParametersList,
                                 thresholdMap]() {
        auto results = m_hostCore.run_sweep(configuration);
        const std::string lastError = m_hostCore.last_error_message();
        const bool cancelled = m_cancelRequested.load();
        const std::size_t resultCount = results.size();

        const auto summary = computeSweepSummary(results, thresholdMap);

        QMetaObject::invokeMethod(
            this,
            [this,
             cancelled,
             summary,
             resultCount,
             lastError,
             startHz = configuration.start_frequency_hz,
             stopHz = configuration.stop_frequency_hz,
             points = configuration.points,
             ifbw = configuration.if_bandwidth_hz,
             power = configuration.power_dbm,
             parameterSummary,
             selectedParameters = selectedParametersList,
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
                    appendStatusMessage(translateText(QStringLiteral("Sweep cancelled after %.3f GHz -> %.3f GHz"))
                                        .arg(startGHzLocal, 0, 'f', 3)
                                        .arg(stopGHzLocal, 0, 'f', 3));
                    resetCharts();
                    setAllChartBadges(QStringLiteral("Cancelled"), QStringLiteral("inactive"));
                    setTestHint(QStringLiteral("Sweep cancelled. Adjust settings and start again."));
                    return;
                }

                if (resultCount == 0) {
                    updateTestState(QStringLiteral("Error"));
                    const QString errorMsg = lastError.empty()
                                                ? translateText(QStringLiteral("No data returned"))
                                                : QString::fromStdString(lastError);
                    appendStatusMessage(translateText(QStringLiteral("Sweep failed: %1")).arg(errorMsg));
                    QMessageBox::warning(this,
                                         translateText(QStringLiteral("Sweep failed")),
                                         translateText(QStringLiteral("Sweep did not produce any data.\n%1")).arg(errorMsg));
                    resetCharts();
                    setAllChartBadges(QStringLiteral("Error"), QStringLiteral("fail"));
                    setTestHint(QStringLiteral("Sweep failed. Check connections and try again."));
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
                                              ? translateText(QStringLiteral("S-parameters"))
                                              : parameterSummary;
                appendStatusMessage(translateText(QStringLiteral("Sweep completed: %1 (%2 points) [%3]"))
                                    .arg(pass ? translateText(QStringLiteral("PASS")) : translateText(QStringLiteral("FAIL")))
                                    .arg(static_cast<qulonglong>(resultCount))
                                    .arg(paramText));

                updateChartsWithResults(results);
                QList<ParameterExportInfo> exportSummaries;
                exportSummaries.reserve(static_cast<int>(summary.parameters.size()));
                for (const auto &parameter : summary.parameters) {
                    const QString parameterId = QString::fromStdString(parameter.name);
                    ParameterExportInfo info;
                    info.name = parameterId;
                    info.worstDb = parameter.worstDb;
                    info.failFrequencyHz = parameter.failFrequencyHz;
                    info.pass = parameter.pass;
                    info.thresholdDb = parameter.thresholdDb;
                    exportSummaries.append(info);

                    if (!m_activeParameters.contains(parameterId)) {
                        continue;
                    }
                    const QString badgeText = parameter.pass ? QStringLiteral("PASS") : QStringLiteral("FAIL");
                    const QString badgeState = parameter.pass ? QStringLiteral("pass") : QStringLiteral("fail");
                    if (auto it = m_parameterSeries.find(parameterId); it != m_parameterSeries.end()) {
                        setChartBadgeState(it.value().badge, badgeText, badgeState);
                    }
                }

                setTestHint(pass
                                ? QStringLiteral("Sweep completed successfully. Review the results.")
                                : QStringLiteral("Sweep failed thresholds. Review the results."));

                persistSweepOutputs(results,
                                    pass,
                                    startHz,
                                    stopHz,
                                    points,
                                    ifbw,
                                    power,
                                    selectedParameters,
                                    exportSummaries);
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
    appendStatusMessage(translateText(QStringLiteral("Sweep stop requested")));
    updateTestState(QStringLiteral("Stopping"));

    setTestHint(QStringLiteral("Attempting to stop the sweep..."));
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

    resetThresholdEditorsToDefault();
    updateThresholdEditorsEnabled();

    appendStatusMessage(translateText(QStringLiteral("Parameters reset to defaults")));
    updateTestState(QStringLiteral("Ready"));
    updateControlsForRunning(false);
    resetCharts();
    m_activeParameters.clear();

    if (m_hostCore.is_connected() && !m_activeCalibrationPath.empty()) {
        setTestHint(QStringLiteral("Ready for new sweep with default parameters."));
    } else {
        setTestHint(QStringLiteral("Connect a device and load calibration before starting tests."));
    }
}

void MainWindow::onSParameterToggled(bool /*checked*/)
{
    updateThresholdEditorsEnabled();

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
        setTestHint(QStringLiteral("Select at least one S parameter before starting the test."));
    } else if (m_hostCore.is_connected() && !m_activeCalibrationPath.empty()) {
        setTestHint(QStringLiteral("Ready to sweep %1"), QList<QVariant>{selected.join(QStringLiteral(", "))});
    } else if (!m_hostCore.is_connected()) {
        setTestHint(QStringLiteral("Connect a device to sweep %1"), QList<QVariant>{selected.join(QStringLiteral(", "))});
    } else {
        setTestHint(QStringLiteral("Load calibration to sweep %1"), QList<QVariant>{selected.join(QStringLiteral(", "))});
    }
}

void MainWindow::prepareChartsForSweep()
{
    const double startGHz = m_startFrequencySpin ? m_startFrequencySpin->value() : 0.0;
    const double stopGHz = m_stopFrequencySpin ? m_stopFrequencySpin->value() : 0.0;
    const double lower = std::min(startGHz, stopGHz);
    const double upper = (stopGHz > startGHz) ? stopGHz : (lower + 1.0);

    m_chartComponents.baseFrequencyMin = lower;
    m_chartComponents.baseFrequencyMax = upper;
    m_chartComponents.baseMagnitudeMin = -100.0;
    m_chartComponents.baseMagnitudeMax = 10.0;
    m_chartComponents.basePhaseMin = -180.0;
    m_chartComponents.basePhaseMax = 180.0;

    if (m_chartComponents.axisFrequency) {
        m_chartComponents.axisFrequency->setRange(lower, upper);
    }
    if (m_chartComponents.axisMagnitude) {
        m_chartComponents.axisMagnitude->setRange(-100.0, 10.0);
    }
    if (m_chartComponents.axisPhase) {
        m_chartComponents.axisPhase->setRange(-180.0, 180.0);
    }

    for (auto it = m_parameterSeries.begin(); it != m_parameterSeries.end(); ++it) {
        const QString parameterId = it.key();
        const bool isActive = m_activeParameters.contains(parameterId);
        auto &controls = it.value();
        if (controls.magnitudeSeries) {
            controls.magnitudeSeries->clear();
        }
        if (controls.phaseSeries) {
            controls.phaseSeries->clear();
        }
        if (controls.thresholdSeries) {
            controls.thresholdSeries->clear();
            controls.thresholdSeries->setVisible(false);
        }

        const QString badgeText = isActive ? QStringLiteral("Pending") : QStringLiteral("Inactive");
        const QString badgeState = isActive ? QStringLiteral("pending") : QStringLiteral("inactive");
        setChartBadgeState(controls.badge, badgeText, badgeState);
    }

    resetChartToBaseline();
}


void MainWindow::resetCharts()
{
    const double startGHz = m_startFrequencySpin ? m_startFrequencySpin->value() : 0.0;
    const double stopGHz = m_stopFrequencySpin ? m_stopFrequencySpin->value() : 0.0;
    const double lower = std::min(startGHz, stopGHz);
    const double upper = (stopGHz > startGHz) ? stopGHz : (lower + 1.0);

    m_chartComponents.baseFrequencyMin = lower;
    m_chartComponents.baseFrequencyMax = upper;
    m_chartComponents.baseMagnitudeMin = -100.0;
    m_chartComponents.baseMagnitudeMax = 10.0;
    m_chartComponents.basePhaseMin = -180.0;
    m_chartComponents.basePhaseMax = 180.0;

    if (m_chartComponents.axisFrequency) {
        m_chartComponents.axisFrequency->setRange(lower, upper);
    }
    if (m_chartComponents.axisMagnitude) {
        m_chartComponents.axisMagnitude->setRange(-100.0, 10.0);
    }
    if (m_chartComponents.axisPhase) {
        m_chartComponents.axisPhase->setRange(-180.0, 180.0);
    }

    for (auto it = m_parameterSeries.begin(); it != m_parameterSeries.end(); ++it) {
        auto &controls = it.value();
        if (controls.magnitudeSeries) {
            controls.magnitudeSeries->clear();
        }
        if (controls.phaseSeries) {
            controls.phaseSeries->clear();
        }
        if (controls.thresholdSeries) {
            controls.thresholdSeries->clear();
            controls.thresholdSeries->setVisible(false);
        }
        setChartBadgeState(controls.badge, QStringLiteral("Inactive"), QStringLiteral("inactive"));
    }
    resetChartToBaseline();
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
    for (auto it = m_parameterSeries.cbegin(); it != m_parameterSeries.cend(); ++it) {
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

    double overallMagnitudeMin = std::numeric_limits<double>::max();
    double overallMagnitudeMax = std::numeric_limits<double>::lowest();
    double overallPhaseMin = std::numeric_limits<double>::max();
    double overallPhaseMax = std::numeric_limits<double>::lowest();

    for (auto it = m_parameterSeries.begin(); it != m_parameterSeries.end(); ++it) {
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
            if (components.thresholdSeries) {
                components.thresholdSeries->clear();
                components.thresholdSeries->setVisible(false);
            }
            setChartBadgeState(components.badge, QStringLiteral("Inactive"), QStringLiteral("inactive"));
            continue;
        }

        const auto magnitudePoints = magnitudeData.value(parameterId);
        const auto phasePoints = phaseData.value(parameterId);
        const double thresholdDb = m_lastThresholds.value(parameterId, defaultThresholds().value(parameterId, kDefaultThresholdDb));

        if (components.magnitudeSeries) {
            components.magnitudeSeries->replace(magnitudePoints);
        }
        if (components.phaseSeries) {
            components.phaseSeries->replace(phasePoints);
        }

        auto magnitudeRange = magnitudeRanges.value(parameterId);
        double magnitudeMin = magnitudeRange.min;
        double magnitudeMax = magnitudeRange.max;
        if (std::isfinite(thresholdDb)) {
            magnitudeMin = std::min(magnitudeMin, thresholdDb - 5.0);
            magnitudeMax = std::max(magnitudeMax, thresholdDb + 5.0);
        }

        if (!magnitudePoints.isEmpty() && magnitudeMin <= magnitudeMax) {
            const double span = std::max(5.0, (magnitudeMax - magnitudeMin) * 0.1);
            magnitudeMin -= span;
            magnitudeMax += span;
        } else {
            magnitudeMin = -100.0;
            magnitudeMax = 10.0;
        }
        overallMagnitudeMin = std::min(overallMagnitudeMin, magnitudeMin);
        overallMagnitudeMax = std::max(overallMagnitudeMax, magnitudeMax);

        auto phaseRange = phaseRanges.value(parameterId);
        double phaseMin = phaseRange.min;
        double phaseMax = phaseRange.max;
        if (!phasePoints.isEmpty() && phaseMin <= phaseMax) {
            const double padding = 10.0;
            phaseMin -= padding;
            phaseMax += padding;
        } else {
            phaseMin = -180.0;
            phaseMax = 180.0;
        }
        overallPhaseMin = std::min(overallPhaseMin, phaseMin);
        overallPhaseMax = std::max(overallPhaseMax, phaseMax);

        if (components.thresholdSeries) {
            if (std::isfinite(thresholdDb) && (frequencyMax - frequencyMin) > std::numeric_limits<double>::epsilon()) {
                QVector<QPointF> thresholdPoints;
                thresholdPoints.append(QPointF(frequencyMin, thresholdDb));
                thresholdPoints.append(QPointF(frequencyMax, thresholdDb));
                components.thresholdSeries->replace(thresholdPoints);
                const bool show = !components.magnitudeToggle || components.magnitudeToggle->isChecked();
                components.thresholdSeries->setVisible(show);
            } else {
                components.thresholdSeries->clear();
                components.thresholdSeries->setVisible(false);
            }
        }
    }

    if (!std::isfinite(overallMagnitudeMin) || !std::isfinite(overallMagnitudeMax) || overallMagnitudeMin > overallMagnitudeMax) {
        overallMagnitudeMin = -100.0;
        overallMagnitudeMax = 10.0;
    }
    if (!std::isfinite(overallPhaseMin) || !std::isfinite(overallPhaseMax) || overallPhaseMin > overallPhaseMax) {
        overallPhaseMin = -180.0;
        overallPhaseMax = 180.0;
    }

    if (m_chartComponents.axisFrequency) {
        m_chartComponents.axisFrequency->setRange(frequencyMin, frequencyMax);
    }
    if (m_chartComponents.axisMagnitude) {
        m_chartComponents.axisMagnitude->setRange(overallMagnitudeMin, overallMagnitudeMax);
    }
    if (m_chartComponents.axisPhase) {
        m_chartComponents.axisPhase->setRange(overallPhaseMin, overallPhaseMax);
    }

    m_chartComponents.baseFrequencyMin = frequencyMin;
    m_chartComponents.baseFrequencyMax = frequencyMax;
    m_chartComponents.baseMagnitudeMin = overallMagnitudeMin;
    m_chartComponents.baseMagnitudeMax = overallMagnitudeMax;
    m_chartComponents.basePhaseMin = overallPhaseMin;
    m_chartComponents.basePhaseMax = overallPhaseMax;

    updateAxisVisibility();
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    const int contentWidth = centralWidget() ? centralWidget()->width() : event->size().width();
    updateResponsiveLayout(contentWidth);
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    if (m_initialShowHandled) {
        return;
    }
    m_initialShowHandled = true;
    adjustWindowForScreen();
}

void MainWindow::updateResponsiveLayout(int availableWidth)
{
    if (!m_infoRowLayout) {
        return;
    }

    const bool stackVertically = availableWidth < 1360;
    const auto desiredDirection = stackVertically ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight;
    if (m_infoRowLayout->direction() != desiredDirection) {
        m_infoRowLayout->setDirection(desiredDirection);
    }

    if (m_calibrationPanel) {
        m_calibrationPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    }
    if (m_testControlPanel) {
        m_testControlPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    }

    if (stackVertically) {
        m_infoRowLayout->setSpacing(12);
    } else {
        m_infoRowLayout->setSpacing(16);
    }
}

void MainWindow::adjustWindowForScreen()
{
    if (auto *screen = QGuiApplication::primaryScreen()) {
        const QRect available = screen->availableGeometry();
        setMinimumSize(QSize(800, 600));
        if (available.width() <= 2560) {
            QTimer::singleShot(0, this, [this]() {
                this->showMaximized();
            });
        }
        updateResponsiveLayout(centralWidget() ? centralWidget()->width() : available.width());
    }
}

void MainWindow::registerTouchInputWidget(QWidget *widget)
{
    if (!widget) {
        return;
    }
    widget->setProperty("touchInput", true);
    widget->installEventFilter(this);
    m_touchInputWidgets.insert(widget);
}

void MainWindow::showVirtualKeyboard()
{
#ifdef Q_OS_WIN
    auto focusedWindowHandle = []() -> HWND {
        if (auto *win = QGuiApplication::focusWindow()) {
            return reinterpret_cast<HWND>(win->winId());
        }
        return GetForegroundWindow();
    };

    auto invokeViaCOM = [focusedWindowHandle]() -> bool {
        ITipInvocation *tip = nullptr;
        const HRESULT hrInit = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        const bool needUninit = SUCCEEDED(hrInit);
        HRESULT hr = CoCreateInstance(CLSID_UIHostNoLaunch,
                                      nullptr,
                                      CLSCTX_INPROC_HANDLER | CLSCTX_LOCAL_SERVER,
                                      IID_ITipInvocation,
                                      reinterpret_cast<void **>(&tip));
        if (SUCCEEDED(hr) && tip) {
            HWND hwnd = focusedWindowHandle();
            tip->Show(hwnd);
            tip->Release();
            if (needUninit) {
                CoUninitialize();
            }
            return true;
        }
        if (needUninit) {
            CoUninitialize();
        }
        return false;
    };

    if (invokeViaCOM()) {
        return;
    }

    auto bringTabTipToFront = []() -> bool {
        if (HWND existing = FindWindow(L"IPTip_Main_Window", nullptr)) {
            ShowWindow(existing, SW_SHOWNORMAL);
            SetForegroundWindow(existing);
            return true;
        }
        return false;
    };

    if (bringTabTipToFront()) {
        return;
    }

    static const QStringList tabTipCandidates = {
        QStringLiteral("C:/Program Files/Common Files/Microsoft Shared/ink/TabTip.exe"),
        QStringLiteral("C:/Program Files (x86)/Common Files/Microsoft Shared/ink/TabTip.exe")
    };
    for (const auto &path : tabTipCandidates) {
        if (QFile::exists(path)) {
            QProcess::startDetached(path);
            break;
        }
    }

    QTimer::singleShot(700, this, [bringTabTipToFront]() { bringTabTipToFront(); });
#endif
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::FocusIn) {
        if (auto *widget = qobject_cast<QWidget *>(watched)) {
            if (widget->property("touchInput").toBool()) {
                showVirtualKeyboard();
            }
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::persistSweepOutputs(const std::vector<librevna::headless::VNAMeasurement> &results,
                                     bool overallPass,
                                     double startFrequencyHz,
                                     double stopFrequencyHz,
                                     std::uint32_t pointCount,
                                     double ifBandwidthHz,
                                     double powerDbm,
                                     const QStringList &activeParameters,
                                     const QList<ParameterExportInfo> &parameterSummaries)
{
    const QHash<QString, double> &thresholds = m_lastThresholds;
    if (results.empty() || activeParameters.isEmpty()) {
        return;
    }

    try {
        static const QString kOutputRoot = QStringLiteral("output");
        const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
        const QString prefix = QStringLiteral("gui_sweep");
        const QString folderName = QStringLiteral("%1_result_%2").arg(prefix, timestamp);

        std::filesystem::path basePath = std::filesystem::path(kOutputRoot.toStdString());
        std::filesystem::create_directories(basePath);

        const std::filesystem::path outputDir = basePath / folderName.toStdString();
        std::filesystem::create_directories(outputDir);

        const QString fileStem = QStringLiteral("%1_%2").arg(prefix, timestamp);
        const std::filesystem::path jsonPath = outputDir / (fileStem + QStringLiteral(".json")).toStdString();
        const std::filesystem::path csvPath = outputDir / (fileStem + QStringLiteral(".csv")).toStdString();

        std::vector<std::string> parameterNames;
        parameterNames.reserve(activeParameters.size());
        for (const auto &param : activeParameters) {
            parameterNames.emplace_back(param.toStdString());
        }

        std::map<std::string, ParameterExportInfo> summaryByParameter;
        for (const auto &info : parameterSummaries) {
            summaryByParameter.emplace(info.name.toStdString(), info);
        }

        nlohmann::json thresholdsSection = nlohmann::json::object();
        for (auto it = thresholds.constBegin(); it != thresholds.constEnd(); ++it) {
            thresholdsSection[it.key().toStdString()] = it.value();
        }

        nlohmann::json payload;
        payload["device"] = {
            {"serial", m_connectedSerial.isEmpty() ? "unknown" : m_connectedSerial.toStdString()},
            {"transport", "USB"}};
        payload["config"] = {
            {"f_start", startFrequencyHz},
            {"f_stop", stopFrequencyHz},
            {"points", pointCount},
            {"ifbw", ifBandwidthHz},
            {"power_dBm", powerDbm}};
        payload["overall_pass"] = overallPass;
        payload["measured_parameters"] = parameterNames;
        payload["thresholds_db"] = std::move(thresholdsSection);

        nlohmann::json resultsSection = nlohmann::json::object();
        for (const auto &name : parameterNames) {
            const auto it = summaryByParameter.find(name);
            if (it == summaryByParameter.end()) {
                continue;
            }
            nlohmann::json entry = {
                {"pass", it->second.pass},
                {"worst_db", it->second.worstDb},
                {"threshold_db", it->second.thresholdDb}};
            if (!it->second.pass) {
                entry["fail_at_hz"] = it->second.failFrequencyHz;
            }
            resultsSection[name] = std::move(entry);
        }
        payload["results"] = std::move(resultsSection);

        nlohmann::json trace = nlohmann::json::array();
        for (const auto &measurement : results) {
            nlohmann::json point;
            point["frequency"] = measurement.frequency;
            for (const auto &name : parameterNames) {
                const auto value = measurement.get(name);
                point[name] = {
                    {"real", value.real()},
                    {"imag", value.imag()}};
            }
            trace.push_back(std::move(point));
        }
        payload["trace"] = std::move(trace);

        {
            std::ofstream jsonStream(jsonPath);
            if (!jsonStream) {
                throw std::runtime_error("Unable to open JSON output path");
            }
            jsonStream << std::setw(2) << payload;
            if (!jsonStream.good()) {
                throw std::runtime_error("Failed to write JSON output");
            }
        }

        constexpr double kRadToDeg = 180.0 / 3.14159265358979323846;
        {
            std::ofstream csvStream(csvPath);
            if (!csvStream) {
                throw std::runtime_error("Unable to open CSV output path");
            }
            csvStream << "Frequency (Hz)";
            for (const auto &name : parameterNames) {
                csvStream << ',' << name << "_Real"
                          << ',' << name << "_Imag"
                          << ',' << name << "_Magnitude_dB"
                          << ',' << name << "_Phase_deg";
            }
            csvStream << '\n';
            csvStream << std::setprecision(16);

            for (const auto &measurement : results) {
                csvStream << measurement.frequency;
                for (const auto &name : parameterNames) {
                    const auto value = measurement.get(name);
                    const double real = value.real();
                    const double imag = value.imag();
                    const double magnitude = std::hypot(real, imag);
                    const double magnitudeDb = 20.0 * std::log10(std::max(magnitude, 1e-12));
                    const double phaseDeg = std::atan2(imag, real) * kRadToDeg;
                    csvStream << ',' << real
                              << ',' << imag
                              << ',' << magnitudeDb
                              << ',' << phaseDeg;
                }
                csvStream << '\n';
            }
            if (!csvStream.good()) {
                throw std::runtime_error("Failed to write CSV output");
            }
        }

        const QString exportPath =
            QDir::toNativeSeparators(QString::fromStdString(outputDir.u8string()));
        appendStatusMessage(translateText(QStringLiteral("Sweep outputs saved to %1")).arg(exportPath));
    } catch (const std::exception &ex) {
        appendStatusMessage(translateText(QStringLiteral("Failed to save sweep outputs: %1"))
                                .arg(QString::fromLocal8Bit(ex.what())));
    }
}

void MainWindow::setChartBadgeState(QLabel *badge, const QString &text, const QString &state)
{
    if (!badge) {
        return;
    }

    badge->setText(translateText(text));
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
    for (auto it = m_parameterSeries.begin(); it != m_parameterSeries.end(); ++it) {
        if (!applyToAll && !m_activeParameters.contains(it.key())) {
            continue;
        }
        setChartBadgeState(it.value().badge, text, state);
    }
}

void MainWindow::resetChartToBaseline()
{
    if (m_chartComponents.chart) {
        m_chartComponents.chart->zoomReset();
    }
    if (m_chartComponents.axisFrequency) {
        m_chartComponents.axisFrequency->setRange(m_chartComponents.baseFrequencyMin, m_chartComponents.baseFrequencyMax);
    }
    if (m_chartComponents.axisMagnitude) {
        m_chartComponents.axisMagnitude->setRange(m_chartComponents.baseMagnitudeMin, m_chartComponents.baseMagnitudeMax);
    }
    if (m_chartComponents.axisPhase) {
        m_chartComponents.axisPhase->setRange(m_chartComponents.basePhaseMin, m_chartComponents.basePhaseMax);
    }
    for (auto it = m_parameterSeries.begin(); it != m_parameterSeries.end(); ++it) {
        if (auto *threshold = it.value().thresholdSeries) {
            threshold->clear();
            threshold->setVisible(false);
        }
    }
    updateAxisVisibility();
}

void MainWindow::updateAxisVisibility()
{
    bool magnitudeVisible = false;
    bool phaseVisible = false;

    for (auto it = m_parameterSeries.constBegin(); it != m_parameterSeries.constEnd(); ++it) {
        const auto &controls = it.value();
        if (!magnitudeVisible && controls.magnitudeSeries && controls.magnitudeSeries->isVisible()) {
            magnitudeVisible = true;
        }
        if (!phaseVisible && controls.phaseSeries && controls.phaseSeries->isVisible()) {
            phaseVisible = true;
        }
        if (magnitudeVisible && phaseVisible) {
            break;
        }
    }

    if (m_chartComponents.axisMagnitude) {
        m_chartComponents.axisMagnitude->setVisible(magnitudeVisible);
    }
    if (m_chartComponents.axisPhase) {
        m_chartComponents.axisPhase->setVisible(phaseVisible);
    }
}

void MainWindow::updateThresholdEditorsEnabled()
{
    QSet<QString> active;
    for (auto *button : m_parameterButtons) {
        if (!button) {
            continue;
        }
        if (button->isChecked()) {
            active.insert(button->text());
        }
    }

    for (auto it = m_thresholdEditors.begin(); it != m_thresholdEditors.end(); ++it) {
        if (auto *editor = it.value()) {
            editor->setEnabled(active.contains(it.key()));
        }
    }
}

void MainWindow::resetThresholdEditorsToDefault()
{
    const auto defaults = defaultThresholds();
    for (auto it = m_thresholdEditors.begin(); it != m_thresholdEditors.end(); ++it) {
        if (auto *editor = it.value()) {
            editor->setValue(defaults.value(it.key(), kDefaultThresholdDb));
        }
    }
}

QHash<QString, double> MainWindow::collectThresholds() const
{
    QHash<QString, double> thresholds = defaultThresholds();
    for (auto it = m_thresholdEditors.constBegin(); it != m_thresholdEditors.constEnd(); ++it) {
        if (auto *editor = it.value()) {
            thresholds.insert(it.key(), editor->value());
        }
    }
    return thresholds;
}

void MainWindow::registerTranslatable(const QString &key, std::function<void(const QString &)> setter)
{
    if (!setter) {
        return;
    }
    setter(translateText(key));
    m_translatableItems.append(TranslatableItem{key, std::move(setter)});
}

void MainWindow::initializeTranslations()
{
    m_translationZhHans.clear();
    m_translationZhHant.clear();

    const auto add = [&](const QString &english, const QString &zhHans, const QString &zhHant) {
        m_translationZhHans.insert(english, zhHans);
        m_translationZhHant.insert(english, zhHant);
    };

    add(QStringLiteral("S Parameter Test System"), QStringLiteral("S参数测试系统"), QStringLiteral("S參數測試系統"));
    add(QStringLiteral("Professional touchpad GUI for S parameter measurements and analysis"),
        QStringLiteral("用于S参数测量与分析的专业触控界面"),
        QStringLiteral("用於S參數量測與分析的專業觸控介面"));
    add(QStringLiteral("Calibration & Device Control"), QStringLiteral("校准与设备控制"), QStringLiteral("校準與設備控制"));
    add(QStringLiteral("Detected LibreVNA Devices"), QStringLiteral("检测到的 LibreVNA 设备"), QStringLiteral("偵測到的 LibreVNA 裝置"));
    add(QStringLiteral("Calibration Files"), QStringLiteral("校准文件"), QStringLiteral("校準檔案"));
    add(QStringLiteral("Not Loaded"), QStringLiteral("未加载"), QStringLiteral("未載入"));
    add(QStringLiteral("Loaded"), QStringLiteral("已加载"), QStringLiteral("已載入"));
    add(QStringLiteral("Active"), QStringLiteral("启用"), QStringLiteral("啟用"));
    add(QStringLiteral("No calibration files found"), QStringLiteral("未找到校准文件"), QStringLiteral("找不到校準檔案"));
    add(QStringLiteral("Calibration folder not found"), QStringLiteral("未找到校准文件夹"), QStringLiteral("找不到校準資料夾"));
    add(QStringLiteral("Load External"), QStringLiteral("导入外部文件"), QStringLiteral("匯入外部檔案"));
    add(QStringLiteral("Scan Devices"), QStringLiteral("扫描设备"), QStringLiteral("掃描設備"));
    add(QStringLiteral("Connect"), QStringLiteral("连接"), QStringLiteral("連線"));
    add(QStringLiteral("Setup New"), QStringLiteral("新建校准"), QStringLiteral("新增校準"));
    add(QStringLiteral("No Device"), QStringLiteral("无设备"), QStringLiteral("無設備"));
    add(QStringLiteral("No Device Detected"), QStringLiteral("未检测到设备"), QStringLiteral("未偵測到裝置"));
    add(QStringLiteral("Device Detected"), QStringLiteral("设备已检测"), QStringLiteral("裝置已偵測"));
    add(QStringLiteral("Connected"), QStringLiteral("已连接"), QStringLiteral("已連線"));
    add(QStringLiteral("Connection Failed"), QStringLiteral("连接失败"), QStringLiteral("連線失敗"));
    add(QStringLiteral("Scan Failed"), QStringLiteral("扫描失败"), QStringLiteral("掃描失敗"));
    add(QStringLiteral("No Devices"), QStringLiteral("无设备"), QStringLiteral("無設備"));
    add(QStringLiteral("No devices detected"), QStringLiteral("未检测到任何设备"), QStringLiteral("未偵測到任何裝置"));
    add(QStringLiteral("No LibreVNA devices detected."), QStringLiteral("未检测到 LibreVNA 设备。"), QStringLiteral("未偵測到 LibreVNA 裝置。"));
    add(QStringLiteral("Found %1 LibreVNA device(s)"), QStringLiteral("发现 %1 台 LibreVNA 设备"), QStringLiteral("發現 %1 台 LibreVNA 裝置"));
    add(QStringLiteral("Connecting to %1 (%2)..."), QStringLiteral("正在连接 %1（%2）..."), QStringLiteral("正在連線 %1（%2）..."));
    add(QStringLiteral("Connected to %1 (%2)"), QStringLiteral("已连接到 %1（%2）"), QStringLiteral("已連線至 %1（%2）"));
    add(QStringLiteral("Unable to connect to %1.\n%2"), QStringLiteral("无法连接到 %1。\n%2"), QStringLiteral("無法連線至 %1。\n%2"));
    add(QStringLiteral("Device connected. Load a calibration file to begin."),
        QStringLiteral("设备已连接，请加载校准文件后开始。"),
        QStringLiteral("裝置已連線，請載入校準檔案後開始。"));
    add(QStringLiteral("Device ready. Calibration loaded. You can start a sweep."),
        QStringLiteral("设备已就绪，校准已加载，可以开始扫频。"),
        QStringLiteral("裝置已就緒，校準已載入，可以開始掃頻。"));
    add(QStringLiteral("Calibration load failed"), QStringLiteral("校准加载失败"), QStringLiteral("校準載入失敗"));
    add(QStringLiteral("Calibration loaded. Connect a device to start testing."),
        QStringLiteral("校准已加载，请连接设备后开始测试。"),
        QStringLiteral("校準已載入，請連線裝置後開始測試。"));
    add(QStringLiteral("Calibration loaded. You can start a test when ready."),
        QStringLiteral("校准已加载，准备好后即可开始测试。"),
        QStringLiteral("校準已載入，準備好後即可開始測試。"));
    add(QStringLiteral("Calibration setup requested"), QStringLiteral("已请求进行校准设置"), QStringLiteral("已請求進行校準設定"));
    add(QStringLiteral("Calibration Setup"), QStringLiteral("校准设置"), QStringLiteral("校準設定"));
    add(QStringLiteral("Calibration capture is currently available from the CLI workflow.\nUse the command-line tools to generate a new calibration file, then select it here."),
        QStringLiteral("当前仅可通过命令行流程进行校准采集。\n请使用命令行工具生成新的校准文件，然后在此处选择它。"),
        QStringLiteral("目前僅能透過命令列流程進行校準量測。\n請使用命令列工具產生新的校準檔案，然後在此選擇。"));
    add(QStringLiteral("S Parameter Test Control"), QStringLiteral("S参数测试控制"), QStringLiteral("S參數測試控制"));
    add(QStringLiteral("Ready"), QStringLiteral("就绪"), QStringLiteral("就緒"));
    add(QStringLiteral("Running"), QStringLiteral("运行中"), QStringLiteral("執行中"));
    add(QStringLiteral("Stopping"), QStringLiteral("正在停止"), QStringLiteral("正在停止"));
    add(QStringLiteral("Cancelled"), QStringLiteral("已取消"), QStringLiteral("已取消"));
    add(QStringLiteral("Error"), QStringLiteral("错误"), QStringLiteral("錯誤"));
    add(QStringLiteral("Pass"), QStringLiteral("通过"), QStringLiteral("通過"));
    add(QStringLiteral("Fail"), QStringLiteral("失败"), QStringLiteral("失敗"));
    add(QStringLiteral("PASS"), QStringLiteral("通过"), QStringLiteral("通過"));
    add(QStringLiteral("FAIL"), QStringLiteral("失败"), QStringLiteral("失敗"));
    add(QStringLiteral("Inactive"), QStringLiteral("未激活"), QStringLiteral("未啟用"));
    add(QStringLiteral("Pending"), QStringLiteral("待执行"), QStringLiteral("待執行"));
    add(QStringLiteral("Start Frequency"), QStringLiteral("起始频率"), QStringLiteral("起始頻率"));
    add(QStringLiteral("Stop Frequency"), QStringLiteral("终止频率"), QStringLiteral("終止頻率"));
    add(QStringLiteral("Number of Points"), QStringLiteral("采样点数"), QStringLiteral("取樣點數"));
    add(QStringLiteral("S Parameters to Test"), QStringLiteral("待测试的 S 参数"), QStringLiteral("待測試的 S 參數"));
    add(QStringLiteral("Thresholds (dB)"), QStringLiteral("阈值 (dB)"), QStringLiteral("閾值 (dB)"));
    add(QStringLiteral("%1 Threshold"), QStringLiteral("%1 阈值"), QStringLiteral("%1 閾值"));
    add(QStringLiteral("Start Test"), QStringLiteral("开始测试"), QStringLiteral("開始測試"));
    add(QStringLiteral("Stop"), QStringLiteral("停止"), QStringLiteral("停止"));
    add(QStringLiteral("Reset"), QStringLiteral("重置"), QStringLiteral("重設"));
    add(QStringLiteral("Please complete calibration before starting tests"),
        QStringLiteral("开始测试前请先完成校准"),
        QStringLiteral("開始測試前請先完成校準"));
    add(QStringLiteral("Select at least one S parameter before starting the test."),
        QStringLiteral("开始测试前请至少选择一个 S 参数。"),
        QStringLiteral("開始測試前請至少選擇一個 S 參數。"));
    add(QStringLiteral("S Parameter Charts"), QStringLiteral("S参数图表"), QStringLiteral("S參數圖表"));
    add(QStringLiteral("System Status"), QStringLiteral("系统状态"), QStringLiteral("系統狀態"));
    add(QStringLiteral("Last Update"), QStringLiteral("最近更新"), QStringLiteral("最近更新"));
    add(QStringLiteral("Total Messages"), QStringLiteral("消息总数"), QStringLiteral("訊息總數"));
    add(QStringLiteral("Activity Log"), QStringLiteral("活动日志"), QStringLiteral("活動記錄"));
    add(QStringLiteral("Input Return Loss"), QStringLiteral("输入回波损耗"), QStringLiteral("輸入反射損耗"));
    add(QStringLiteral("Reverse Transmission"), QStringLiteral("反向传输"), QStringLiteral("反向傳輸"));
    add(QStringLiteral("Forward Gain"), QStringLiteral("正向增益"), QStringLiteral("順向增益"));
    add(QStringLiteral("Output Return Loss"), QStringLiteral("输出回波损耗"), QStringLiteral("輸出反射損耗"));
    add(QStringLiteral("Magnitude"), QStringLiteral("幅度"), QStringLiteral("幅度"));
    add(QStringLiteral("Phase"), QStringLiteral("相位"), QStringLiteral("相位"));
    add(QStringLiteral("Magnitude (dB)"), QStringLiteral("幅度 (dB)"), QStringLiteral("幅度 (dB)"));
    add(QStringLiteral("Phase (deg)"), QStringLiteral("相位 (°)"), QStringLiteral("相位 (°)"));
    add(QStringLiteral("VID:PID %1:%2\nSerial: %3"), QStringLiteral("VID:PID %1:%2\n序列号: %3"), QStringLiteral("VID:PID %1:%2\n序號: %3"));
    add(QStringLiteral("Select Calibration File"), QStringLiteral("选择校准文件"), QStringLiteral("選擇校準檔案"));
    add(QStringLiteral("Calibration Files (*.cal);;All Files (*.*)"),
        QStringLiteral("校准文件 (*.cal);;所有文件 (*.*)"),
        QStringLiteral("校準檔案 (*.cal);;所有檔案 (*.*)"));
    add(QStringLiteral("Calibration load failed"), QStringLiteral("校准加载失败"), QStringLiteral("校準載入失敗"));
    add(QStringLiteral("Failed to load calibration: %1"), QStringLiteral("加载校准失败：%1"), QStringLiteral("載入校準失敗：%1"));
    add(QStringLiteral("Unable to load calibration file.\n%1"), QStringLiteral("无法加载校准文件。\n%1"), QStringLiteral("無法載入校準檔案。\n%1"));
    add(QStringLiteral("The selected calibration file does not exist:\n%1"), QStringLiteral("所选校准文件不存在：\n%1"), QStringLiteral("所選校準檔案不存在：\n%1"));
    add(QStringLiteral("Calibration loaded: %1"), QStringLiteral("已加载校准：%1"), QStringLiteral("已載入校準：%1"));
    add(QStringLiteral("Cannot start sweep: no device connected"), QStringLiteral("无法启动扫频：未连接设备"), QStringLiteral("無法啟動掃頻：未連線裝置"));
    add(QStringLiteral("Cannot start sweep: calibration not loaded"), QStringLiteral("无法启动扫频：未加载校准"), QStringLiteral("無法啟動掃頻：未載入校準"));
    add(QStringLiteral("No device connected"), QStringLiteral("未连接设备"), QStringLiteral("未連線裝置"));
    add(QStringLiteral("Connect a device to start a sweep."), QStringLiteral("请连接设备后再开始扫频。"), QStringLiteral("請連線裝置後再開始掃頻。"));
    add(QStringLiteral("Load a calibration file before starting a sweep."), QStringLiteral("扫频前请先加载校准文件。"), QStringLiteral("掃頻前請先載入校準檔案。"));
    add(QStringLiteral("Load a calibration file to continue."), QStringLiteral("请加载校准文件以继续。"), QStringLiteral("請載入校準檔案以繼續。"));
    add(QStringLiteral("Load calibration to sweep %1"), QStringLiteral("请加载校准以扫频 %1"), QStringLiteral("請載入校準以掃頻 %1"));
    add(QStringLiteral("Connect a device to sweep %1"), QStringLiteral("请连接设备以扫频 %1"), QStringLiteral("請連線裝置以掃頻 %1"));
    add(QStringLiteral("Ready to sweep %1"), QStringLiteral("准备扫频 %1"), QStringLiteral("準備掃頻 %1"));
    add(QStringLiteral("Connect a device and load calibration before starting tests."),
        QStringLiteral("开始测试前请连接设备并加载校准。"),
        QStringLiteral("開始測試前請連線裝置並載入校準。"));
    add(QStringLiteral("Ready for new sweep with default parameters."),
        QStringLiteral("已准备好使用默认参数进行新的扫频。"),
        QStringLiteral("已準備好使用預設參數進行新的掃頻。"));
    add(QStringLiteral("Sweep already in progress"), QStringLiteral("扫频正在进行中"), QStringLiteral("掃頻正在進行中"));
    add(QStringLiteral("Starting sweep: %1 GHz -> %2 GHz (%3 points) [%4]"),
        QStringLiteral("开始扫频：%1 GHz -> %2 GHz（%3 点）[%4]"),
        QStringLiteral("開始掃頻：%1 GHz -> %2 GHz（%3 點）[%4]"));
    add(QStringLiteral("Sweep in progress. This may take a moment..."),
        QStringLiteral("扫频进行中，这可能需要一些时间..."),
        QStringLiteral("掃頻進行中，可能需要一些時間..."));
    add(QStringLiteral("Sweep stop requested"), QStringLiteral("已请求停止扫频"), QStringLiteral("已請求停止掃頻"));
    add(QStringLiteral("Attempting to stop the sweep..."),
        QStringLiteral("正在尝试停止扫频..."),
        QStringLiteral("正在嘗試停止掃頻..."));
    add(QStringLiteral("Sweep cancelled after %.3f GHz -> %.3f GHz"),
        QStringLiteral("扫频在 %.3f GHz -> %.3f GHz 后被取消"),
        QStringLiteral("掃頻在 %.3f GHz -> %.3f GHz 後被取消"));
    add(QStringLiteral("Sweep cancelled. Adjust settings and start again."),
        QStringLiteral("扫频已取消，请调整设置后重新开始。"),
        QStringLiteral("掃頻已取消，請調整設定後重新開始。"));
    add(QStringLiteral("Sweep failed: %1"), QStringLiteral("扫频失败：%1"), QStringLiteral("掃頻失敗：%1"));
    add(QStringLiteral("Sweep failed thresholds. Review the results."),
        QStringLiteral("扫频未通过阈值，请检查结果。"),
        QStringLiteral("掃頻未通過閾值，請檢查結果。"));
    add(QStringLiteral("Sweep completed successfully. Review the results."),
        QStringLiteral("扫频完成，请查看结果。"),
        QStringLiteral("掃頻完成，請檢視結果。"));
    add(QStringLiteral("Sweep failed. Check connections and try again."),
        QStringLiteral("扫频失败，请检查连接后重试。"),
        QStringLiteral("掃頻失敗，請檢查連線後重試。"));
    add(QStringLiteral("Sweep did not produce any data.\n%1"),
        QStringLiteral("扫频未产生任何数据。\n%1"),
        QStringLiteral("掃頻未產生任何資料。\n%1"));
    add(QStringLiteral("Sweep outputs saved to %1"), QStringLiteral("扫频结果已保存到 %1"), QStringLiteral("掃頻結果已儲存到 %1"));
    add(QStringLiteral("Failed to save sweep outputs: %1"), QStringLiteral("保存扫频结果失败：%1"), QStringLiteral("儲存掃頻結果失敗：%1"));
    add(QStringLiteral("Frequency (GHz)"), QStringLiteral("频率 (GHz)"), QStringLiteral("頻率 (GHz)"));
    add(QStringLiteral("No data returned"), QStringLiteral("无数据返回"), QStringLiteral("無資料返回"));
    add(QStringLiteral("Invalid sweep range"), QStringLiteral("扫频范围无效"), QStringLiteral("掃頻範圍無效"));
    add(QStringLiteral("Stop frequency must be greater than start frequency."),
        QStringLiteral("终止频率必须大于起始频率。"),
        QStringLiteral("終止頻率必須大於起始頻率。"));
    add(QStringLiteral("S Parameter Test System initialized"), QStringLiteral("S参数测试系统已初始化"), QStringLiteral("S參數測試系統已初始化"));
    add(QStringLiteral("S-parameters"), QStringLiteral("S 参数"), QStringLiteral("S 參數"));
    add(QStringLiteral("Device scan failed"), QStringLiteral("设备扫描失败"), QStringLiteral("裝置掃描失敗"));
    add(QStringLiteral("Device scan failed: %1"), QStringLiteral("设备扫描失败：%1"), QStringLiteral("裝置掃描失敗：%1"));
    add(QStringLiteral("Scanning for LibreVNA devices..."), QStringLiteral("正在扫描 LibreVNA 设备..."), QStringLiteral("正在掃描 LibreVNA 裝置..."));
    add(QStringLiteral("%1 - %2"), QStringLiteral("%1 - %2"), QStringLiteral("%1 - %2"));
    add(QStringLiteral("Calibration required"), QStringLiteral("需要校准"), QStringLiteral("需要校準"));
    add(QStringLiteral("Cannot start sweep: calibration not loaded"), QStringLiteral("无法启动扫频：未加载校准"), QStringLiteral("無法啟動掃頻：未載入校準"));
    add(QStringLiteral("No device connected"), QStringLiteral("未连接设备"), QStringLiteral("未連線裝置"));
    add(QStringLiteral("Device connected. Load a calibration file to begin."),
        QStringLiteral("设备已连接，请加载校准文件后开始。"),
        QStringLiteral("裝置已連線，請載入校準檔案後開始。"));
    add(QStringLiteral("Calibration loaded. You can start a test when ready."),
        QStringLiteral("校准已加载，准备好后即可开始测试。"),
        QStringLiteral("校準已載入，準備好後即可開始測試。"));
    add(QStringLiteral("Calibration loaded. Connect a device to start testing."),
        QStringLiteral("校准已加载，请连接设备后开始测试。"),
        QStringLiteral("校準已載入，請連線裝置後開始測試。"));
    add(QStringLiteral("Language"), QStringLiteral("语言"), QStringLiteral("語言"));
}

void MainWindow::applyTranslations()
{
    for (const auto &item : std::as_const(m_translatableItems)) {
        if (item.setter) {
            item.setter(translateText(item.key));
        }
    }

    if (!m_currentCalibrationStatusKey.isEmpty()) {
        updateCalibrationStatus(m_currentCalibrationStatusKey, m_currentCalibrationStatusStyle);
    }
    if (!m_currentDeviceStatusKey.isEmpty()) {
        updateDeviceStatus(m_currentDeviceStatusKey, m_currentDeviceStatusStyle);
    }
    if (!m_currentTestStateKey.isEmpty()) {
        updateTestState(m_currentTestStateKey);
    }
    if (!m_testHintKey.isEmpty()) {
        setTestHint(m_testHintKey, m_testHintArgs);
    }
}

void MainWindow::setLanguage(Language language)
{
    if (m_currentLanguage == language) {
        return;
    }
    m_currentLanguage = language;
    if (m_languageCombo) {
        const int index = indexFromLanguage(language);
        if (index >= 0 && m_languageCombo->currentIndex() != index) {
            QSignalBlocker blocker(m_languageCombo);
            m_languageCombo->setCurrentIndex(index);
        }
    }
    applyTranslations();
}

QString MainWindow::translateText(const QString &text) const
{
    switch (m_currentLanguage) {
    case Language::SimplifiedChinese:
        return m_translationZhHans.value(text, text);
    case Language::TraditionalChinese:
        return m_translationZhHant.value(text, text);
    case Language::English:
    default:
        return text;
    }
}

MainWindow::Language MainWindow::languageFromIndex(int index) const
{
    switch (index) {
    case 1:
        return Language::SimplifiedChinese;
    case 2:
        return Language::TraditionalChinese;
    case 0:
    default:
        return Language::English;
    }
}

int MainWindow::indexFromLanguage(Language language) const
{
    switch (language) {
    case Language::SimplifiedChinese:
        return 1;
    case Language::TraditionalChinese:
        return 2;
    case Language::English:
    default:
        return 0;
    }
}

void MainWindow::setTestHint(const QString &key, const QList<QVariant> &args)
{
    m_testHintKey = key;
    m_testHintArgs = args;
    if (!m_testHintLabel) {
        return;
    }
    QString resolved = translateText(key);
    for (int i = 0; i < args.size(); ++i) {
        resolved = resolved.arg(args.at(i).toString());
    }
    m_testHintLabel->setText(resolved);
}
