from __future__ import annotations

from typing import Optional

from PySide6.QtCore import QLocale, Signal, Qt
from PySide6.QtWidgets import QAbstractSpinBox, QDoubleSpinBox, QSpinBox


class PrecisionDoubleSpinBox(QDoubleSpinBox):
    """
    3-decimal precision spin box with hidden arrows and monospace alignment.
    """

    valueCommitted = Signal(float)

    def __init__(
        self,
        minimum: float = 0.0,
        maximum: float = 99_999.999,
        step: float = 0.001,
        parent=None,
    ) -> None:
        super().__init__(parent)
        self.setDecimals(3)
        self.setRange(minimum, maximum)
        self.setSingleStep(step)
        self.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        self.setKeyboardTracking(False)
        self.setButtonSymbols(QAbstractSpinBox.NoButtons)
        self.setMinimumHeight(32)
        self.valueChanged.connect(self.valueCommitted.emit)

    def setLocale(self, locale: QLocale) -> None:  # noqa: D401
        super().setLocale(locale)
        self.lineEdit().setLocale(locale)

    def textFromValue(self, value: float) -> str:
        locale = self.locale()
        return locale.toString(value, "f", 3)


class PointsSpinBox(QSpinBox):
    valueCommitted = Signal(int)

    def __init__(self, minimum: int = 1, maximum: int = 10_001, step: int = 1, parent=None) -> None:
        super().__init__(parent)
        self.setRange(minimum, maximum)
        self.setSingleStep(step)
        self.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        self.setButtonSymbols(QAbstractSpinBox.NoButtons)
        self.setMinimumHeight(32)
        self.valueChanged.connect(self.valueCommitted.emit)

    def setLocale(self, locale: QLocale) -> None:  # noqa: D401
        super().setLocale(locale)
        self.lineEdit().setLocale(locale)
