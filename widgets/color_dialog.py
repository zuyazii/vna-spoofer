from __future__ import annotations

from typing import Optional, Tuple

from PySide6.QtCore import Qt
from PySide6.QtGui import QColor
from PySide6.QtWidgets import (
    QDialog,
    QDialogButtonBox,
    QFormLayout,
    QLabel,
    QSlider,
    QSpinBox,
    QVBoxLayout,
)

from tokens import TOKENS


class RGBColorDialog(QDialog):
    """
    Modal RGB color picker with synchronized sliders and spin boxes.
    """

    def __init__(self, color: QColor, parent: Optional = None) -> None:
        super().__init__(parent)
        self.setWindowTitle("Select Color")
        self.setModal(True)
        self.setAttribute(Qt.WA_StyledBackground, True)
        self.setStyleSheet(
            f"""
            QDialog {{
                background: {TOKENS.color['bg.card']};
            }}
            """
        )

        self._color = QColor(color)

        layout = QVBoxLayout(self)
        layout.setContentsMargins(16, 16, 16, 16)
        layout.setSpacing(12)

        form = QFormLayout()
        form.setLabelAlignment(Qt.AlignLeft)
        form.setFormAlignment(Qt.AlignLeft | Qt.AlignVCenter)

        self._controls = []

        for channel, label in zip(("red", "green", "blue"), ("R", "G", "B")):
            slider = QSlider(Qt.Horizontal, self)
            slider.setRange(0, 255)

            spin = QSpinBox(self)
            spin.setRange(0, 255)

            slider.valueChanged.connect(spin.setValue)
            spin.valueChanged.connect(slider.setValue)
            slider.valueChanged.connect(self._update_color)

            form.addRow(QLabel(label, self), slider)
            form.addRow(QLabel("", self), spin)
            self._controls.append((channel, slider, spin))

        layout.addLayout(form)

        self._preview = QLabel(self)
        self._preview.setFixedHeight(40)
        self._preview.setStyleSheet("border: 1px solid #D0C9C2; border-radius: 6px;")
        layout.addWidget(self._preview)

        button_box = QDialogButtonBox(QDialogButtonBox.Ok | QDialogButtonBox.Cancel, self)
        button_box.accepted.connect(self.accept)
        button_box.rejected.connect(self.reject)
        layout.addWidget(button_box)

        self._set_ui_from_color()

    def _set_ui_from_color(self) -> None:
        r, g, b, _ = self._color.getRgb()
        for channel, slider, spin in self._controls:
            value = {"red": r, "green": g, "blue": b}[channel]
            slider.blockSignals(True)
            spin.blockSignals(True)
            slider.setValue(value)
            spin.setValue(value)
            slider.blockSignals(False)
            spin.blockSignals(False)
        self._update_preview()

    def _update_color(self, value: int) -> None:  # noqa: ARG002
        values = {}
        for channel, slider, _ in self._controls:
            values[channel] = slider.value()
        self._color.setRgb(values["red"], values["green"], values["blue"])
        self._update_preview()

    def _update_preview(self) -> None:
        self._preview.setStyleSheet(
            f"border: 1px solid #D0C9C2; border-radius: 6px; background: {self._color.name()};"
        )

    def selectedColor(self) -> QColor:
        return QColor(self._color)

    @staticmethod
    def getColor(color: QColor, parent: Optional = None) -> Tuple[QColor, bool]:
        dialog = RGBColorDialog(color, parent)
        ok = dialog.exec() == QDialog.Accepted
        return dialog.selectedColor(), ok
