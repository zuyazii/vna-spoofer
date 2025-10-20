from __future__ import annotations

import json
import sys
from pathlib import Path
from typing import Dict

from PySide6.QtCore import QLocale, QObject, Qt, Signal
from PySide6.QtGui import QColor
from PySide6.QtWidgets import QApplication, QMainWindow

from tokens import SERIES_NAMES, TOKENS
from views.mainview import MainView


class TranslationManager(QObject):
    languageChanged = Signal(str, dict)

    def __init__(self, directory: Path) -> None:
        super().__init__()
        self._directory = directory
        self._bundles: Dict[str, Dict] = {}
        self._current = ""
        self._load_bundles()

    def _load_bundles(self) -> None:
        for path in sorted(self._directory.glob("*.json")):
            language_code = path.stem
            try:
                with path.open("r", encoding="utf-8") as handle:
                    self._bundles[language_code] = json.load(handle)
            except (OSError, json.JSONDecodeError):
                continue
        if "en" not in self._bundles and self._bundles:
            self._current = next(iter(self._bundles))
        else:
            self._current = "en"

    def set_language(self, language_code: str) -> None:
        if language_code not in self._bundles:
            return
        if language_code == self._current:
            return
        self._current = language_code
        self.languageChanged.emit(language_code, self._bundles[language_code])

    def current_bundle(self) -> Dict:
        return self._bundles.get(self._current, {})

    def current_language(self) -> str:
        return self._current


class MainWindow(QMainWindow):
    def __init__(self) -> None:
        super().__init__()
        self._translation_manager = TranslationManager(Path(__file__).resolve().parent / "i18n")
        self._series_visibility = {(name, domain): True for name in SERIES_NAMES for domain in ("magnitude", "phase")}

        self._view = MainView(self)
        self.setCentralWidget(self._view)

        self._view.languageChanged.connect(self._translation_manager.set_language)
        self._view.seriesVisibilityChanged.connect(self._on_series_visibility)
        self._view.seriesColorChanged.connect(self._on_series_color_changed)
        self._view.startSweepRequested.connect(self._on_start_sweep)
        self._view.stopSweepRequested.connect(self._on_stop_sweep)
        self._view.resetRequested.connect(self._on_reset_requested)

        self._translation_manager.languageChanged.connect(self._apply_language)

        self._apply_language(
            self._translation_manager.current_language(), self._translation_manager.current_bundle()
        )
        self._view.set_language(self._translation_manager.current_language())

        defaults = self._view.default_values()
        self._last_range = (defaults["start"], defaults["end"])
        self._last_points = defaults["points"]

        self.resize(1320, 860)

    def _apply_language(self, language_code: str, bundle: Dict) -> None:
        if not bundle:
            return
        locale = self._locale_for(language_code)
        QLocale.setDefault(locale)

        self._view.set_locale(locale)
        self._view.apply_translations(bundle)
        self._view.set_language(language_code)

        app_title = bundle.get("app", {}).get("title", "VNA Spoofer")
        self.setWindowTitle(app_title)

    def _on_series_visibility(self, series_name: str, domain: str, visible: bool) -> None:
        key = (series_name.lower(), domain)
        self._series_visibility[key] = visible
        self._view.plot.set_series_visible(series_name, domain, visible)

    def _on_series_color_changed(self, series_name: str, domain: str, color: QColor) -> None:
        self._view.plot.set_series_color(series_name, domain, color)

    def _on_start_sweep(self) -> None:
        # Sweep integration will be added once the backend is wired up.
        pass

    def _on_stop_sweep(self) -> None:
        # Sweep integration will be added once the backend is wired up.
        pass

    def _on_reset_requested(self) -> None:
        # Reset integration will be added once the backend is wired up.
        pass

    def _locale_for(self, language_code: str) -> QLocale:
        if language_code == "zh_Hant":
            return QLocale(QLocale.TraditionalChinese, QLocale.Taiwan)
        if language_code == "zh_Hans":
            return QLocale(QLocale.Chinese, QLocale.China)
        return QLocale(QLocale.English, QLocale.UnitedStates)


def load_stylesheet() -> str:
    stylesheet_path = Path(__file__).resolve().parent / "theme.qss"
    with stylesheet_path.open("r", encoding="utf-8") as handle:
        template = handle.read()
    tokens = TOKENS.flatten_for_stylesheet()
    return template.format(**tokens)


def main() -> int:
    app = QApplication(sys.argv)
    app.setApplicationName("VNA CLI UI")
    app.setFont(TOKENS.qfont("sm"))

    stylesheet = load_stylesheet()
    app.setStyleSheet(stylesheet)

    window = MainWindow()
    window.show()

    return app.exec()


if __name__ == "__main__":
    sys.exit(main())
