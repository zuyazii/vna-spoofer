from __future__ import annotations

from dataclasses import dataclass
from typing import Optional

from PySide6.QtCore import QPoint, Qt, Signal
from PySide6.QtGui import QColor, QPainter, QPen
from PySide6.QtWidgets import (
    QAbstractButton,
    QButtonGroup,
    QFrame,
    QGridLayout,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QPushButton,
    QSlider,
    QStackedWidget,
    QVBoxLayout,
    QWidget,
)

from tokens import TOKENS


class ColorChipButton(QAbstractButton):
    """
    Flat square button that displays the active color chip.
    QSS provides the border; we paint the fill to keep everything in sync.
    """

    colorActivated = Signal()

    def __init__(self, color: QColor, parent: Optional[QWidget] = None) -> None:
        super().__init__(parent)
        self.setCheckable(False)
        self.setCursor(Qt.PointingHandCursor)
        self.setFixedSize(32, 32)
        self.setProperty("group", "color-chip")
        self._color = QColor(color)

    def setColor(self, color: QColor) -> None:
        self._color = QColor(color)
        self.update()

    def color(self) -> QColor:
        return QColor(self._color)

    def paintEvent(self, event) -> None:  # noqa: D401
        painter = QPainter(self)
        painter.setRenderHint(QPainter.Antialiasing, True)
        inset = 4
        rect = self.rect().adjusted(inset, inset, -inset, -inset)
        painter.setBrush(self._color)
        pen = QPen(self._color.darker(110))
        pen.setWidthF(0.5)
        painter.setPen(pen)
        painter.drawRoundedRect(rect, TOKENS.radius["sm"], TOKENS.radius["sm"])

    def mouseReleaseEvent(self, event) -> None:
        if event.button() == Qt.LeftButton and self.rect().contains(event.position().toPoint()):
            self.colorActivated.emit()
        super().mouseReleaseEvent(event)


