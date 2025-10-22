#include "ThemeLoader.hpp"

#include "DesignTokens.hpp"

#include <QColor>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QMap>
#include <QTextStream>
#include <QStringConverter>
#include <QStringList>
#include <algorithm>

namespace ui::theme {

namespace {

QString readResourceFile(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    return stream.readAll();
}

QColor adjustLuminance(const QColor& color, double factor) {
    QColor out = color;
    int r = color.red();
    int g = color.green();
    int b = color.blue();

    auto mix = [factor](int channel) -> int {
        const double value = channel * factor;
        return std::clamp(static_cast<int>(value), 0, 255);
    };

    out.setRed(mix(r));
    out.setGreen(mix(g));
    out.setBlue(mix(b));
    return out;
}

QString hex(const QColor& color) {
    return color.name(QColor::HexRgb);
}

} // namespace

void ThemeLoader::apply(QApplication& app) {
    ensureFontsRegistered();
    QFont appFont(QStringLiteral("Anonymous Pro"));
    if (appFont.family().compare(QStringLiteral("Anonymous Pro"), Qt::CaseInsensitive) != 0) {
        appFont = app.font();
        appFont.setFamily(QString::fromUtf8(Tokens::Font::Family).split(',').first().trimmed());
    }
    appFont.setPointSize(Tokens::Font::MD);
    app.setFont(appFont);

    app.setStyleSheet(stylesheet());
}

QString ThemeLoader::stylesheet() {
    static QString cached;
    if (cached.isEmpty()) {
        const QString tmpl = loadTemplate();
        cached = applyTokens(tmpl);
    }
    return cached;
}

void ThemeLoader::ensureFontsRegistered() {
    static bool registered = false;
    if (registered) {
        return;
    }

    const QString fontPath = QStringLiteral(":/ui/theme/fonts/AnonymousPro-Regular.ttf");
    if (QFontDatabase::addApplicationFont(fontPath) == -1) {
        // Continue even if the font fails to load; Qt will fall back automatically.
    }
    registered = true;
}

QString ThemeLoader::loadTemplate() {
    return readResourceFile(QStringLiteral(":/ui/theme/theme.qss"));
}

QString ThemeLoader::applyTokens(const QString& tmpl) {
    using namespace Tokens;

    const QColor bgCard(QString::fromUtf8(BG_Card));
    const QColor hover = adjustLuminance(bgCard, 0.96); // ~4% darker

    const QMap<QString, QString> replacements{
        {QStringLiteral("{{FontFamily}}"), QString::fromUtf8(Font::Family)},
        {QStringLiteral("{{InkPrimary}}"), QString::fromUtf8(InkPrimary)},
        {QStringLiteral("{{InkMuted}}"), QString::fromUtf8(InkMuted)},
        {QStringLiteral("{{BG_Canvas}}"), QString::fromUtf8(BG_Canvas)},
        {QStringLiteral("{{BG_Card}}"), QString::fromUtf8(BG_Card)},
        {QStringLiteral("{{StrokeSoft}}"), QString::fromUtf8(StrokeSoft)},
        {QStringLiteral("{{StrokeGrid}}"), QString::fromUtf8(StrokeGrid)},
        {QStringLiteral("{{Focus}}"), QString::fromUtf8(Focus)},
        {QStringLiteral("{{Hover}}"), hex(hover)},
        {QStringLiteral("{{RadiusXS}}"), QString::number(radius(Radius::XS))},
        {QStringLiteral("{{RadiusSM}}"), QString::number(radius(Radius::SM))},
        {QStringLiteral("{{RadiusMD}}"), QString::number(radius(Radius::MD))},
        {QStringLiteral("{{RadiusLG}}"), QString::number(radius(Radius::LG))}
    };

    QString out = tmpl;
    for (auto it = replacements.cbegin(); it != replacements.cend(); ++it) {
        out.replace(it.key(), it.value());
    }
    return out;
}

} // namespace ui::theme
