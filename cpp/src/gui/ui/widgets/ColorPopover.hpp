// Copyright (c) 2025 vna-spoofer
// SPDX-License-Identifier: MIT

#pragma once

#include <QColor>
#include <QWidget>

class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class QPushButton;
class QKeyEvent;
class QHideEvent;

namespace ui::widgets {

namespace detail {
class ColorPlaneWidget;
class HueSliderWidget;
class AlphaSliderWidget;
} // namespace detail

class ColorPopover : public QWidget {
    Q_OBJECT

public:
    explicit ColorPopover(QWidget* parent = nullptr);

    void openFor(QWidget* anchor, const QColor& initial);
    QColor color() const;

signals:
    void colorSelected(const QColor& color);
    void closed();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    void buildUi();
    void syncEditors();
    void updatePreview();
    void updateFromHex();
    void updateFromHsl();
    void updateFromPlane(double saturation, double lightness);
    void updateFromHue(double hue);
    void updateFromAlpha(double alpha);

    QColor m_color;
    bool m_blockSignals = false;
    QWidget* m_preview = nullptr;
    detail::ColorPlaneWidget* m_colorField = nullptr;
    detail::HueSliderWidget* m_hueSlider = nullptr;
    detail::AlphaSliderWidget* m_alphaSlider = nullptr;
    QPushButton* m_dropperButton = nullptr;
    QComboBox* m_modeCombo = nullptr;
    QLineEdit* m_hexEdit = nullptr;
    QDoubleSpinBox* m_hSpin = nullptr;
    QDoubleSpinBox* m_sSpin = nullptr;
    QDoubleSpinBox* m_lSpin = nullptr;
    QDoubleSpinBox* m_alphaSpin = nullptr;
};

} // namespace ui::widgets
