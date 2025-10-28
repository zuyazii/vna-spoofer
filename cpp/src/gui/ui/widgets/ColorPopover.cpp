#include "ColorPopover.hpp"

#include "../theme/DesignTokens.hpp"

#include <QAbstractSpinBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QHideEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QHBoxLayout>
#include <QImage>
#include <QLinearGradient>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QScreen>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

namespace ui::widgets {

namespace detail {

constexpr double kEpsilon = 1e-6;

inline double clamp01(double value) {
    return std::clamp(value, 0.0, 1.0);
}

QColor colorFromHsl(double hue, double saturation, double lightness, double alpha) {
    QColor c;
    c.setHslF(clamp01(hue), clamp01(saturation), clamp01(lightness), clamp01(alpha));
    return c;
}

class ColorPlaneWidget : public QWidget {
    Q_OBJECT

public:
    explicit ColorPlaneWidget(QWidget* parent = nullptr)
        : QWidget(parent) {
        setMinimumSize(240, 180);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

    void setHue(double hue) {
        hue = clamp01(hue);
        if (std::abs(m_hue - hue) < kEpsilon) {
            return;
        }
        m_hue = hue;
        m_dirty = true;
        update();
    }

    void setPoint(double saturation, double lightness) {
        saturation = clamp01(saturation);
        lightness = clamp01(lightness);
        if (std::abs(m_saturation - saturation) < kEpsilon &&
            std::abs(m_lightness - lightness) < kEpsilon) {
            return;
        }
        m_saturation = saturation;
        m_lightness = lightness;
        update();
    }

    double saturation() const { return m_saturation; }
    double lightness() const { return m_lightness; }

signals:
    void picked(double saturation, double lightness);

protected:
    void paintEvent(QPaintEvent*) override {
        if (m_dirty || m_cache.size() != size() || m_cache.devicePixelRatio() != devicePixelRatioF()) {
            regenerateCache();
        }

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.drawImage(rect(), m_cache);

        painter.setPen(QPen(QColor(QString::fromUtf8(ui::theme::Tokens::StrokeSoft)), 1.0));
        painter.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 10.0, 10.0);

        const QPointF indicatorPos = pointFor(m_saturation, m_lightness);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(Qt::white, 2));
        painter.drawEllipse(indicatorPos, 8, 8);
        painter.setPen(QPen(Qt::black, 1));
        painter.drawEllipse(indicatorPos, 8, 8);
    }

    void resizeEvent(QResizeEvent* event) override {
        QWidget::resizeEvent(event);
        m_dirty = true;
    }

    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            updateFromPos(event->position());
        }
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        if (event->buttons() & Qt::LeftButton) {
            updateFromPos(event->position());
        }
    }

private:
    void regenerateCache() {
        const QSize sz = size() * devicePixelRatioF();
        if (sz.isEmpty()) {
            return;
        }
        m_cache = QImage(sz, QImage::Format_RGB32);
        m_cache.setDevicePixelRatio(devicePixelRatioF());

        const int w = std::max(1, sz.width());
        const int h = std::max(1, sz.height());

        for (int y = 0; y < h; ++y) {
            const double lightness = 1.0 - static_cast<double>(y) / (h - 1);
            QRgb* line = reinterpret_cast<QRgb*>(m_cache.scanLine(y));
            for (int x = 0; x < w; ++x) {
                const double saturation = static_cast<double>(x) / (w - 1);
                const QColor c = colorFromHsl(m_hue, saturation, lightness, 1.0);
                line[x] = c.rgb();
            }
        }
        m_dirty = false;
    }

    QPointF pointFor(double saturation, double lightness) const {
        const double x = saturation * width();
        const double y = (1.0 - lightness) * height();
        return QPointF(x, y);
    }

    void updateFromPos(const QPointF& pos) {
        const double x = clamp01(pos.x() / std::max(1.0, width() * 1.0));
        const double y = clamp01(pos.y() / std::max(1.0, height() * 1.0));
        const double saturation = x;
        const double lightness = 1.0 - y;
        setPoint(saturation, lightness);
        emit picked(saturation, lightness);
    }

    double m_hue = 0.0;
    double m_saturation = 1.0;
    double m_lightness = 0.5;
    QImage m_cache;
    bool m_dirty = true;
};

