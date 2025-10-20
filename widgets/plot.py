from __future__ import annotations

import math
from typing import Dict, List, Tuple

from PySide6.QtCore import QPoint, QPointF, QRect, QRectF, Qt, QLocale, QSize
from PySide6.QtGui import QColor, QMouseEvent, QPainter, QPainterPath, QPen, QWheelEvent
from PySide6.QtWidgets import QFrame, QRubberBand

from tokens import TOKENS, SERIES_NAMES, iter_series_colors


class PlotWidget(QFrame):
    """
    Custom interactive plot supporting pan/zoom and dual magnitude/phase rendering.
    """

    def __init__(self, parent=None) -> None:
        super().__init__(parent)
        self.setAttribute(Qt.WA_OpaquePaintEvent, True)
        self.setMouseTracking(True)
        self.setFocusPolicy(Qt.StrongFocus)
        self.setMinimumSize(640, 360)

        self._locale = QLocale(QLocale.English)
        self._axis_labels = {
            "magnitude": "Magnitude (dB)",
            "phase": "Phase (deg)",
            "frequency": "Frequency (GHz)",
        }

        self._x_range = [1.0, 6.0]
        self._y_ranges: Dict[str, List[float]] = {
            "magnitude": [-40.0, 5.0],
            "phase": [-180.0, 180.0],
        }
        self._series_colors: Dict[Tuple[str, str], QColor] = {}
        for domain in ("magnitude", "phase"):
            for name, color in iter_series_colors():
                adjusted = QColor(color)
                if domain == "phase":
                    adjusted = adjusted.darker(120)
                self._series_colors[(name, domain)] = adjusted

        self._series_visible: Dict[Tuple[str, str], bool] = {
            (name, domain): True for name in SERIES_NAMES for domain in ("magnitude", "phase")
        }

        self._data: Dict[Tuple[str, str], List[QPointF]] = self._generate_series_data()

        self._panning = False
        self._pan_start: QPoint | None = None
        self._initial_x_range: Tuple[float, float] | None = None
        self._initial_y_ranges: Dict[str, Tuple[float, float]] | None = None
        self._rubber_band: QRubberBand | None = None
        self._rubber_origin: QPoint | None = None

    # --- Public API -----------------------------------------------------
    def set_series_color(self, series_name: str, domain: str, color: QColor) -> None:
        key = (series_name.lower(), domain)
        if key in self._series_colors and color.isValid():
            self._series_colors[key] = QColor(color)
            self.update()

    def set_series_visible(self, series_name: str, domain: str, visible: bool) -> None:
        key = (series_name.lower(), domain)
        if key in self._series_visible:
            self._series_visible[key] = visible
            self.update()

    def set_axis_labels(self, magnitude: str, phase: str, frequency: str) -> None:
        self._axis_labels["magnitude"] = magnitude
        self._axis_labels["phase"] = phase
        self._axis_labels["frequency"] = frequency
        self.update()

    def apply_locale(self, locale: QLocale) -> None:
        self._locale = locale
        self.update()

    # --- Painting -------------------------------------------------------
    def paintEvent(self, event) -> None:  # noqa: D401
        painter = QPainter(self)
        painter.setRenderHint(QPainter.Antialiasing, True)
        painter.fillRect(self.rect(), TOKENS.qcolor("bg.canvas"))

        plot_rect = self._plot_rect()
        painter.fillRect(plot_rect, TOKENS.qcolor("bg.card"))
        self._draw_grid(painter, plot_rect)
        self._draw_axes(painter, plot_rect)
        self._draw_series(painter, plot_rect, "magnitude")
        self._draw_series(painter, plot_rect, "phase")

    def _plot_rect(self) -> QRectF:
        left = 80
        right = 96
        top = 32
        bottom = 72
        rect = QRectF(self.rect())
        rect.adjust(left, top, -right, -bottom)
        return rect

    def _draw_grid(self, painter: QPainter, rect: QRectF) -> None:
        pen = QPen(TOKENS.qcolor("stroke.grid"))
        pen.setWidthF(1.0)
        painter.setPen(pen)

        x_min, x_max = self._x_range
        for step in range(6):
            value = x_min + (x_max - x_min) * step / 5
            x = self._map_x(rect, value)
            painter.drawLine(x, rect.top(), x, rect.bottom())

        y_min, y_max = self._y_ranges["magnitude"]
        for step in range(6):
            value = y_min + (y_max - y_min) * step / 5
            y = self._map_y(rect, value, "magnitude")
            painter.drawLine(rect.left(), y, rect.right(), y)

    def _draw_axes(self, painter: QPainter, rect: QRectF) -> None:
        pen = QPen(TOKENS.qcolor("stroke.soft"))
        pen.setWidthF(1.0)
        painter.setPen(pen)
        painter.drawRect(rect)

        tick_font = TOKENS.qfont("xs", "regular")
        label_font = TOKENS.qfont("sm", "medium")

        painter.setFont(tick_font)
        painter.setPen(TOKENS.qcolor("ink.muted"))

        x_min, x_max = self._x_range
        for step in range(6):
            value = x_min + (x_max - x_min) * step / 5
            x = self._map_x(rect, value)
            painter.drawLine(x, rect.bottom(), x, rect.bottom() + 6)
            text = self._locale.toString(value, "f", 3)
            painter.drawText(QRectF(x - 40, rect.bottom() + 16, 80, 16), Qt.AlignHCenter | Qt.AlignTop, text)

        y_min, y_max = self._y_ranges["magnitude"]
        for step in range(6):
            value = y_min + (y_max - y_min) * step / 5
            y = self._map_y(rect, value, "magnitude")
            painter.drawLine(rect.left() - 6, y, rect.left(), y)
            text = self._locale.toString(value, "f", 0)
            painter.drawText(QRectF(rect.left() - 62, y - 8, 54, 16), Qt.AlignRight | Qt.AlignVCenter, text)

        painter.setFont(label_font)
        painter.setPen(TOKENS.qcolor("ink.primary"))

        painter.save()
        painter.translate(rect.left() - 70, rect.center().y())
        painter.rotate(-90)
        painter.drawText(QRectF(-rect.height() / 2, -32, rect.height(), 32), Qt.AlignCenter, self._axis_labels["magnitude"])
        painter.restore()

        painter.drawText(QRectF(rect.left(), rect.bottom() + 30, rect.width(), 24), Qt.AlignCenter, self._axis_labels["frequency"])

        painter.setFont(tick_font)
        painter.setPen(TOKENS.qcolor("ink.muted"))
        y_min, y_max = self._y_ranges["phase"]
        for step in range(6):
            value = y_min + (y_max - y_min) * step / 5
            y = self._map_y(rect, value, "phase")
            painter.drawLine(rect.right(), y, rect.right() + 6, y)
            text = self._locale.toString(value, "f", 0)
            painter.drawText(QRectF(rect.right() + 8, y - 8, 56, 16), Qt.AlignLeft | Qt.AlignVCenter, text)

        painter.setFont(label_font)
        painter.setPen(TOKENS.qcolor("ink.primary"))
        painter.save()
        painter.translate(rect.right() + 70, rect.center().y())
        painter.rotate(90)
        painter.drawText(QRectF(-rect.height() / 2, -32, rect.height(), 32), Qt.AlignCenter, self._axis_labels["phase"])
        painter.restore()

    def _draw_series(self, painter: QPainter, rect: QRectF, domain: str) -> None:
        for name in SERIES_NAMES:
            key = (name, domain)
            if not self._series_visible.get(key, False):
                continue
            color = self._series_colors.get(key, TOKENS.qcolor("ink.muted"))
            pen = QPen(color)
            pen.setWidthF(1.5)
            painter.setPen(pen)

            data = self._data.get(key, [])
            if not data:
                continue

            path = QPainterPath()
            first = True
            for point in data:
                x = self._map_x(rect, point.x())
                y = self._map_y(rect, point.y(), domain)
                if first:
                    path.moveTo(x, y)
                    first = False
                else:
                    path.lineTo(x, y)
            painter.drawPath(path)

    # --- Interaction ----------------------------------------------------
    def wheelEvent(self, event: QWheelEvent) -> None:
        delta = event.angleDelta().y()
        if delta == 0:
            return
        factor = 0.85 if delta > 0 else 1.15
        if event.modifiers() & Qt.ControlModifier:
            self._zoom_x(factor, event.position().x())
        elif event.modifiers() & Qt.ShiftModifier:
            self._zoom_y(factor)
        else:
            self._zoom_x(factor, event.position().x())
        event.accept()

    def mousePressEvent(self, event: QMouseEvent) -> None:
        if event.button() == Qt.LeftButton:
            self._panning = True
            self._pan_start = event.position().toPoint()
            self._initial_x_range = tuple(self._x_range)
            self._initial_y_ranges = {domain: tuple(values) for domain, values in self._y_ranges.items()}
            self.setCursor(Qt.ClosedHandCursor)
        elif event.button() == Qt.RightButton:
            self._rubber_origin = event.position().toPoint()
            self._rubber_band = QRubberBand(QRubberBand.Rectangle, self)
            self._rubber_band.setGeometry(QRect(self._rubber_origin, QSize(1, 1)))
            self._rubber_band.show()
        super().mousePressEvent(event)

    def mouseMoveEvent(self, event: QMouseEvent) -> None:
        if self._panning and self._pan_start and self._initial_x_range and self._initial_y_ranges:
            rect = self._plot_rect()
            delta_x = event.position().x() - self._pan_start.x()
            delta_y = event.position().y() - self._pan_start.y()

            if rect.width() > 0:
                span_x = self._initial_x_range[1] - self._initial_x_range[0]
                shift_x = (delta_x / rect.width()) * span_x
                self._x_range[0] = self._initial_x_range[0] - shift_x
                self._x_range[1] = self._initial_x_range[1] - shift_x

            if rect.height() > 0:
                ratio_y = delta_y / rect.height()
                for domain, (min_init, max_init) in self._initial_y_ranges.items():
                    span = max_init - min_init
                    shift_y = ratio_y * span
                    self._y_ranges[domain][0] = min_init + shift_y
                    self._y_ranges[domain][1] = max_init + shift_y
            self.update()
        elif self._rubber_band and self._rubber_origin:
            rect = QRect(self._rubber_origin, event.position().toPoint()).normalized()
            self._rubber_band.setGeometry(rect)
        super().mouseMoveEvent(event)

    def mouseReleaseEvent(self, event: QMouseEvent) -> None:
        if event.button() == Qt.LeftButton and self._panning:
            self._panning = False
            self._pan_start = None
            self._initial_x_range = None
            self._initial_y_ranges = None
            self.unsetCursor()
        elif event.button() == Qt.RightButton and self._rubber_band:
            rect = self._rubber_band.geometry().intersected(self._plot_rect().toRect())
            self._rubber_band.hide()
            self._rubber_band.deleteLater()
            self._rubber_band = None
            self._rubber_origin = None
            if rect.width() > 10 and rect.height() > 10:
                self._apply_zoom_rect(rect)
        super().mouseReleaseEvent(event)

    # --- Helpers --------------------------------------------------------
    def _zoom_x(self, factor: float, position_x: float) -> None:
        rect = self._plot_rect()
        if rect.width() <= 0:
            return
        cursor_ratio = (position_x - rect.left()) / rect.width()
        cursor_ratio = min(max(cursor_ratio, 0.0), 1.0)

        x_min, x_max = self._x_range
        center = x_min + (x_max - x_min) * cursor_ratio
        new_min = center - (center - x_min) * factor
        new_max = center + (x_max - center) * factor

        self._x_range[0], self._x_range[1] = new_min, new_max
        self.update()

    def _zoom_y(self, factor: float) -> None:
        for domain in self._y_ranges:
            y_min, y_max = self._y_ranges[domain]
            center = (y_min + y_max) / 2
            self._y_ranges[domain][0] = center - (center - y_min) * factor
            self._y_ranges[domain][1] = center + (y_max - center) * factor
        self.update()

    def _apply_zoom_rect(self, rect: QRect) -> None:
        plot_rect = self._plot_rect()
        if plot_rect.width() <= 0 or plot_rect.height() <= 0:
            return

        left_ratio = (rect.left() - plot_rect.left()) / plot_rect.width()
        right_ratio = (rect.right() - plot_rect.left()) / plot_rect.width()
        top_ratio = (rect.top() - plot_rect.top()) / plot_rect.height()
        bottom_ratio = (rect.bottom() - plot_rect.top()) / plot_rect.height()

        x_min, x_max = self._x_range
        span_x = x_max - x_min
        self._x_range[0] = x_min + span_x * left_ratio
        self._x_range[1] = x_min + span_x * right_ratio

        for domain in self._y_ranges:
            y_min, y_max = self._y_ranges[domain]
            span_y = y_max - y_min
            self._y_ranges[domain][0] = y_min + span_y * (1 - bottom_ratio)
            self._y_ranges[domain][1] = y_min + span_y * (1 - top_ratio)
        self.update()

    def _map_x(self, rect: QRectF, value: float) -> float:
        x_min, x_max = self._x_range
        span = max(x_max - x_min, 1e-6)
        return rect.left() + (value - x_min) / span * rect.width()

    def _map_y(self, rect: QRectF, value: float, domain: str) -> float:
        y_min, y_max = self._y_ranges[domain]
        span = max(y_max - y_min, 1e-6)
        return rect.bottom() - (value - y_min) / span * rect.height()

    def _generate_series_data(self) -> Dict[Tuple[str, str], List[QPointF]]:
        data: Dict[Tuple[str, str], List[QPointF]] = {}
        x_min, x_max = self._x_range
        span = x_max - x_min
        for index, name in enumerate(SERIES_NAMES):
            magnitude_points: List[QPointF] = []
            phase_points: List[QPointF] = []
            offset = index * 6.0
            for step in range(201):
                freq = x_min + span * step / 200.0
                mag_value = -20 + math.sin((freq - x_min) * math.pi * (1.0 + index * 0.35)) * 5 - offset
                phase_value = math.cos((freq - x_min) * math.pi * (1.0 + index * 0.2)) * 120 - index * 10
                magnitude_points.append(QPointF(freq, mag_value))
                phase_points.append(QPointF(freq, phase_value))
            data[(name, "magnitude")] = magnitude_points
            data[(name, "phase")] = phase_points
        return data

