"""
LibreVNA Calibration Module

This module provides calibration file parsing and correction algorithms
for LibreVNA Vector Network Analyzer measurements.

Main components:
- CalFile: Parse LibreVNA .cal JSON files
- CalPoint: Calibration data structures
- Corrector: Apply calibration corrections to measurements
- MathUtils: Mathematical utilities for calibration calculations
"""

from .cal_file import CalFile, CalPoint
from .structures import CalibrationData
from .corrector import Corrector
from .math_utils import MathUtils

__all__ = ['CalFile', 'CalPoint', 'CalibrationData', 'Corrector', 'MathUtils']
__version__ = '0.1.0'
