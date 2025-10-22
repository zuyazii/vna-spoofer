#include "ColorPopover.hpp"

#include "../theme/DesignTokens.hpp"

#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPalette>
#include <QPointer>
#include <QScreen>
#include <QVBoxLayout>

namespace ui::widgets {

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
    m_color = initial.isValid() ? initial : QColor(QString::fromUtf8(theme::Tokens::S11));
    syncEditors();
    updatePreview();

    const QPoint anchorBottomRight = anchor->mapToGlobal(anchor->rect().bottomRight());
    move(anchorBottomRight + QPoint(8, 8));

    // Ensure the popover stays on screen.
    if (const QScreen* screen = anchor->screen()) {
        QRect available = screen->availableGeometry();
        QRect geometry = this->geometry();
        if (!available.contains(geometry)) {
            geometry.moveLeft(std::min(geometry.left(), available.right() - geometry.width()));
            geometry.moveTop(std::min(geometry.top(), available.bottom() - geometry.height()));
            setGeometry(geometry);
        }
    }

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
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    m_preview = new QWidget(this);
    m_preview->setFixedSize(48, 48);
    m_preview->setAutoFillBackground(true);
    m_preview->setProperty("type", "card");
    layout->addWidget(m_preview, 0, Qt::AlignHCenter);

    auto addHexRow = [this, layout]() {
        auto* label = new QLabel(tr("HEX"), this);
        label->setProperty("role", "subtitle");
        layout->addWidget(label);

        m_hexEdit = new QLineEdit(this);
        m_hexEdit->setMaxLength(7);
        m_hexEdit->setPlaceholderText(QStringLiteral("#000000"));
        layout->addWidget(m_hexEdit);

        connect(m_hexEdit, &QLineEdit::editingFinished, this, &ColorPopover::updateFromHex);
    };

    auto addHslRow = [this, layout](const QString& title, QDoubleSpinBox** spin, double min,
                                    double max, double step) {
        auto* row = new QHBoxLayout;
        row->setSpacing(8);

        auto* label = new QLabel(title, this);
        label->setProperty("role", "subtitle");
        label->setMinimumWidth(48);
        row->addWidget(label);

        auto* sb = new QDoubleSpinBox(this);
        sb->setButtonSymbols(QAbstractSpinBox::NoButtons);
        sb->setDecimals(1);
        sb->setRange(min, max);
        sb->setSingleStep(step);
        sb->setAlignment(Qt::AlignRight);
        sb->setSuffix(title == tr("Hue") ? QStringLiteral("°") : QStringLiteral("%"));
        row->addWidget(sb, 1);

        layout->addLayout(row);
        *spin = sb;

        connect(sb, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
                &ColorPopover::updateFromHsl);
    };

    addHexRow();
    addHslRow(tr("Hue"), &m_hSpin, 0.0, 360.0, 1.0);
    addHslRow(tr("Sat"), &m_sSpin, 0.0, 100.0, 1.0);
    addHslRow(tr("Light"), &m_lSpin, 0.0, 100.0, 1.0);
}

void ColorPopover::syncEditors() {
    m_blockSignals = true;

    m_hexEdit->setText(m_color.name(QColor::HexRgb).toUpper());

    const int hue = qRound(m_color.hueF() * 360.0);
    const int sat = qRound(m_color.saturationF() * 100.0);
    const int light = qRound(m_color.lightnessF() * 100.0);

    m_hSpin->setValue(hue);
    m_sSpin->setValue(sat);
    m_lSpin->setValue(light);

    updatePreview();

    m_blockSignals = false;
}

void ColorPopover::updatePreview() {
    QPalette pal = palette();
    pal.setColor(QPalette::Window, m_color);
    m_preview->setPalette(pal);
    m_preview->setAutoFillBackground(true);
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
    if (parsed == m_color) {
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

    const double h = m_hSpin->value() / 360.0;
    const double s = m_sSpin->value() / 100.0;
    const double l = m_lSpin->value() / 100.0;

    QColor parsed;
    parsed.setHslF(h, s, l);
    if (!parsed.isValid() || parsed == m_color) {
        return;
    }
    m_color = parsed;
    syncEditors();
    emit colorSelected(m_color);
}

} // namespace ui::widgets
