from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Tuple

from PySide6.QtCore import QLocale, Qt, Signal, QSize
from PySide6.QtGui import QColor, QIcon
from PySide6.QtWidgets import (
    QCheckBox,
    QFrame,
    QGridLayout,
    QHBoxLayout,
    QLabel,
    QPushButton,
    QToolButton,
    QVBoxLayout,
    QWidget,
)

from tokens import TOKENS, SERIES_NAMES, iter_series_colors
from widgets.color_dialog import RGBColorDialog
from widgets.color_picker import ColorChipButton
from widgets.collapsible import CollapsibleSection

ICON_DIR = Path(__file__).resolve().parent.parent / "assets" / "icons"


def load_icon(name: str) -> QIcon:
    icon_path = ICON_DIR / name
    icon = QIcon(str(icon_path))
    return icon if not icon.isNull() else QIcon()


@dataclass
class SeriesControls:
    checkbox: QCheckBox
    chip: ColorChipButton
    dot: QLabel


class BottomBar(QFrame):
    languageChanged = Signal(str)
    seriesVisibilityChanged = Signal(str, str, bool)
    seriesColorChanged = Signal(str, str, QColor)
    startSweepRequested = Signal()
    stopSweepRequested = Signal()
    resetRequested = Signal()
    collapseRequested = Signal()

    def __init__(self, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        self.setObjectName("BottomBar")
        self.setProperty("variant", "card")

        self._current_language = "en"
        self._language_labels: Dict[str, str] = {
            "en": "English",
            "zh_Hant": "繁體中文",
            "zh_Hans": "简体中文",
        }
        self._action_tooltips: Dict[str, str] = {
            "start": "Start Sweep",
            "stop": "Stop",
            "reset": "Reset",
        }
        self._collapse_icon_expanded = load_icon("ooui_collapse-1.svg")
        self._collapse_icon_collapsed = load_icon("ooui_collapse.svg")

        self._series_colors: Dict[Tuple[str, str], QColor] = {}
        for domain in ("magnitude", "phase"):
            for name, color in iter_series_colors():
                adjusted = QColor(color)
                if domain == "phase":
                    adjusted = adjusted.darker(120)
                self._series_colors[(name, domain)] = adjusted

        self._controls: Dict[Tuple[str, str], SeriesControls] = {}
        self._content_widgets: list[QWidget] = []

        self._build_ui()

    def _build_ui(self) -> None:
        root = QVBoxLayout(self)
        root.setContentsMargins(16, 16, 16, 16)
        root.setSpacing(16)

        content_row = QHBoxLayout()
        content_row.setContentsMargins(0, 0, 0, 0)
        content_row.setSpacing(32)

        language_column = QVBoxLayout()
        language_column.setContentsMargins(0, 0, 0, 0)
        language_column.setSpacing(8)

        self._language_label = QLabel("Language", self)
        self._language_label.setFont(TOKENS.qfont("sm", "medium"))
        language_column.addWidget(self._language_label)

        self._language_buttons: Dict[str, QCheckBox] = {}
        for code in ("en", "zh_Hant", "zh_Hans"):
            checkbox = QCheckBox(self._language_labels[code], self)
            checkbox.setChecked(code == "en")
            checkbox.toggled.connect(lambda checked, c=code: checked and self._emit_language_changed(c))
            language_column.addWidget(checkbox)
            self._language_buttons[code] = checkbox

        language_widget = QWidget(self)
        language_widget.setLayout(language_column)
        content_row.addWidget(language_widget)

        controls_layout = QVBoxLayout()
        controls_layout.setContentsMargins(0, 0, 0, 0)
        controls_layout.setSpacing(12)

        magnitude_content = self._build_domain_widget("magnitude")
        phase_content = self._build_domain_widget("phase")

        magnitude_icon = load_icon("line.svg")
        phase_icon = load_icon("Hue_selector.svg")

        self._magnitude_section = CollapsibleSection("Magnitude", magnitude_content, self, icon=magnitude_icon)
        self._phase_section = CollapsibleSection("Phase", phase_content, self, icon=phase_icon)

        controls_layout.addWidget(self._magnitude_section)
        controls_layout.addWidget(self._phase_section)

        control_widget = QWidget(self)
        control_widget.setLayout(controls_layout)
        content_row.addWidget(control_widget, 1)

        collapse_column = QVBoxLayout()
        collapse_column.setContentsMargins(0, 0, 0, 0)
        collapse_column.addStretch(1)
        self._collapse_button = QToolButton(self)
        if self._collapse_icon_expanded.isNull() or self._collapse_icon_collapsed.isNull():
            self._collapse_button.setToolButtonStyle(Qt.ToolButtonTextOnly)
            self._collapse_button.setText("v")
        else:
            self._collapse_button.setToolButtonStyle(Qt.ToolButtonIconOnly)
            self._collapse_button.setIcon(self._collapse_icon_expanded)
            self._collapse_button.setIconSize(QSize(16, 16))
        self._collapse_button.setCursor(Qt.PointingHandCursor)
        self._collapse_button.setToolTip("Collapse bottom bar")
        self._collapse_button.clicked.connect(self.collapseRequested.emit)
        collapse_column.addWidget(self._collapse_button, alignment=Qt.AlignRight | Qt.AlignBottom)

        collapse_widget = QWidget(self)
        collapse_widget.setLayout(collapse_column)
        content_row.addWidget(collapse_widget)

        content_container = QWidget(self)
        content_container.setLayout(content_row)
        root.addWidget(content_container)

        self._action_bar = QWidget(self)
        action_layout = QHBoxLayout(self._action_bar)
        action_layout.setContentsMargins(0, 0, 0, 0)
        action_layout.setSpacing(12)

        self._start_button = self._build_action_button("start-test.svg", self._action_tooltips["start"])
        self._start_button.clicked.connect(self.startSweepRequested.emit)
        action_layout.addWidget(self._start_button)

        self._stop_button = self._build_action_button("stop.svg", self._action_tooltips["stop"])
        self._stop_button.clicked.connect(self.stopSweepRequested.emit)
        action_layout.addWidget(self._stop_button)

        self._reset_button = self._build_action_button("reset.svg", self._action_tooltips["reset"])
        self._reset_button.clicked.connect(self.resetRequested.emit)
        action_layout.addWidget(self._reset_button)

        action_layout.addStretch(1)
        root.addWidget(self._action_bar)

        self._content_widgets.extend([content_container, self._action_bar])

    def _build_action_button(self, icon_name: str, tooltip: str) -> QPushButton:
        button = QPushButton("", self)
        button.setCursor(Qt.PointingHandCursor)
        icon = load_icon(icon_name)
        if icon.isNull():
            button.setText(tooltip)
        else:
            button.setIcon(icon)
            button.setIconSize(QSize(20, 20))
        button.setToolTip(tooltip)
        button.setCheckable(False)
        return button

    def _build_domain_widget(self, domain: str) -> QWidget:
        container = QWidget(self)
        layout = QGridLayout(container)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setHorizontalSpacing(24)
        layout.setVerticalSpacing(6)

        for column_index, name in enumerate(SERIES_NAMES):
            checkbox = QCheckBox(name.upper(), container)
            checkbox.setChecked(True)
            checkbox.stateChanged.connect(
                lambda _state, series=name, dom=domain, cb=checkbox: self._handle_visibility(series, dom, cb.isChecked())
            )
            layout.addWidget(checkbox, 0, column_index, alignment=Qt.AlignHCenter)

            chip = ColorChipButton(self._series_colors[(name, domain)], container)
            chip.colorActivated.connect(lambda series=name, dom=domain, cp=chip: self._open_color_dialog(series, dom, cp))
            layout.addWidget(chip, 1, column_index, alignment=Qt.AlignHCenter)

            dot = QLabel(container)
            dot.setFixedSize(12, 12)
            dot.setStyleSheet(f"background: {self._series_colors[(name, domain)].name()}; border-radius: 6px;")
            layout.addWidget(dot, 2, column_index, alignment=Qt.AlignHCenter)

            layout.setColumnStretch(column_index, 1)
            self._controls[(name, domain)] = SeriesControls(checkbox=checkbox, chip=chip, dot=dot)

        return container

    def _emit_language_changed(self, code: str) -> None:
        if code == self._current_language:
            return
        for lang_code, checkbox in self._language_buttons.items():
            checkbox.blockSignals(True)
            checkbox.setChecked(lang_code == code)
            checkbox.blockSignals(False)
        self._current_language = code
        self.languageChanged.emit(code)

    def _handle_visibility(self, series: str, domain: str, checked: bool) -> None:
        self.seriesVisibilityChanged.emit(series, domain, checked)

    def _open_color_dialog(self, series: str, domain: str, chip: ColorChipButton) -> None:
        current = self._series_colors[(series, domain)]
        color, ok = RGBColorDialog.getColor(current, self)
        if not ok:
            return
        self._series_colors[(series, domain)] = color
        chip.setColor(color)
        controls = self._controls[(series, domain)]
        controls.dot.setStyleSheet(f"background: {color.name()}; border-radius: 6px;")
        self.seriesColorChanged.emit(series, domain, color)

    def apply_translations(self, bundle: Dict) -> None:
        bottom = bundle.get("bottom_bar", {})
        languages = bottom.get("languages", {})

        self._language_label.setText(bottom.get("language", "Language"))
        self._magnitude_section.setTitle(bottom.get("magnitude", "Magnitude"))
        self._phase_section.setTitle(bottom.get("phase", "Phase"))

        for code, label in languages.items():
            self._language_labels[code] = label

        for code, checkbox in self._language_buttons.items():
            checkbox.setText(self._language_labels.get(code, code))

        self._action_tooltips["start"] = bottom.get("start", self._action_tooltips["start"])
        self._action_tooltips["stop"] = bottom.get("stop", self._action_tooltips["stop"])
        self._action_tooltips["reset"] = bottom.get("reset", self._action_tooltips["reset"])

        self._start_button.setToolTip(self._action_tooltips["start"])
        if self._start_button.icon().isNull():
            self._start_button.setText(self._action_tooltips["start"])
        self._stop_button.setToolTip(self._action_tooltips["stop"])
        if self._stop_button.icon().isNull():
            self._stop_button.setText(self._action_tooltips["stop"])
        self._reset_button.setToolTip(self._action_tooltips["reset"])
        if self._reset_button.icon().isNull():
            self._reset_button.setText(self._action_tooltips["reset"])

    def set_locale(self, locale: QLocale) -> None:  # noqa: ARG002
        return

    def set_current_language(self, language: str) -> None:
        self._emit_language_changed(language)

    def setCollapsed(self, collapsed: bool) -> None:
        for widget in self._content_widgets:
            widget.setVisible(not collapsed)
        if self._collapse_icon_expanded.isNull() or self._collapse_icon_collapsed.isNull():
            self._collapse_button.setText("^" if collapsed else "v")
        else:
            if self._collapse_button.toolButtonStyle() != Qt.ToolButtonIconOnly:
                self._collapse_button.setToolButtonStyle(Qt.ToolButtonIconOnly)
                self._collapse_button.setIconSize(QSize(16, 16))
            self._collapse_button.setIcon(
                self._collapse_icon_collapsed if collapsed else self._collapse_icon_expanded
            )
        if collapsed:
            self.setMinimumHeight(48)
        else:
            self.setMinimumHeight(0)
