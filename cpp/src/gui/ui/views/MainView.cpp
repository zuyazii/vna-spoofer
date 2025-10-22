#include "MainView.hpp"

#include "../../ui/theme/DesignTokens.hpp"

#include <QBoxLayout>
#include <QHBoxLayout>
#include <QFile>
#include <QHash>
#include <QIcon>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSplitter>
#include <QToolButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QSize>
#include <QSizePolicy>
#include <algorithm>
#include <numeric>

namespace ui::views {

namespace {

const QStringList kSeriesNames = {
    QStringLiteral("S11"),
    QStringLiteral("S12"),
    QStringLiteral("S21"),
    QStringLiteral("S22")
};

QToolButton* makeGhostButton(const QString& text, QWidget* parent) {
    auto* button = new QToolButton(parent);
    button->setText(text);
    button->setCheckable(false);
    button->setAutoRaise(false);
    button->setMinimumSize(120, 32);
    button->setToolButtonStyle(Qt::ToolButtonTextOnly);
    button->setProperty("variant", "ghost");
    return button;
}

} // namespace

MainView::MainView(QWidget* parent)
    : QWidget(parent) {
    setObjectName(QStringLiteral("AppRoot"));
    setAttribute(Qt::WA_StyledBackground, true);

    m_baseStrings = loadStrings(QStringLiteral(":/ui/i18n/en.json"));

    buildUi();
    setupConnections();
    initSeriesState();
    onLanguageSelected(m_currentLocale);
}

void MainView::setSeriesColor(const QString& name, const QColor& color) {
    if (!color.isValid()) {
        return;
    }
    m_seriesColors.insert(name, color);
    updateColorsUi();
    m_plot->setSeriesColor(name, color);
}

void MainView::setSeriesVisible(const QString& name, bool visible) {
    m_seriesVisibility.insert(name, visible);
    updateVisibilityUi();
    m_plot->setSeriesVisible(name, visible);
}

void MainView::setShowMagnitude(bool on) {
    m_plot->setShowMagnitude(on);
    m_bottomPanel->setMagnitudeVisible(on);
}

void MainView::setShowPhase(bool on) {
    m_plot->setShowPhase(on);
    m_bottomPanel->setPhaseVisible(on);
}

void MainView::setFrequencyRange(double startGHz, double endGHz) {
    m_sidebar->setFrequencyRange(startGHz, endGHz);
}

void MainView::setPointCount(int points) {
    m_sidebar->setPointCount(points);
}

void MainView::setThreshold(const QString& name, double value) {
    m_sidebar->setThresholdValue(name, value);
}

void MainView::buildUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_mainSplitter = new QSplitter(Qt::Horizontal, this);
    m_mainSplitter->setChildrenCollapsible(false);
    m_mainSplitter->setObjectName(QStringLiteral("MainSplitter"));

    m_sidebarScroll = new QScrollArea(m_mainSplitter);
    m_sidebarScroll->setObjectName(QStringLiteral("SidebarScroll"));
    m_sidebarScroll->setFrameShape(QFrame::NoFrame);
    m_sidebarScroll->setWidgetResizable(true);
    m_sidebarScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_sidebarScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_sidebarScroll->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    m_sidebarScroll->setMinimumWidth(260);
    m_sidebarScroll->setMaximumWidth(400);

    m_sidebar = new Sidebar;
    m_sidebarScroll->setWidget(m_sidebar);

    auto* rightPane = new QWidget(m_mainSplitter);
    auto* rightPaneLayout = new QVBoxLayout(rightPane);
    rightPaneLayout->setContentsMargins(16, 16, 16, 16);
    rightPaneLayout->setSpacing(16);

    auto* plotSection = new QWidget(rightPane);
    auto* plotLayout = new QVBoxLayout(plotSection);
    plotLayout->setContentsMargins(0, 0, 0, 0);
    plotLayout->setSpacing(0);

    auto* plotTopRow = new QHBoxLayout;
    plotTopRow->setContentsMargins(0, 0, 0, 0);
    plotTopRow->setSpacing(8);

