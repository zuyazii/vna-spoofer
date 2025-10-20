from __future__ import annotations

from typing import Optional

from PySide6.QtCore import QEasingCurve, QPropertyAnimation, Qt
from PySide6.QtGui import QIcon
from PySide6.QtWidgets import (
    QFrame,
    QHBoxLayout,
    QToolButton,
    QVBoxLayout,
    QWidget,
)

from tokens import TOKENS


class CollapsibleSection(QFrame):
    """
    Simple collapsible container with animated expand/collapse behaviour.
    """

    def __init__(
        self,
        title: str,
        content: QWidget,
        parent: Optional[QWidget] = None,
        icon: Optional[QIcon] = None,
    ) -> None:
        super().__init__(parent)
        self.setProperty("variant", "card")

        self._content = content
        self._content.setParent(self)
        self._icon: Optional[QIcon] = icon if icon is not None and not icon.isNull() else None

        self._main_layout = QVBoxLayout(self)
        self._main_layout.setContentsMargins(12, 12, 12, 12)
        self._main_layout.setSpacing(8)

        header_layout = QHBoxLayout()
        header_layout.setContentsMargins(0, 0, 0, 0)
        header_layout.setSpacing(8)

        self._toggle_button = QToolButton(self)
        self._toggle_button.setCheckable(True)
        self._toggle_button.setChecked(True)
        self._toggle_button.setArrowType(Qt.DownArrow)
        self._toggle_button.setStyleSheet(
            f"""
            QToolButton {{
                border: none;
                padding: 4px;
                border-radius: {TOKENS.radius['sm']}px;
            }}
            QToolButton:hover {{
                background: {TOKENS.color['state.hover']};
            }}
            """
        )
        self._toggle_button.toggled.connect(self._handle_toggle)
        header_layout.addWidget(self._toggle_button, alignment=Qt.AlignLeft)

        self._title_button = QToolButton(self)
        self._title_button.setText(title)
        if self._icon is not None:
            self._title_button.setToolButtonStyle(Qt.ToolButtonTextBesideIcon)
            self._title_button.setIcon(self._icon)
        else:
            self._title_button.setToolButtonStyle(Qt.ToolButtonTextOnly)
        self._title_button.setCheckable(True)
        self._title_button.setChecked(True)
        self._title_button.setStyleSheet(
            f"""
            QToolButton {{
                border: none;
                color: {TOKENS.color['ink.primary']};
                font: {TOKENS.font.size['sm']}pt;
            }}
            """
        )
        self._title_button.toggled.connect(self._handle_toggle)
        header_layout.addWidget(self._title_button, alignment=Qt.AlignLeft)
        header_layout.addStretch(1)
        self._main_layout.addLayout(header_layout)

        self._content_frame = QFrame(self)
        frame_layout = QVBoxLayout(self._content_frame)
        frame_layout.setContentsMargins(0, 0, 0, 0)
        frame_layout.addWidget(self._content)
        self._main_layout.addWidget(self._content_frame)

        self._animation = QPropertyAnimation(self._content_frame, b"maximumHeight", self)
        self._animation.setEasingCurve(QEasingCurve.OutCubic)
        self._animation.setDuration(180)
        self._content_frame.setMaximumHeight(self._content.sizeHint().height())

    def _handle_toggle(self, checked: bool) -> None:
        self._toggle_button.blockSignals(True)
        self._title_button.blockSignals(True)
        self._toggle_button.setChecked(checked)
        self._title_button.setChecked(checked)
        self._toggle_button.blockSignals(False)
        self._title_button.blockSignals(False)

        start_height = self._content_frame.maximumHeight()
        end_height = self._content.sizeHint().height() if checked else 0
        self._animation.stop()
        self._animation.setStartValue(start_height)
        self._animation.setEndValue(end_height)
        self._animation.start()
        self._toggle_button.setArrowType(Qt.DownArrow if checked else Qt.RightArrow)

    def setTitle(self, title: str) -> None:
        self._title_button.setText(title)

    def setIcon(self, icon: Optional[QIcon]) -> None:
        icon = icon if icon is not None and not icon.isNull() else None
        self._icon = icon
        if self._icon is not None:
            self._title_button.setToolButtonStyle(Qt.ToolButtonTextBesideIcon)
            self._title_button.setIcon(self._icon)
        else:
            self._title_button.setToolButtonStyle(Qt.ToolButtonTextOnly)
            self._title_button.setIcon(QIcon())

    def contentWidget(self) -> QWidget:
        return self._content