class HueSliderWidget : public QWidget {
    Q_OBJECT

public:
    explicit HueSliderWidget(QWidget* parent = nullptr)
        : QWidget(parent) {
        setFixedHeight(22);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    void setHue(double hue) {
        hue = clamp01(hue);
        if (std::abs(m_hue - hue) < kEpsilon) {
            return;
        }
        m_hue = hue;
        update();
    }

    double hue() const { return m_hue; }

signals:
    void hueChanged(double hue);

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        QLinearGradient gradient(rect().topLeft(), rect().topRight());
        gradient.setColorAt(0.0, QColor::fromHsl(0, 255, 128));
        gradient.setColorAt(1.0 / 6.0, QColor::fromHsl(43, 255, 128));
        gradient.setColorAt(2.0 / 6.0, QColor::fromHsl(85, 255, 128));
        gradient.setColorAt(3.0 / 6.0, QColor::fromHsl(128, 255, 128));
        gradient.setColorAt(4.0 / 6.0, QColor::fromHsl(170, 255, 128));
        gradient.setColorAt(5.0 / 6.0, QColor::fromHsl(213, 255, 128));
        gradient.setColorAt(1.0, QColor::fromHsl(0, 255, 128));

        QRect inner = rect().adjusted(0, 6, 0, -6);
        painter.setPen(Qt::NoPen);
        painter.setBrush(gradient);
        painter.drawRoundedRect(inner, 6, 6);

        painter.setPen(QPen(QColor(QString::fromUtf8(ui::theme::Tokens::StrokeSoft)), 1.0));
        painter.drawRoundedRect(inner, 6, 6);

        const double x = inner.left() + m_hue * inner.width();
        painter.setPen(QPen(Qt::white, 2));
        painter.drawEllipse(QPointF(x, rect().center().y()), 7, 7);
        painter.setPen(QPen(Qt::black, 1));
        painter.drawEllipse(QPointF(x, rect().center().y()), 7, 7);
    }

    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            setFromPosition(event->position());
        }
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        if (event->buttons() & Qt::LeftButton) {
            setFromPosition(event->position());
        }
    }

private:
    void setFromPosition(const QPointF& pos) {
        const QRect inner = rect().adjusted(0, 6, 0, -6);
        if (inner.width() <= 0) {
            return;
        }
        const double hue = clamp01((pos.x() - inner.left()) / inner.width());
        m_hue = hue;
        emit hueChanged(m_hue);
        update();
    }

    double m_hue = 0.0;
};

class AlphaSliderWidget : public QWidget {
    Q_OBJECT

public:
    explicit AlphaSliderWidget(QWidget* parent = nullptr)
        : QWidget(parent) {
        setFixedHeight(22);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    void setBaseColor(const QColor& color) {
        QColor opaque = color;
        opaque.setAlphaF(1.0);
        if (m_baseColor == opaque) {
            return;
        }
        m_baseColor = opaque;
        update();
    }

    void setAlpha(double alpha) {
        alpha = clamp01(alpha);
        if (std::abs(m_alpha - alpha) < kEpsilon) {
            return;
        }
        m_alpha = alpha;
        update();
    }

signals:
    void alphaChanged(double alpha);

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        QRect inner = rect().adjusted(0, 6, 0, -6);
        drawCheckerboard(painter, inner);

        QLinearGradient gradient(inner.topLeft(), inner.topRight());
        QColor transparent = m_baseColor;
        transparent.setAlpha(0);
        gradient.setColorAt(0.0, transparent);
        gradient.setColorAt(1.0, m_baseColor);
        painter.setBrush(gradient);
        painter.setPen(Qt::NoPen);
        painter.drawRoundedRect(inner, 6, 6);

        painter.setPen(QPen(QColor(QString::fromUtf8(ui::theme::Tokens::StrokeSoft)), 1.0));
        painter.drawRoundedRect(inner, 6, 6);

        const double x = inner.left() + m_alpha * inner.width();
        painter.setPen(QPen(Qt::white, 2));
        painter.drawEllipse(QPointF(x, rect().center().y()), 7, 7);
        painter.setPen(QPen(Qt::black, 1));
        painter.drawEllipse(QPointF(x, rect().center().y()), 7, 7);
    }

    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            setFromPosition(event->position());
        }
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        if (event->buttons() & Qt::LeftButton) {
            setFromPosition(event->position());
        }
    }

private:
    void setFromPosition(const QPointF& pos) {
        const QRect inner = rect().adjusted(0, 6, 0, -6);
        if (inner.width() <= 0) {
            return;
        }
        const double alpha = clamp01((pos.x() - inner.left()) / inner.width());
        m_alpha = alpha;
        emit alphaChanged(m_alpha);
        update();
    }

    void drawCheckerboard(QPainter& painter, const QRect& rect) {
        const int square = 8;
        QColor light(255, 255, 255);
        QColor dark(220, 220, 220);
        for (int y = rect.top(); y < rect.bottom(); y += square) {
            for (int x = rect.left(); x < rect.right(); x += square) {
                painter.fillRect(QRect(x, y, square, square), ((x / square + y / square) % 2) ? light : dark);
            }
        }
    }

    QColor m_baseColor = Qt::white;
    double m_alpha = 1.0;
};

} // namespace detail

