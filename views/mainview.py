from __future__ import annotations

from pathlib import Path
from typing import Dict

from PySide6.QtCore import QLocale, QSize, Qt, Signal, QPropertyAnimation, QEasingCurve
from PySide6.QtGui import QColor, QIcon
from PySide6.QtWidgets import (
    QFrame,
    QHBoxLayout,
    QPushButton,
    QSplitter,
    QStackedLayout,
    QVBoxLayout,
    QWidget,
)

from views.bottombar import BottomBar
from views.sidebar import SidebarView
from widgets.plot import PlotWidget


class PlotContainer(QFrame):
    def __init__(self, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        self.setProperty("variant", "card")
        self.setContentsMargins(0, 0, 0, 0)

        layout = QStackedLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setStackingMode(QStackedLayout.StackAll)

        self.plot = PlotWidget(self)
        layout.addWidget(self.plot)

        overlay = QWidget(self)
        overlay.setAttribute(Qt.WA_TransparentForMouseEvents, True)
        overlay_layout = QVBoxLayout(overlay)
        overlay_layout.setContentsMargins(16, 16, 16, 16)
        overlay_layout.setSpacing(0)

        top_bar = QHBoxLayout()
        top_bar.setContentsMargins(0, 0, 0, 0)
        top_bar.setSpacing(0)
        top_bar.addStretch(1)

        icon_path = Path(__file__).resolve().parent.parent / "assets" / "icons" / "preset.svg"
        self.presets_button = QPushButton("", overlay)
        self.presets_button.setObjectName("PresetsButton")
        self.presets_button.setFlat(False)
        self.presets_button.setAttribute(Qt.WA_TransparentForMouseEvents, False)
        self.presets_button.setIcon(QIcon(str(icon_path)))
        self.presets_button.setIconSize(QSize(18, 18))
        top_bar.addWidget(self.presets_button)

        overlay_layout.addLayout(top_bar)
        overlay_layout.addStretch(1)

        layout.addWidget(overlay)


class MainView(QFrame):
    languageChanged = Signal(str)
    rangeChanged = Signal(float, float)
    pointsChanged = Signal(int)
    seriesVisibilityChanged = Signal(str, str, bool)
    seriesColorChanged = Signal(str, str, QColor)
    startSweepRequested = Signal()
    stopSweepRequested = Signal()
    resetRequested = Signal()

    def __init__(self, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        self._locale = QLocale(QLocale.English)
        self._strings: Dict = {}

        self._build_ui()
        self._connect_signals()

        self._sidebar_collapsed = False
        self._bottom_collapsed = False
        self._sidebar_animation: QPropertyAnimation | None = None
        self._bottom_animation: QPropertyAnimation | None = None
        self._sidebar_expanded_width = self.sidebar.sizeHint().width()
        self._bottom_expanded_height = self.bottom_bar.sizeHint().height()

    def _build_ui(self) -> None:
        layout = QHBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)

        self._splitter = QSplitter(Qt.Horizontal, self)
        self._splitter.setChildrenCollapsible(False)
        layout.addWidget(self._splitter)

        self.sidebar = SidebarView(self)
        self._splitter.addWidget(self.sidebar)

        central = QFrame(self)
        central_layout = QVBoxLayout(central)
        central_layout.setContentsMargins(0, 16, 16, 0)
        central_layout.setSpacing(16)

        self.plot_container = PlotContainer(central)
        central_layout.addWidget(self.plot_container, 1)

        self.bottom_bar = BottomBar(central)
        central_layout.addWidget(self.bottom_bar, 0)

        self._splitter.addWidget(central)
        self._splitter.setSizes([268, 960])

    def _connect_signals(self) -> None:
        self.sidebar.rangeChanged.connect(self.rangeChanged.emit)
        self.sidebar.pointsChanged.connect(self.pointsChanged.emit)
        self.sidebar.collapseRequested.connect(self._toggle_sidebar)

        self.bottom_bar.languageChanged.connect(self.languageChanged.emit)
        self.bottom_bar.seriesVisibilityChanged.connect(self.seriesVisibilityChanged.emit)
        self.bottom_bar.seriesColorChanged.connect(self.seriesColorChanged.emit)
        self.bottom_bar.collapseRequested.connect(self._toggle_bottom_bar)
        self.bottom_bar.startSweepRequested.connect(self.startSweepRequested.emit)
        self.bottom_bar.stopSweepRequested.connect(self.stopSweepRequested.emit)
        self.bottom_bar.resetRequested.connect(self.resetRequested.emit)

    @property
    def plot(self) -> PlotWidget:
        return self.plot_container.plot

    def apply_translations(self, bundle: Dict) -> None:
        self._strings = bundle
        self.sidebar.apply_translations(bundle)
        self.bottom_bar.apply_translations(bundle)

        buttons = bundle.get("buttons", {})
        presets_text = buttons.get("presets", "Presets")
        self.plot_container.presets_button.setToolTip(presets_text)

        bottom = bundle.get("bottom_bar", {})
        magnitude = bottom.get("axis_magnitude", "Magnitude (dB)")
        phase = bottom.get("axis_phase", "Phase (deg)")
        frequency = bottom.get("axis_frequency", "Frequency (GHz)")
        self.plot.set_axis_labels(magnitude, phase, frequency)

    def set_locale(self, locale: QLocale) -> None:
        self._locale = locale
        self.sidebar.set_locale(locale)
        self.bottom_bar.set_locale(locale)
        self.plot.apply_locale(locale)

    def set_language(self, language_code: str) -> None:
        self.bottom_bar.set_current_language(language_code)

    def _animate_property(self, target: QWidget, prop: bytes, start: int, end: int) -> QPropertyAnimation:
        animation = QPropertyAnimation(target, prop, self)
        animation.setDuration(220)
        animation.setEasingCurve(QEasingCurve.InOutCubic)
        animation.setStartValue(start)
        animation.setEndValue(end)

        def finalize() -> None:
            if prop == b"maximumWidth":
                target.setMaximumWidth(end)
            elif prop == b"maximumHeight":
                target.setMaximumHeight(end)

        animation.finished.connect(finalize)
        animation.start()
        return animation

    def _toggle_sidebar(self) -> None:
        self._sidebar_collapsed = not self._sidebar_collapsed
        target_width = 64 if self._sidebar_collapsed else self._sidebar_expanded_width
        current_width = self.sidebar.width()
        self.sidebar.setCollapsed(self._sidebar_collapsed)
        self.sidebar.setMaximumWidth(max(current_width, target_width))
        self._sidebar_animation = self._animate_property(self.sidebar, b"maximumWidth", current_width, target_width)

    def _toggle_bottom_bar(self) -> None:
        self._bottom_collapsed = not self._bottom_collapsed
        target_height = 48 if self._bottom_collapsed else self._bottom_expanded_height
        current_height = self.bottom_bar.height()
        self.bottom_bar.setCollapsed(self._bottom_collapsed)
        self.bottom_bar.setMaximumHeight(max(current_height, target_height))
        self._bottom_animation = self._animate_property(self.bottom_bar, b"maximumHeight", current_height, target_height)

    def default_values(self) -> Dict[str, float]:
        return self.sidebar.default_values()
