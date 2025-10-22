// Copyright (c) 2025 vna-spoofer
// SPDX-License-Identifier: MIT

#pragma once

#include <QColor>
#include <QWidget>

class QDoubleSpinBox;
class QLineEdit;

class QKeyEvent;
class QHideEvent;

namespace ui::widgets {

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

    QColor m_color;
    bool m_blockSignals = false;
    QWidget* m_preview = nullptr;
    QLineEdit* m_hexEdit = nullptr;
    QDoubleSpinBox* m_hSpin = nullptr;
    QDoubleSpinBox* m_sSpin = nullptr;
    QDoubleSpinBox* m_lSpin = nullptr;
};

} // namespace ui::widgets