using detail::AlphaSliderWidget;
using detail::ColorPlaneWidget;
using detail::HueSliderWidget;
using detail::clamp01;
using detail::colorFromHsl;

ColorPopover::ColorPopover(QWidget* parent)
    : QWidget(parent)
    , m_color(Qt::white) {
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_StyledBackground, true);
    setFocusPolicy(Qt::StrongFocus);
    setProperty("type", "card");
    buildUi();
    syncEditors();
}

void ColorPopover::openFor(QWidget* anchor, const QColor& initial) {
    if (!anchor) {
        return;
    }
    QColor fallback(QString::fromUtf8(theme::Tokens::S11));
    m_color = initial.isValid() ? initial : fallback;
    syncEditors();
    updatePreview();

    adjustSize();
    QRect geometry = frameGeometry();
    geometry.moveTopLeft(anchor->mapToGlobal(anchor->rect().bottomRight()) + QPoint(8, 8));

    if (const QScreen* screen = anchor->screen()) {
        const QRect available = screen->availableGeometry();
        if (geometry.right() > available.right()) {
            geometry.moveRight(available.right());
        }
        if (geometry.bottom() > available.bottom()) {
            geometry.moveBottom(available.bottom());
        }
        if (geometry.left() < available.left()) {
            geometry.moveLeft(available.left());
        }
        if (geometry.top() < available.top()) {
            geometry.moveTop(available.top());
        }
    }

    setGeometry(geometry);

    show();
    raise();
    activateWindow();
    setFocus(Qt::ActiveWindowFocusReason);
}

QColor ColorPopover::color() const {
    return m_color;
}

void ColorPopover::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        close();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void ColorPopover::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    emit closed();
}

void ColorPopover::buildUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(16);

    m_preview = new QWidget(this);
    m_preview->setFixedSize(64, 64);
    m_preview->setProperty("type", "card");
    m_preview->setAutoFillBackground(true);

    auto* previewRow = new QHBoxLayout;
    previewRow->setSpacing(12);
    previewRow->addWidget(m_preview, 0, Qt::AlignLeft | Qt::AlignTop);
    layout->addLayout(previewRow);

    m_colorField = new ColorPlaneWidget(this);
    layout->addWidget(m_colorField, 1);

    m_hueSlider = new HueSliderWidget(this);
    layout->addWidget(m_hueSlider);

    m_alphaSlider = new AlphaSliderWidget(this);
    layout->addWidget(m_alphaSlider);

    auto* hexRow = new QHBoxLayout;
    hexRow->setSpacing(8);
    auto* hexLabel = new QLabel(tr("HEX"), this);
    hexLabel->setProperty("role", "subtitle");
    hexRow->addWidget(hexLabel);
    m_hexEdit = new QLineEdit(this);
    m_hexEdit->setPlaceholderText(QStringLiteral("#000000"));
    m_hexEdit->setMaxLength(9);
    hexRow->addWidget(m_hexEdit, 1);
    layout->addLayout(hexRow);

    auto* bottomRow = new QHBoxLayout;
    bottomRow->setSpacing(8);
    m_dropperButton = new QPushButton(tr("Pick"), this);
    m_dropperButton->setFixedSize(40, 40);
    m_dropperButton->setProperty("variant", "ghost");
    bottomRow->addWidget(m_dropperButton);

    m_modeCombo = new QComboBox(this);
    m_modeCombo->addItem(tr("HSL"));
    m_modeCombo->setFixedHeight(40);
    bottomRow->addWidget(m_modeCombo);

    auto addSpin = [this, bottomRow](const QString& title, double min, double max, double step,
                                     const QString& suffix, QDoubleSpinBox** out) {
        auto* container = new QVBoxLayout;
        container->setSpacing(4);
        auto* label = new QLabel(title, this);
        label->setProperty("role", "subtitle");
        container->addWidget(label, 0, Qt::AlignLeft);
        auto* spin = new QDoubleSpinBox(this);
        spin->setRange(min, max);
        spin->setSingleStep(step);
        spin->setDecimals(step < 1.0 ? 1 : 0);
        spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
        spin->setAlignment(Qt::AlignCenter);
        spin->setSuffix(suffix);
        spin->setFixedHeight(36);
        container->addWidget(spin);
        bottomRow->addLayout(container, 1);
        if (out) {
            *out = spin;
        }
    };

    addSpin(tr("H"), 0.0, 360.0, 1.0, QStringLiteral("\u00B0"), &m_hSpin);
    addSpin(tr("S"), 0.0, 100.0, 1.0, QStringLiteral("%"), &m_sSpin);
    addSpin(tr("L"), 0.0, 100.0, 1.0, QStringLiteral("%"), &m_lSpin);
    addSpin(tr("A"), 0.0, 100.0, 1.0, QStringLiteral("%"), &m_alphaSpin);

    layout->addLayout(bottomRow);

    connect(m_hexEdit, &QLineEdit::editingFinished, this, &ColorPopover::updateFromHex);

    connect(m_hSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            &ColorPopover::updateFromHsl);
    connect(m_sSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            &ColorPopover::updateFromHsl);
    connect(m_lSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            &ColorPopover::updateFromHsl);
    connect(m_alphaSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [this](double) { updateFromAlpha(m_alphaSpin->value() / 100.0); });

    connect(m_colorField, &ColorPlaneWidget::picked, this,
            [this](double s, double l) { updateFromPlane(s, l); });
    connect(m_hueSlider, &HueSliderWidget::hueChanged, this,
            [this](double hue) { updateFromHue(hue); });
    connect(m_alphaSlider, &AlphaSliderWidget::alphaChanged, this,
            [this](double alpha) { updateFromAlpha(alpha); });
}