class ColorPickerPopover(QFrame):
    """
    Lightweight, non-blocking color picker with HSL sliders and HEX entry.
    """

    colorChanged = Signal(QColor)
    dismissed = Signal()

    def __init__(self, parent: Optional[QWidget] = None) -> None:
        super().__init__(parent, Qt.Popup)
        self.setAttribute(Qt.WA_StyledBackground, True)
        self.setProperty("role", "popover")
        self._color = QColor(TOKENS.color["series.s11"])
        self._building = False

        self._build_ui()
        self._sync_ui_from_color()

    def _build_ui(self) -> None:
        layout = QVBoxLayout(self)
        layout.setContentsMargins(16, 16, 16, 16)
        layout.setSpacing(12)

        title = QLabel(self)
        title.setFont(TOKENS.qfont("sm", "medium"))
        title.setText("")
        title.setObjectName("ColorPickerTitle")
        layout.addWidget(title)
        self._title_label = title

        self._stack = QStackedWidget(self)
        layout.addWidget(self._stack)

        self._build_hsl_panel()
        self._build_hex_panel()

        toggle_row = QHBoxLayout()
        toggle_row.setSpacing(8)
        toggle_row.addStretch(1)
        self._mode_group = QButtonGroup(self)
        self._mode_group.setExclusive(True)
        self._mode_group.idClicked.connect(self._stack.setCurrentIndex)

        self._hsl_button = QPushButton("HSL", self)
        self._hsl_button.setCheckable(True)
        self._hsl_button.setChecked(True)
        self._mode_group.addButton(self._hsl_button, 0)
        toggle_row.addWidget(self._hsl_button)

        self._hex_button = QPushButton("HEX", self)
        self._hex_button.setCheckable(True)
        self._mode_group.addButton(self._hex_button, 1)
        toggle_row.addWidget(self._hex_button)
        layout.addLayout(toggle_row)

        self._mode_group.buttonClicked.connect(lambda _btn: self._sync_toggle_state())

    def _build_hsl_panel(self) -> None:
        panel = QWidget(self)
        grid = QGridLayout(panel)
        grid.setHorizontalSpacing(12)
        grid.setVerticalSpacing(12)
        grid.setContentsMargins(0, 0, 0, 0)

        self._hue_slider = self._make_slider(0, 359)
        self._sat_slider = self._make_slider(0, 100)
        self._light_slider = self._make_slider(0, 100)

        self._hue_value = QLabel(panel)
        self._sat_value = QLabel(panel)
        self._light_value = QLabel(panel)

        h_label = QLabel("H", panel)
        h_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        h_label.setFont(TOKENS.qfont("sm"))
        grid.addWidget(h_label, 0, 0)
        grid.addWidget(self._hue_slider, 0, 1)
        grid.addWidget(self._hue_value, 0, 2)

        s_label = QLabel("S", panel)
        s_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        s_label.setFont(TOKENS.qfont("sm"))
        grid.addWidget(s_label, 1, 0)
        grid.addWidget(self._sat_slider, 1, 1)
        grid.addWidget(self._sat_value, 1, 2)

        l_label = QLabel("L", panel)
        l_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        l_label.setFont(TOKENS.qfont("sm"))
        grid.addWidget(l_label, 2, 0)
        grid.addWidget(self._light_slider, 2, 1)
        grid.addWidget(self._light_value, 2, 2)

        for value_label in (self._hue_value, self._sat_value, self._light_value):
            value_label.setAlignment(Qt.AlignCenter)
            value_label.setFixedWidth(40)
            value_label.setFont(TOKENS.qfont("sm", "medium"))

        self._stack.addWidget(panel)

        self._hue_slider.valueChanged.connect(self._update_color_from_sliders)
        self._sat_slider.valueChanged.connect(self._update_color_from_sliders)
        self._light_slider.valueChanged.connect(self._update_color_from_sliders)

    def _build_hex_panel(self) -> None:
        panel = QWidget(self)
        layout = QVBoxLayout(panel)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(12)

        self._hex_edit = QLineEdit(panel)
        self._hex_edit.setPlaceholderText("#E2554E")
        self._hex_edit.textChanged.connect(self._update_color_from_hex)
        layout.addWidget(self._hex_edit)
        self._stack.addWidget(panel)

    def _make_slider(self, minimum: int, maximum: int) -> QSlider:
        slider = QSlider(Qt.Horizontal, self)
        slider.setRange(minimum, maximum)
        slider.setSingleStep(1)
        slider.setPageStep(5)
        slider.setFixedHeight(18)
        return slider

    def _sync_toggle_state(self) -> None:
        active_id = self._stack.currentIndex()
        for button in self._mode_group.buttons():
            button.setChecked(self._mode_group.id(button) == active_id)

    def _update_color_from_sliders(self) -> None:
        if self._building:
            return
        h = self._hue_slider.value()
        s = self._sat_slider.value()
        l = self._light_slider.value()
        color = QColor()
        color.setHsl(h, int(s / 100 * 255), int(l / 100 * 255))
        self.setColor(color, emit_signal=True)

    def _update_color_from_hex(self, text: str) -> None:
        if self._building:
            return
        c = QColor(text)
        if c.isValid():
            self.setColor(c, emit_signal=True)

    def _sync_ui_from_color(self) -> None:
        self._building = True
        color = self._color
        h, s, l, _ = color.getHsl()
        self._hue_slider.setValue(h)
        self._sat_slider.setValue(int(s / 255 * 100))
        self._light_slider.setValue(int(l / 255 * 100))
        self._hue_value.setText(f"{h:03d}")
        self._sat_value.setText(f"{int(s / 255 * 100):03d}")
        self._light_value.setText(f"{int(l / 255 * 100):03d}")
        self._hex_edit.setText(color.name().upper())
        self._building = False

    def set_strings(self, title: str, hsl_label: str, hex_label: str) -> None:
        self._title_label.setText(title)
        self._hsl_button.setText(hsl_label)
        self._hex_button.setText(hex_label)

    def setColor(self, color: QColor, emit_signal: bool = False) -> None:
        if not color.isValid():
            return
        if color == self._color:
            return
        self._color = QColor(color)
        self._sync_ui_from_color()
        if emit_signal:
            self.colorChanged.emit(QColor(self._color))

    def color(self) -> QColor:
        return QColor(self._color)

    def popup(self, anchor: QWidget) -> None:
        if not anchor:
            return
        global_pos = anchor.mapToGlobal(anchor.rect().bottomLeft())
        self.move(global_pos + QPoint(0, 8))
        self.show()
        self.raise_()
        self.activateWindow()

    def closeEvent(self, event) -> None:
        self.dismissed.emit()
        super().closeEvent(event)