    m_sidebarToggle = new QToolButton(plotSection);
    m_sidebarToggle->setIcon(QIcon(QStringLiteral(":/ui/theme/icons/sidebar.svg")));
    m_sidebarToggle->setToolTip(tr("Toggle Sidebar"));
    m_sidebarToggle->setCheckable(false);
    m_sidebarToggle->setAutoRaise(false);
    m_sidebarToggle->setFixedSize(32, 32);
    m_sidebarToggle->setIconSize(QSize(18, 18));
    m_sidebarToggle->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_sidebarToggle->setProperty("variant", "ghost");
    plotTopRow->addWidget(m_sidebarToggle, 0, Qt::AlignLeft);

    plotTopRow->addStretch(1);

    m_presetsButton = makeGhostButton(tr("Presets"), plotSection);
    m_presetsButton->setIcon(QIcon(QStringLiteral(":/ui/theme/icons/presets.svg")));
    m_presetsButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_presetsButton->setIconSize(QSize(18, 18));
    m_presetsButton->setAccessibleName(tr("Presets"));
    plotTopRow->addWidget(m_presetsButton, 0, Qt::AlignRight);

    plotLayout->addLayout(plotTopRow);

    m_plot = new widgets::PlotView(plotSection);
    plotLayout->addWidget(m_plot, 1);

    rightPaneLayout->addWidget(plotSection, 1);

    m_bottomWrapper = new QWidget(rightPane);
    m_bottomWrapper->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    auto* bottomLayout = new QVBoxLayout(m_bottomWrapper);
    bottomLayout->setContentsMargins(0, 0, 0, 0);
    bottomLayout->setSpacing(12);

    auto* bottomHeader = new QHBoxLayout;
    bottomHeader->setContentsMargins(0, 0, 0, 0);
    bottomHeader->setSpacing(8);

    bottomHeader->addStretch(1);

    m_bottomToggle = new QToolButton(m_bottomWrapper);
    m_bottomToggle->setIcon(QIcon(QStringLiteral(":/ui/theme/icons/chevron_down.svg")));
    m_bottomToggle->setToolTip(tr("Toggle Bottom Panel"));
    m_bottomToggle->setFixedSize(32, 32);
    m_bottomToggle->setIconSize(QSize(14, 14));
    m_bottomToggle->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_bottomToggle->setProperty("variant", "ghost");
    bottomHeader->addWidget(m_bottomToggle, 0, Qt::AlignRight);

    bottomLayout->addLayout(bottomHeader);

    m_bottomPanel = new BottomPanel(m_bottomWrapper);
    bottomLayout->addWidget(m_bottomPanel);

    rightPaneLayout->addWidget(m_bottomWrapper, 0);

    m_mainSplitter->addWidget(m_sidebarScroll);
    m_mainSplitter->addWidget(rightPane);
    m_mainSplitter->setStretchFactor(0, 0);
    m_mainSplitter->setStretchFactor(1, 1);
    m_mainSplitter->setCollapsible(0, true);

    layout->addWidget(m_mainSplitter);
}

