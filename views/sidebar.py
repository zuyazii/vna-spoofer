from __future__ import annotations

from pathlib import Path
from typing import Dict, List

from PySide6.QtCore import QLocale, Qt, Signal, QSize, QStringListModel
from PySide6.QtGui import QIcon
from PySide6.QtWidgets import (
    QFrame,
    QHBoxLayout,
    QLabel,
    QListView,
    QSizePolicy,
    QToolButton,
    QVBoxLayout,
    QWidget,
)

from tokens import TOKENS
from widgets.inputs import PointsSpinBox, PrecisionDoubleSpinBox

ICON_DIR = Path(__file__).resolve().parent.parent / "assets" / "icons"


def load_icon(name: str) -> QIcon:
    icon_path = ICON_DIR / name
    icon = QIcon(str(icon_path))
    return icon if not icon.isNull() else QIcon()


class SidebarView(QFrame):
    rangeChanged = Signal(float, float)
    pointsChanged = Signal(int)
    scanRequested = Signal()
    loadCalibrationRequested = Signal()
    collapseRequested = Signal()

    def __init__(self, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        self.setObjectName("SidebarView")
        self.setFixedWidth(280)
        self.setFrameStyle(0)
        self.setContentsMargins(0, 0, 0, 0)

        self._locale = QLocale(QLocale.English)
        self._content_widgets: List[QWidget] = []
        self._collapse_icon_expanded = load_icon("ooui_collapse.svg")
        self._collapse_icon_collapsed = load_icon("menu.svg")
        self._scan_button: QToolButton | None = None
        self._load_button: QToolButton | None = None

        self._build_ui()

    def _build_ui(self) -> None:
        layout = QVBoxLayout(self)
        layout.setContentsMargins(16, 16, 16, 16)
        layout.setSpacing(16)

        layout.addLayout(self._build_header())
        freq_card = self._build_frequency_card()
        points_card = self._build_points_card()
        vna_card = self._build_list_card("section_vna", action="scan")
        cal_card = self._build_list_card("section_calibration", action="load")

        layout.addWidget(freq_card)
        layout.addWidget(points_card)
        layout.addWidget(vna_card)
        layout.addWidget(cal_card)
        layout.addStretch(1)

        self._content_widgets.extend([freq_card, points_card, vna_card, cal_card])

    def _build_header(self) -> QHBoxLayout:
        header = QHBoxLayout()
        header.setContentsMargins(0, 0, 0, 0)
        header.setSpacing(8)

        icon_label = QLabel(self)
        sidebar_icon = load_icon("Side Bar Icon.svg")
        if not sidebar_icon.isNull():
            icon_label.setPixmap(sidebar_icon.pixmap(QSize(20, 20)))
        else:
            icon_label.setVisible(False)
        header.addWidget(icon_label)

        self._title_label = QLabel("VNA", self)
        self._title_label.setFont(TOKENS.qfont("md", "bold"))
        header.addWidget(self._title_label)

        header.addStretch(1)

        self._collapse_button = QToolButton(self)
        if self._collapse_icon_expanded.isNull():
            self._collapse_button.setToolButtonStyle(Qt.ToolButtonTextOnly)
            self._collapse_button.setText("<")
        else:
            self._collapse_button.setToolButtonStyle(Qt.ToolButtonIconOnly)
            self._collapse_button.setIcon(self._collapse_icon_expanded)
            self._collapse_button.setIconSize(QSize(16, 16))
        self._collapse_button.setCursor(Qt.PointingHandCursor)
        self._collapse_button.setToolTip("Collapse sidebar")
        self._collapse_button.clicked.connect(self.collapseRequested.emit)
        header.addWidget(self._collapse_button)
        return header

    def _build_frequency_card(self) -> QWidget:
        card = QFrame(self)
        card.setProperty("variant", "card")
        wrapper = QVBoxLayout(card)
        wrapper.setContentsMargins(12, 12, 12, 12)
        wrapper.setSpacing(8)

        self._frequency_label = QLabel("Frequency Range", card)
        self._frequency_label.setFont(TOKENS.qfont("sm", "medium"))
        wrapper.addWidget(self._frequency_label)

        content_row = QHBoxLayout()
        content_row.setSpacing(6)

        start_column = QVBoxLayout()
        start_column.setSpacing(4)
        self._start_spin = PrecisionDoubleSpinBox(0.0, 30.0, 0.001, card)
        self._start_spin.setValue(1.000)
        self._start_spin.setLocale(self._locale)
        self._start_spin.valueCommitted.connect(self._emit_range_change)
        start_column.addWidget(self._start_spin)
        self._start_hint = self._build_hint_label("start")
        start_column.addWidget(self._start_hint, alignment=Qt.AlignLeft)
        content_row.addLayout(start_column, 1)

        self._range_separator = QLabel("\u2013", card)
        self._range_separator.setAlignment(Qt.AlignCenter)
        content_row.addWidget(self._range_separator)

        end_column = QVBoxLayout()
        end_column.setSpacing(4)
        self._end_spin = PrecisionDoubleSpinBox(0.0, 30.0, 0.001, card)
        self._end_spin.setValue(6.000)
        self._end_spin.setLocale(self._locale)
        self._end_spin.valueCommitted.connect(self._emit_range_change)
        end_column.addWidget(self._end_spin)
        self._end_hint = self._build_hint_label("end")
        end_column.addWidget(self._end_hint, alignment=Qt.AlignLeft)
        content_row.addLayout(end_column, 1)

        self._unit_label = QLabel("GHz", card)
        self._unit_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        self._unit_label.setSizePolicy(QSizePolicy.Fixed, QSizePolicy.Preferred)
        content_row.addWidget(self._unit_label)

        wrapper.addLayout(content_row)
        return card

    def _build_points_card(self) -> QWidget:
        card = QFrame(self)
        card.setProperty("variant", "card")
        layout = QVBoxLayout(card)
        layout.setContentsMargins(12, 12, 12, 12)
        layout.setSpacing(8)

        self._points_label = QLabel("Points", card)
        self._points_label.setFont(TOKENS.qfont("sm", "medium"))
        layout.addWidget(self._points_label)

        row = QHBoxLayout()
        row.setSpacing(8)
        self._points_spin = PointsSpinBox(1, 4001, 2, card)
        self._points_spin.setValue(201)
        self._points_spin.setLocale(self._locale)
        self._points_spin.valueCommitted.connect(self.pointsChanged.emit)
        row.addWidget(self._points_spin)

        self._points_suffix = QLabel("Points", card)
        self._points_suffix.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        row.addWidget(self._points_suffix)
        layout.addLayout(row)

        self._points_hint = self._build_hint_label("sample points")
        layout.addWidget(self._points_hint)

        return card

    def _build_list_card(self, section_key: str, action: str) -> QWidget:
        card = QFrame(self)
        card.setProperty("variant", "card")
        layout = QVBoxLayout(card)
        layout.setContentsMargins(12, 12, 12, 12)
        layout.setSpacing(12)

        header_row = QHBoxLayout()
        header_row.setSpacing(8)

        title = QLabel(section_key, card)
        title.setFont(TOKENS.qfont("sm", "medium"))
        header_row.addWidget(title)
        header_row.addStretch(1)

        action_button = QToolButton(card)
        action_button.setCursor(Qt.PointingHandCursor)
        icon_name = "system-status.svg" if action == "scan" else "upload.svg"
        action_icon = load_icon(icon_name)
        default_text = "Scan" if action == "scan" else "Load"
        if action_icon.isNull():
            action_button.setToolButtonStyle(Qt.ToolButtonTextOnly)
            action_button.setText(default_text)
        else:
            action_button.setToolButtonStyle(Qt.ToolButtonIconOnly)
            action_button.setIcon(action_icon)
            action_button.setIconSize(QSize(18, 18))
            action_button.setToolTip(default_text)
        action_button.clicked.connect(self.scanRequested.emit if action == "scan" else self.loadCalibrationRequested.emit)
        header_row.addWidget(action_button)

        layout.addLayout(header_row)

        view = QListView(card)
        view.setEditTriggers(QListView.NoEditTriggers)
        view.setSelectionMode(QListView.SingleSelection)
        view.setVerticalScrollMode(QListView.ScrollPerPixel)
        view.setHorizontalScrollBarPolicy(Qt.ScrollBarAlwaysOff)
        layout.addWidget(view)

        model = QStringListModel([])
        view.setModel(model)

        if action == "scan":
            self._vna_label = title
            self._vna_model = model
            self._scan_button = action_button
        else:
            self._calibration_label = title
            self._calibration_model = model
            self._load_button = action_button

        return card

    def _build_hint_label(self, text: str) -> QLabel:
        label = QLabel(text, self)
        label.setFont(TOKENS.qfont("xs", "regular"))
        label.setProperty("role", "muted")
        return label

    def _emit_range_change(self, _value: float) -> None:
        self.rangeChanged.emit(self._start_spin.value(), self._end_spin.value())

    def apply_translations(self, strings: Dict) -> None:
        sidebar = strings.get("sidebar", {})

        self._title_label.setText(sidebar.get("section_vna", "VNA"))

        self._frequency_label.setText(sidebar.get("frequency_range", "Frequency Range"))
        self._range_separator.setText(f" {sidebar.get('separator', '\u2013')} ")
        self._unit_label.setText(sidebar.get("unit_ghz", "GHz"))
        self._start_hint.setText(sidebar.get("start", "start"))
        self._end_hint.setText(sidebar.get("end", "end"))

        self._points_label.setText(sidebar.get("points", "Points"))
        self._points_suffix.setText(sidebar.get("points_suffix", "Points"))
        self._points_hint.setText(sidebar.get("points_hint", "sample points"))

        self._vna_label.setText(sidebar.get("section_vna", "VNA"))
        self._calibration_label.setText(sidebar.get("section_calibration", "Calibration"))

        scan_text = sidebar.get("scan", "Scan")
        load_text = sidebar.get("load", "Load")
        if self._scan_button is not None:
            if self._scan_button.toolButtonStyle() == Qt.ToolButtonTextOnly:
                self._scan_button.setText(scan_text)
            else:
                self._scan_button.setToolTip(scan_text)
        if self._load_button is not None:
            if self._load_button.toolButtonStyle() == Qt.ToolButtonTextOnly:
                self._load_button.setText(load_text)
            else:
                self._load_button.setToolTip(load_text)

        self._vna_model.setStringList(sidebar.get("list_vna", []))
        self._calibration_model.setStringList(sidebar.get("list_calibration", []))

        collapse_hint = strings.get("buttons", {}).get("menu", "Toggle menu")
        self._collapse_button.setToolTip(collapse_hint)

    def set_locale(self, locale: QLocale) -> None:
        self._locale = locale
        self._start_spin.setLocale(locale)
        self._end_spin.setLocale(locale)
        self._points_spin.setLocale(locale)

    def setCollapsed(self, collapsed: bool) -> None:
        for widget in self._content_widgets:
            widget.setVisible(not collapsed)
        if self._collapse_icon_expanded.isNull():
            self._collapse_button.setText(">" if collapsed else "<")
        else:
            self._collapse_button.setIcon(
                self._collapse_icon_collapsed if collapsed else self._collapse_icon_expanded
            )
        if collapsed:
            self.setMinimumWidth(60)
        else:
            self.setMinimumWidth(0)

    def default_values(self) -> Dict[str, float]:
        return {
            "start": self._start_spin.value(),
            "end": self._end_spin.value(),
            "points": self._points_spin.value(),
        }