void ColorPopover::syncEditors() {
    m_blockSignals = true;

    m_hexEdit->setText(m_color.name(QColor::HexArgb).toUpper());

    const double hue = m_color.hueF() < 0 ? 0.0 : m_color.hueF();
    const double saturation = m_color.saturationF();
    const double lightness = m_color.lightnessF();
    const double alpha = m_color.alphaF();

    m_hSpin->setValue(hue * 360.0);
    m_sSpin->setValue(saturation * 100.0);
    m_lSpin->setValue(lightness * 100.0);
    if (m_alphaSpin) {
        m_alphaSpin->setValue(alpha * 100.0);
    }

    if (m_colorField) {
        m_colorField->setHue(hue);
        m_colorField->setPoint(saturation, lightness);
    }
    if (m_hueSlider) {
        m_hueSlider->setHue(hue);
    }
    if (m_alphaSlider) {
        QColor base = m_color;
        base.setAlphaF(1.0);
        m_alphaSlider->setBaseColor(base);
        m_alphaSlider->setAlpha(alpha);
    }

    updatePreview();

    m_blockSignals = false;
}

void ColorPopover::updatePreview() {
    if (!m_preview) {
        return;
    }
    const QString border = QString::fromUtf8(theme::Tokens::StrokeSoft);
    const QString color = m_color.name(QColor::HexArgb);
    m_preview->setStyleSheet(
        QStringLiteral("background-color: %1; border: 1px solid %2; border-radius: 12px;")
            .arg(color, border));
}

void ColorPopover::updateFromHex() {
    if (m_blockSignals) {
        return;
    }
    QString text = m_hexEdit->text().trimmed();
    if (!text.startsWith('#')) {
        text.prepend('#');
    }
    QColor parsed(text);
    if (!parsed.isValid()) {
        return;
    }
    m_color = parsed;
    syncEditors();
    emit colorSelected(m_color);
}

void ColorPopover::updateFromHsl() {
    if (m_blockSignals) {
        return;
    }
    const double h = clamp01(m_hSpin->value() / 360.0);
    const double s = clamp01(m_sSpin->value() / 100.0);
    const double l = clamp01(m_lSpin->value() / 100.0);
    const double a = clamp01(m_alphaSpin ? m_alphaSpin->value() / 100.0 : m_color.alphaF());

    QColor parsed = colorFromHsl(h, s, l, a);
    if (!parsed.isValid()) {
        return;
    }
    m_color = parsed;
    syncEditors();
    emit colorSelected(m_color);
}

void ColorPopover::updateFromPlane(double saturation, double lightness) {
    if (m_blockSignals) {
        return;
    }
    const double hue = m_hueSlider ? m_hueSlider->hue() : m_color.hueF();
    const double alpha = m_alphaSpin ? m_alphaSpin->value() / 100.0 : m_color.alphaF();
    m_color = colorFromHsl(hue, saturation, lightness, alpha);
    syncEditors();
    emit colorSelected(m_color);
}

void ColorPopover::updateFromHue(double hue) {
    if (m_blockSignals) {
        return;
    }
    const double saturation = m_colorField ? m_colorField->saturation() : m_color.saturationF();
    const double lightness = m_colorField ? m_colorField->lightness() : m_color.lightnessF();
    const double alpha = m_alphaSpin ? m_alphaSpin->value() / 100.0 : m_color.alphaF();
    m_color = colorFromHsl(hue, saturation, lightness, alpha);
    syncEditors();
    emit colorSelected(m_color);
}

void ColorPopover::updateFromAlpha(double alpha) {
    if (m_blockSignals) {
        return;
    }
    alpha = clamp01(alpha);
    QColor updated = m_color;
    updated.setAlphaF(alpha);
    m_color = updated;
    syncEditors();
    emit colorSelected(m_color);
}

} // namespace ui::widgets

#include "ColorPopover.moc"