void MainView::setupConnections() {
    connect(m_sidebar, &Sidebar::startRequested, this, &MainView::startRequested);
    connect(m_sidebar, &Sidebar::stopRequested, this, &MainView::stopRequested);
    connect(m_sidebar, &Sidebar::resetRequested, this, &MainView::resetRequested);

    connect(m_sidebar, &Sidebar::rangeChanged, this, [this](double start, double end) {
        emit rangeChanged(start, end);
    });
    connect(m_sidebar, &Sidebar::pointsChanged, this, &MainView::pointsChanged);

    connect(m_sidebar, &Sidebar::parameterToggled, this, [this](const QString& name, bool on) {
        m_seriesVisibility.insert(name, on);
        updateVisibilityUi();
        m_plot->setSeriesVisible(name, on);
        emit seriesToggled(name, on);
    });

    connect(m_sidebar, &Sidebar::thresholdChanged, this, [this](const QString& name, double value) {
        emit thresholdChanged(name, value);
    });

    connect(m_plot, &widgets::PlotView::rangeChanged, this,
            [this](double start, double end) { emit rangeChanged(start, end); });

    connect(m_bottomPanel, &BottomPanel::languageChanged, this,
            [this](const QLocale& locale) { onLanguageSelected(locale); });

    connect(m_bottomPanel, &BottomPanel::seriesVisibilityChanged, this,
            [this](const QString& name, bool on) {
                m_seriesVisibility.insert(name, on);
                m_sidebar->setSeriesSelection(m_seriesVisibility);
                m_plot->setSeriesVisible(name, on);
                emit seriesToggled(name, on);
            });

    connect(m_bottomPanel, &BottomPanel::seriesColorChanged, this,
            [this](const QString& name, const QColor& color) {
                if (!color.isValid()) {
                    return;
                }
                m_seriesColors.insert(name, color);
                m_plot->setSeriesColor(name, color);
                emit seriesColorChanged(name, color);
            });

    connect(m_bottomPanel, &BottomPanel::magnitudeToggled, this, [this](bool on) {
        m_plot->setShowMagnitude(on);
        emit magnitudeShown(on);
    });

    connect(m_bottomPanel, &BottomPanel::phaseToggled, this, [this](bool on) {
        m_plot->setShowPhase(on);
        emit phaseShown(on);
    });

    connect(m_sidebarToggle, &QToolButton::clicked, this, &MainView::toggleSidebarVisibility);
    connect(m_bottomToggle, &QToolButton::clicked, this, &MainView::toggleBottomPanelVisibility);
    connect(m_presetsButton, &QToolButton::clicked, this, &MainView::presetsRequested);
}

void MainView::initSeriesState() {
    m_seriesVisibility.clear();
    m_seriesColors.clear();

    m_seriesColors.insert(QStringLiteral("S11"), QColor(QString::fromUtf8(theme::Tokens::S11)));
    m_seriesColors.insert(QStringLiteral("S12"), QColor(QString::fromUtf8(theme::Tokens::S12)));
    m_seriesColors.insert(QStringLiteral("S21"), QColor(QString::fromUtf8(theme::Tokens::S21)));
    m_seriesColors.insert(QStringLiteral("S22"), QColor(QString::fromUtf8(theme::Tokens::S22)));

    for (const QString& name : kSeriesNames) {
        m_seriesVisibility.insert(name, true);
        m_plot->setSeriesColor(name, m_seriesColors.value(name));
        m_plot->setSeriesVisible(name, true);
    }

    updateVisibilityUi();
    updateColorsUi();
}

void MainView::applyLocale(const QLocale& locale) {
    QLocale::setDefault(locale);
    m_sidebar->applyLocale(locale);
    m_bottomPanel->applyLocale(locale);
}

void MainView::applyTranslations(const QHash<QString, QString>& strings) {
    m_activeStrings = strings;
    m_sidebar->applyTranslations(m_activeStrings);
    m_bottomPanel->applyTranslations(m_activeStrings);
    m_plot->applyTranslations(m_activeStrings);

    const QString presetsText =
        m_activeStrings.value(QStringLiteral("plot.presets"), tr("Presets"));
    m_presetsButton->setText(presetsText);
    m_presetsButton->setAccessibleName(presetsText);

    m_sidebarToggle->setToolTip(
        m_activeStrings.value(QStringLiteral("panel.toggleSidebar"), tr("Toggle Sidebar")));
    m_bottomToggle->setToolTip(
        m_activeStrings.value(QStringLiteral("panel.toggleBottom"), tr("Toggle Bottom Panel")));
}

void MainView::updateVisibilityUi() {
    m_bottomPanel->setSeriesState(m_seriesVisibility);
    m_sidebar->setSeriesSelection(m_seriesVisibility);
}

void MainView::updateColorsUi() {
    m_bottomPanel->setSeriesColors(m_seriesColors);
}

