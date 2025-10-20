from __future__ import annotations

from dataclasses import dataclass
from typing import Dict, Iterable, Tuple

from PySide6.QtGui import QColor, QFont


@dataclass(frozen=True)
class FontScale:
    family: str
    size: Dict[str, int]
    weight: Dict[str, int]
    line_height: float


@dataclass(frozen=True)
class DesignTokens:
    color: Dict[str, str]
    radius: Dict[str, int]
    space: Tuple[int, ...]
    border_width: Dict[str, int]
    font: FontScale

    def flatten_for_stylesheet(self) -> Dict[str, str]:
        """
        Produce a flat mapping compatible with str.format placeholders in QSS templates.
        Converts token names like color.bg.canvas -> color_bg_canvas.
        """
        flat: Dict[str, str] = {}
        for group_name, values in (
            ("color", self.color),
            ("radius", self.radius),
            ("border_width", self.border_width),
        ):
            for key, value in values.items():
                placeholder = f"{group_name}_{key.replace('.', '_')}"
                flat[placeholder] = str(value)

        # Space tokens exposed as space_0, space_1, etc.
        for index, value in enumerate(self.space):
            flat[f"space_{index}"] = str(value)

        # Font sizes and weights flattened for convenience.
        for size_key, size_value in self.font.size.items():
            flat[f"font_size_{size_key}"] = str(size_value)
        for weight_key, weight_value in self.font.weight.items():
            flat[f"font_weight_{weight_key}"] = str(weight_value)
        flat["font_family"] = self.font.family
        flat["font_line_height"] = str(self.font.line_height)
        return flat

    def qcolor(self, token_name: str) -> QColor:
        """
        Resolve a QColor from the design tokens; falls back to black if missing.
        """
        hex_value = self.color.get(token_name, "#000000")
        return QColor(hex_value)

    def qfont(self, size_key: str = "sm", weight_key: str = "regular") -> QFont:
        """
        Build a QFont instance driven by the design tokens.
        """
        font = QFont(self.font.family)
        font.setPointSize(self.font.size.get(size_key, 11))
        weight_value = self.font.weight.get(weight_key, 400)
        font.setWeight(_normalize_weight(weight_value))
        return font


def _normalize_weight(weight: int) -> QFont.Weight:
    """
    Map numeric CSS-style weights into the closest Qt enum.
    """
    if weight <= 300:
        return QFont.Weight.Light
    if weight <= 400:
        return QFont.Weight.Normal
    if weight <= 500:
        return QFont.Weight.Medium
    if weight <= 600:
        return QFont.Weight.DemiBold
    if weight <= 700:
        return QFont.Weight.Bold
    if weight <= 800:
        return QFont.Weight.ExtraBold
    return QFont.Weight.Black


TOKENS = DesignTokens(
    color={
        "bg.canvas": "#E7E1D8",
        "bg.card": "#E7E1D8",
        "ink.primary": "#2B2B29",
        "ink.muted": "#6E665E",
        "stroke.soft": "#D0C9C2",
        "stroke.grid": "#D8D0C8",
        "focus": "#5A4FCF",
        "series.s11": "#E2554E",
        "series.s12": "#3A72E6",
        "series.s21": "#E5C148",
        "series.s22": "#6E5CE6",
        "state.hover": "#DCD4CB",
        "state.selection": "#E2DACE",
        "ink.inverse": "#FFFFFF",
    },
    radius={"xs": 4, "sm": 6, "md": 8, "lg": 12},
    space=(0, 4, 8, 12, 16, 24, 32),
    border_width={"hair": 1, "thin": 2},
    font=FontScale(
        family="IBM Plex Mono, 'JetBrains Mono', Consolas, 'Courier New', monospace",
        size={"xs": 10, "sm": 11, "md": 12, "lg": 14},
        weight={"regular": 400, "medium": 500, "bold": 600},
        line_height=1.25,
    ),
)


SERIES_NAMES: Tuple[str, ...] = ("s11", "s12", "s21", "s22")


def series_token(series_name: str) -> str:
    normalized = series_name.lower()
    if normalized not in SERIES_NAMES:
        raise KeyError(f"Unknown series token '{series_name}'")
    return f"series.{normalized}"


def iter_series_colors() -> Iterable[Tuple[str, QColor]]:
    for name in SERIES_NAMES:
        yield name, TOKENS.qcolor(f"series.{name}")


__all__ = [
    "DesignTokens",
    "TOKENS",
    "SERIES_NAMES",
    "series_token",
    "iter_series_colors",
]
