// Copyright (c) 2025 vna-spoofer
// SPDX-License-Identifier: MIT

#pragma once

#include <QApplication>
#include <QString>

namespace ui::theme {

class ThemeLoader {
public:
    static void apply(QApplication& app);
    static QString stylesheet();

private:
    static void ensureFontsRegistered();
    static QString loadTemplate();
    static QString applyTokens(const QString& tmpl);
};

} // namespace ui::theme