void MainView::toggleSidebarVisibility() {
    if (!m_mainSplitter) {
        return;
    }
    const QList<int> sizes = m_mainSplitter->sizes();
    if (!m_sidebarCollapsed) {
        if (sizes.size() >= 2) {
            m_lastSidebarWidth = std::max(sizes.at(0), m_sidebarScroll ? m_sidebarScroll->sizeHint().width() : 1);
        }
        QList<int> newSizes{0};
        if (sizes.size() >= 2) {
            newSizes.append(sizes.at(0) + sizes.at(1));
        } else {
            newSizes.append(1);
        }
        m_mainSplitter->setSizes(newSizes);
        if (m_sidebarScroll) {
            m_sidebarScroll->setVisible(false);
            m_sidebarScroll->setMinimumWidth(0);
        }
        m_sidebarCollapsed = true;
        m_sidebarToggle->setIcon(QIcon(QStringLiteral(":/ui/theme/icons/chevron_right.svg")));
    } else {
        int total = std::accumulate(sizes.begin(), sizes.end(), 0);
        if (total <= 0) {
            total = m_mainSplitter->width();
        }
        if (total <= 0) {
            total = 800;
        }
        const int minSidebar = 240;
        const int maxSidebar = std::max(minSidebar, total - 200);
        const int sidebarWidth = std::clamp(m_lastSidebarWidth, minSidebar, maxSidebar);
        const int plotWidth = std::max(total - sidebarWidth, 200);
        const int preferred = std::clamp(sidebarWidth, 260, 360);
        QList<int> restored{preferred, std::max(total - preferred, 200)};
        m_mainSplitter->setSizes(restored);
        if (m_sidebarScroll) {
            m_sidebarScroll->setMinimumWidth(std::max(preferred, 260));
        }
        if (m_sidebarScroll) {
            m_sidebarScroll->setVisible(true);
        }
        m_sidebarCollapsed = false;
        m_sidebarToggle->setIcon(QIcon(QStringLiteral(":/ui/theme/icons/sidebar.svg")));
    }
}

void MainView::toggleBottomPanelVisibility() {
    if (!m_bottomPanel) {
        return;
    }
    m_bottomCollapsed = !m_bottomCollapsed;
    if (m_bottomPanel) {
        m_bottomPanel->setVisible(!m_bottomCollapsed);
    }
    m_bottomToggle->setIcon(QIcon(m_bottomCollapsed ? QStringLiteral(":/ui/theme/icons/chevron_up.svg")
                                                    : QStringLiteral(":/ui/theme/icons/chevron_down.svg")));
}

QHash<QString, QString> MainView::loadStrings(const QString& path) const {
    QHash<QString, QString> map;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return map;
    }
    const auto document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
        return map;
    }
    const auto object = document.object();
    for (auto it = object.begin(); it != object.end(); ++it) {
        if (it.value().isString()) {
            map.insert(it.key(), it.value().toString());
        }
    }
    return map;
}

QString MainView::resolveLocaleKey(const QLocale& locale) const {
    const QString name = locale.name();
    if (name.startsWith(QStringLiteral("zh_Hant"))) {
        return QStringLiteral("zh_Hant");
    }
    if (name.startsWith(QStringLiteral("zh_Hans")) || locale.language() == QLocale::Chinese) {
        return QStringLiteral("zh_Hans");
    }
    return QStringLiteral("en");
}

void MainView::onLanguageSelected(const QLocale& locale) {
    m_currentLocale = locale;
    applyLocale(locale);

    QHash<QString, QString> strings = m_baseStrings;
    const QString key = resolveLocaleKey(locale);
    if (key != QStringLiteral("en")) {
        const QString overridePath =
            QStringLiteral(":/ui/i18n/%1.json").arg(key);
        const QHash<QString, QString> overrideMap = loadStrings(overridePath);
        for (auto it = overrideMap.constBegin(); it != overrideMap.constEnd(); ++it) {
            strings.insert(it.key(), it.value());
        }
    }

    applyTranslations(strings);
    emit languageChanged(locale);
}

} // namespace ui::views




