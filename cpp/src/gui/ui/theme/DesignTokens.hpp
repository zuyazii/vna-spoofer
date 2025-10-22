// Copyright (c) 2025 vna-spoofer
// SPDX-License-Identifier: MIT

#pragma once

#include <QString>

namespace ui::theme::Tokens {

// Colors
inline constexpr auto BG_Canvas = "#E7E1D8";
inline constexpr auto BG_Card = "#E7E1D8";
inline constexpr auto InkPrimary = "#2B2B29";
inline constexpr auto InkMuted = "#6E665E";
inline constexpr auto StrokeSoft = "#D0C9C2";
inline constexpr auto StrokeGrid = "#D8D0C8";
inline constexpr auto Focus = "#5A4FCF";

// Series colors
inline constexpr auto S11 = "#E2554E";
inline constexpr auto S12 = "#3A72E6";
inline constexpr auto S21 = "#E5C148";
inline constexpr auto S22 = "#6E5CE6";

enum class Radius {
    XS = 4,
    SM = 6,
    MD = 8,
    LG = 12
};

enum class Space {
    S0 = 0,
    S1 = 4,
    S2 = 8,
    S3 = 12,
    S4 = 16,
    S5 = 24,
    S6 = 32
};

namespace Font {
inline constexpr int XS = 10;
inline constexpr int SM = 11;
inline constexpr int MD = 12;
inline constexpr int LG = 14;
inline constexpr auto Family =
    "Anonymous Pro, JetBrains Mono, IBM Plex Mono, Consolas, 'Courier New', monospace";
} // namespace Font

inline int space(Space value) {
    return static_cast<int>(value);
}

inline int radius(Radius value) {
    return static_cast<int>(value);
}

inline QString fontFamily() {
    return QString::fromUtf8(Font::Family);
}

} // namespace ui::theme::Tokens
