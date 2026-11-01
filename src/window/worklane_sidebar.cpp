#include "window/worklane_sidebar.h"
#include "window/main_window.h"
#include "terminal/worklane_view.h"
#include "terminal/terminal_surface.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QToolButton>
#include <QLabel>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QPainterPath>
#include <QDir>
#include <QIcon>
#include <QScrollBar>
#include <QFontDatabase>
#include <KLocalizedString>
#include <KAcceleratorManager>
#include <KColorScheme>

Q_DECLARE_METATYPE(QPointer<TerminalSurface>)

class WorklaneItemDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        int itemType = index.data(Qt::UserRole).toInt();
        int fontH = option.fontMetrics.height();
        if (itemType == 1) { // Clean section header with top spacing
            return QSize(option.rect.width(), std::max(28, fontH + 14));
        } else if (itemType == 2) { // Lane card row
            return QSize(option.rect.width(), std::max(46, fontH * 2 + 14));
        }
        return QStyledItemDelegate::sizeHint(option, index);
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);

        int itemType = index.data(Qt::UserRole).toInt();
        QRect rect = option.rect;

        KColorScheme viewScheme(QPalette::Active, KColorScheme::View);
        KColorScheme windowScheme(QPalette::Active, KColorScheme::Window);

        if (itemType == 1) {
            // Clean unboxed section header (matching mockup with extra top margin)
            QString tabName = index.data(Qt::DisplayRole).toString();
            int paneCount = index.data(Qt::UserRole + 1).toInt();
            bool isCurrentTab = index.data(Qt::UserRole + 2).toBool();

            // 12px top offset gives distinct visual breathing room between tab groups
            QRect headerRect = rect.adjusted(10, 8, -10, 0);

            // Title text (unboxed, clean typography)
            QFont font = option.font;
            font.setBold(true);
            font.setPointSize(std::max(8, font.pointSize() - 1));
            painter->setFont(font);

            QColor textColor = isCurrentTab ? option.palette.color(QPalette::Highlight)
                                            : viewScheme.foreground(KColorScheme::InactiveText).color();
            painter->setPen(textColor);

            int badgeWidth = option.fontMetrics.horizontalAdvance(i18n("%1 panes", paneCount)) + 16;
            QRect textRect = headerRect.adjusted(0, 0, -badgeWidth - 4, 0);

            QString elidedTabName = option.fontMetrics.elidedText(tabName.toUpper(), Qt::ElideRight, textRect.width());
            painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, elidedTabName);

            // Subtle count badge pill on the right
            QString badge = i18n("%1 panes", paneCount);
            QFont badgeFont = option.font;
            badgeFont.setBold(false);
            badgeFont.setPointSize(std::max(7, badgeFont.pointSize() - 2));
            painter->setFont(badgeFont);

            int pillW = painter->fontMetrics().horizontalAdvance(badge) + 10;
            int pillH = std::max(15, painter->fontMetrics().height() + 2);
            QRect pillRect(headerRect.right() - pillW, headerRect.top() + (headerRect.height() - pillH) / 2, pillW, pillH);

            QColor pillBg = isCurrentTab ? option.palette.color(QPalette::Highlight)
                                        : option.palette.color(QPalette::Mid);
            pillBg.setAlpha(isCurrentTab ? 35 : 40);
            painter->setPen(Qt::NoPen);
            painter->setBrush(pillBg);
            painter->drawRoundedRect(pillRect, 3, 3);

            painter->setPen(isCurrentTab ? option.palette.color(QPalette::Highlight)
                                         : viewScheme.foreground(KColorScheme::InactiveText).color());
            painter->drawText(pillRect, Qt::AlignCenter, badge);

        } else if (itemType == 2) {
            // Lane Card
            QString title = index.data(Qt::DisplayRole).toString();
            QString pwd = index.data(Qt::UserRole + 1).toString();
            QString colTag = index.data(Qt::UserRole + 2).toString();
            bool isActive = index.data(Qt::UserRole + 3).toBool();
            bool hasAlert = index.data(Qt::UserRole + 4).toBool();

            QRect cardRect = rect.adjusted(8, 2, -8, -2);

            QColor cardBg = option.palette.color(QPalette::Window);
            QColor borderCol = option.palette.color(QPalette::Mid);

            if (isActive) {
                // Active pane gets elevated background with crisp contrast
                cardBg = option.palette.color(QPalette::Highlight);
                borderCol = option.palette.color(QPalette::Highlight);
            } else if (hasAlert) {
                cardBg = QColor(235, 160, 20, 35);
                borderCol = QColor(235, 160, 20, 200);
            } else if (option.state & QStyle::State_MouseOver) {
                cardBg = option.palette.color(QPalette::AlternateBase);
                borderCol = option.palette.color(QPalette::Highlight).lighter(120);
            }

            painter->setPen(QPen(borderCol, 1));
            painter->setBrush(cardBg);
            painter->drawRoundedRect(cardRect, 5, 5);

            // Status Indicator Dot
            int dotR = 3;
            int dotX = cardRect.left() + 10;
            int dotY = cardRect.top() + (cardRect.height() / 2) - dotR;
            painter->setPen(Qt::NoPen);
            if (hasAlert) {
                painter->setBrush(QColor(235, 160, 20)); // Amber alert
            } else if (isActive) {
                painter->setBrush(option.palette.color(QPalette::HighlightedText));
            } else {
                painter->setBrush(viewScheme.foreground(KColorScheme::InactiveText).color());
            }
            painter->drawEllipse(QPoint(dotX + dotR, dotY + dotR), dotR, dotR);

            int leftContentX = dotX + dotR * 2 + 8;

            // Column Position Tag (Col 1/3)
            int tagPillWidth = 0;
            QFont monoFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
            monoFont.setPointSize(std::max(8, option.font.pointSize() - 1));

            if (!colTag.isEmpty()) {
                QFont tagFont = option.font;
                tagFont.setPointSize(std::max(7, tagFont.pointSize() - 2));
                QFontMetrics tagFm(tagFont);
                tagPillWidth = tagFm.horizontalAdvance(colTag) + 10;
                int tagPillH = std::max(15, tagFm.height() + 2);

                QRect tagPillRect(cardRect.right() - tagPillWidth - 6, cardRect.top() + 5, tagPillWidth, tagPillH);
                QColor tagBg = isActive ? option.palette.color(QPalette::Highlight).darker(115)
                                        : option.palette.color(QPalette::AlternateBase);
                painter->setPen(QPen(isActive ? option.palette.color(QPalette::HighlightedText) : option.palette.color(QPalette::Mid), 1));
                painter->setBrush(tagBg);
                painter->drawRoundedRect(tagPillRect, 3, 3);

                painter->setFont(tagFont);
                painter->setPen(isActive ? option.palette.color(QPalette::HighlightedText)
                                         : viewScheme.foreground(KColorScheme::NormalText).color());
                painter->drawText(tagPillRect, Qt::AlignCenter, colTag);
            }

            // Line 1: Title (High contrast: HighlightedText when active, NormalText when inactive)
            int lineH = std::max(15, option.fontMetrics.height());
            QRect titleRect(leftContentX, cardRect.top() + 4, cardRect.width() - (leftContentX - cardRect.left()) - tagPillWidth - 8, lineH);
            QFont titleFont = option.font;
            titleFont.setBold(isActive);
            painter->setFont(titleFont);
            painter->setPen(isActive ? option.palette.color(QPalette::HighlightedText)
                                     : viewScheme.foreground(KColorScheme::NormalText).color());
            QString elidedTitle = option.fontMetrics.elidedText(title, Qt::ElideRight, titleRect.width());
            painter->drawText(titleRect, Qt::AlignVCenter | Qt::AlignLeft, elidedTitle);

            // Line 2: PWD in System Monospace Font (High contrast: crisp white when active, InactiveText when inactive)
            QRect pwdRect(leftContentX, cardRect.top() + lineH + 4, cardRect.width() - (leftContentX - cardRect.left()) - 6, lineH);
            painter->setFont(monoFont);
            QColor pwdCol = isActive ? QColor(255, 255, 255, 215)
                                     : viewScheme.foreground(KColorScheme::InactiveText).color();
            painter->setPen(pwdCol);
            QFontMetrics monoFm(monoFont);
            QString elidedPwd = monoFm.elidedText(pwd, Qt::ElideRight, pwdRect.width());
            painter->drawText(pwdRect, Qt::AlignVCenter | Qt::AlignLeft, elidedPwd);

        } else {
            QStyledItemDelegate::paint(painter, option, index);
        }

        painter->restore();
    }
};

WorklaneSidebar::WorklaneSidebar(MainWindow* mainWindow, QWidget* parent)
    : QWidget(parent)
    , m_mainWindow(mainWindow)
{
    setMinimumWidth(200);
    setMaximumWidth(450);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // Header Toolbar Area with Clean KDE Layout
    auto* headerBar = new QWidget(this);
    headerBar->setFixedHeight(36);
    auto* headerLayout = new QHBoxLayout(headerBar);
    headerLayout->setContentsMargins(12, 4, 8, 4);
    headerLayout->setSpacing(8);

    auto* headerLabel = new QLabel(i18n("Worklanes"), headerBar);
    QFont labelFont = headerLabel->font();
    labelFont.setBold(true);
    headerLabel->setFont(labelFont);
    headerLayout->addWidget(headerLabel);

    headerLayout->addStretch(1);

    m_newWorklaneBtn = new QToolButton(headerBar);
    m_newWorklaneBtn->setFocusPolicy(Qt::NoFocus);
    m_newWorklaneBtn->setIcon(QIcon::fromTheme(QStringLiteral("tab-new"), QIcon::fromTheme(QStringLiteral("list-add"))));
    m_newWorklaneBtn->setText(i18n("New Worklane"));
    m_newWorklaneBtn->setToolTip(i18n("Create new tab worklane (Ctrl+Shift+T)"));
    m_newWorklaneBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_newWorklaneBtn->setAutoRaise(true);
    m_newWorklaneBtn->setFixedSize(26, 26);
    connect(m_newWorklaneBtn, &QToolButton::clicked, this, &WorklaneSidebar::newWorklaneRequested);
    headerLayout->addWidget(m_newWorklaneBtn);

    rootLayout->addWidget(headerBar);

    // Tree Widget for Tabs & Lanes
    m_tree = new QTreeWidget(this);
    m_tree->setHeaderHidden(true);
    m_tree->setRootIsDecorated(false);
    m_tree->setIndentation(0);
    m_tree->setAnimated(true);
    m_tree->setFocusPolicy(Qt::NoFocus);
    m_tree->setItemDelegate(new WorklaneItemDelegate(m_tree));
    m_tree->setStyleSheet(QStringLiteral(
        "QTreeWidget { border: none; background: transparent; padding: 4px; }"
        "QTreeWidget::item { padding: 0px; margin: 0px; }"
    ));

    // Suppress Alt accelerator injection into shell paths and titles
    KAcceleratorManager::setNoAccel(m_tree);
    KAcceleratorManager::setNoAccel(this);

    connect(m_tree, &QTreeWidget::itemClicked, this, &WorklaneSidebar::onItemClicked);
    rootLayout->addWidget(m_tree, 1);

    // Debounce timer for terminal title & layout event floods (50ms trailing edge)
    m_debounceTimer.setSingleShot(true);
    m_debounceTimer.setInterval(50);
    connect(&m_debounceTimer, &QTimer::timeout, this, &WorklaneSidebar::refreshTree);
}

WorklaneSidebar::~WorklaneSidebar() = default;

void WorklaneSidebar::paintEvent(QPaintEvent* event) {
    QWidget::paintEvent(event);

    QPainter painter(this);
    KColorScheme windowScheme(QPalette::Active, KColorScheme::Window);

    // Plasma theme Window background (consistent with window background, not black)
    painter.fillRect(rect(), windowScheme.background(KColorScheme::NormalBackground).color());

    // 1px Right divider border separating sidebar from terminal canvas
    QColor borderCol = palette().color(QPalette::Mid);
    painter.setPen(QPen(borderCol, 1));
    painter.drawLine(rect().topRight(), rect().bottomRight());

    // 1px Bottom divider under the header bar
    painter.drawLine(QPoint(0, 35), QPoint(rect().width() - 1, 35));
}

void WorklaneSidebar::scheduleRefresh() {
    if (!isVisible()) return;
    if (!m_debounceTimer.isActive()) {
        m_debounceTimer.start();
    }
}

void WorklaneSidebar::markSurfaceAlert(TerminalSurface* surface, bool alert) {
    if (!surface) return;
    if (alert) {
        if (!m_alertSurfaces.contains(surface)) {
            m_alertSurfaces.append(surface);
            connect(surface, &QObject::destroyed, this, [this, surface]() {
                m_alertSurfaces.removeAll(surface);
            }, Qt::UniqueConnection);
        }
    } else {
        m_alertSurfaces.removeAll(surface);
    }
    scheduleRefresh();
}

void WorklaneSidebar::clearAlert(TerminalSurface* surface) {
    if (!surface) return;
    if (m_alertSurfaces.contains(surface)) {
        m_alertSurfaces.removeAll(surface);
        scheduleRefresh();
    }
}

QString WorklaneSidebar::formatPwd(const QString& fullPath) const {
    if (fullPath.isEmpty()) return QStringLiteral("~");
    QString home = QDir::homePath();
    if (fullPath == home) return QStringLiteral("~");
    if (fullPath.startsWith(home + QLatin1Char('/'))) {
        return QStringLiteral("~") + fullPath.mid(home.length());
    }
    return fullPath;
}

void WorklaneSidebar::refreshTree() {
    if (m_updating || !m_mainWindow || !isVisible()) return;
    m_updating = true;

    // Prune any dead pointers from alert list
    m_alertSurfaces.removeIf([](const QPointer<TerminalSurface>& ptr) {
        return ptr.isNull();
    });

    // Save vertical scrollbar position
    int savedScroll = m_tree->verticalScrollBar() ? m_tree->verticalScrollBar()->value() : 0;

    m_tree->clear();

    int tabCount = m_mainWindow->tabCount();
    for (int i = 0; i < tabCount; ++i) {
        WorklaneView* wl = m_mainWindow->worklaneAt(i);
        if (wl) {
            populateTab(i, wl);
        }
    }

    m_tree->expandAll();

    if (m_tree->verticalScrollBar()) {
        m_tree->verticalScrollBar()->setValue(savedScroll);
    }

    m_updating = false;
}

void WorklaneSidebar::populateTab(int tabIndex, WorklaneView* worklane) {
    auto* tabItem = new QTreeWidgetItem(m_tree);
    tabItem->setData(0, Qt::UserRole, 1); // Group

    QString proc = worklane->activeSurface() ? worklane->activeSurface()->processName() : QString();
    QString semanticTitle = worklane->activeTitle().trimmed();
    if (!proc.isEmpty() && (semanticTitle.isEmpty() || semanticTitle == QStringLiteral("~") || semanticTitle == QStringLiteral("Terminal"))) {
        semanticTitle = proc;
    } else if (semanticTitle.isEmpty()) {
        semanticTitle = i18n("Tab %1", tabIndex + 1);
    }

    tabItem->setData(0, Qt::DisplayRole, semanticTitle);
    tabItem->setData(0, Qt::UserRole + 1, worklane->paneCount());
    tabItem->setData(0, Qt::UserRole + 2, (worklane == m_mainWindow->currentWorklane()));
    tabItem->setData(0, Qt::UserRole + 10, tabIndex);

    int colCount = worklane->columnCount();

    // Flatten child panes inside this tab container
    QList<TerminalSurface*> tabSurfaces;
    QList<QString> tabColTags;

    for (int c = 0; c < colCount; ++c) {
        int rCount = worklane->rowCount(c);
        for (int r = 0; r < rCount; ++r) {
            TerminalSurface* s = worklane->surfaceAt(c, r);
            if (!s) continue;
            tabSurfaces.append(s);

            QString colTag;
            if (colCount > 1) {
                if (rCount > 1) {
                    colTag = QStringLiteral("Col %1/%2 · %3/%4").arg(c + 1).arg(colCount).arg(r + 1).arg(rCount);
                } else {
                    colTag = QStringLiteral("Col %1/%2").arg(c + 1).arg(colCount);
                }
            } else if (rCount > 1) {
                colTag = QStringLiteral("Split %1/%2").arg(r + 1).arg(rCount);
            }
            tabColTags.append(colTag);
        }
    }

    for (int i = 0; i < tabSurfaces.size(); ++i) {
        TerminalSurface* s = tabSurfaces[i];
        auto* laneItem = new QTreeWidgetItem(tabItem);
        laneItem->setData(0, Qt::UserRole, 2); // Lane Row

        QString pName = s->processName();
        QString sTitle = s->title().trimmed();
        if (!pName.isEmpty() && (sTitle.isEmpty() || sTitle == QStringLiteral("~") || sTitle == QStringLiteral("Terminal"))) {
            sTitle = pName;
        } else if (sTitle.isEmpty()) {
            sTitle = i18n("Terminal");
        }

        laneItem->setData(0, Qt::DisplayRole, sTitle);
        laneItem->setData(0, Qt::UserRole + 1, formatPwd(s->currentWorkingDirectory()));
        laneItem->setData(0, Qt::UserRole + 2, tabColTags[i]);

        bool isActive = (s == m_mainWindow->activeSurface());
        laneItem->setData(0, Qt::UserRole + 3, isActive);

        bool alert = m_alertSurfaces.contains(s);
        laneItem->setData(0, Qt::UserRole + 4, alert);

        bool isLast = (i == tabSurfaces.size() - 1);
        laneItem->setData(0, Qt::UserRole + 5, isLast);

        laneItem->setData(0, Qt::UserRole + 10, tabIndex);
        laneItem->setData(0, Qt::UserRole + 11, QVariant::fromValue(QPointer<TerminalSurface>(s)));
    }
}

void WorklaneSidebar::onItemClicked(QTreeWidgetItem* item, int column) {
    Q_UNUSED(column);
    if (!item || !m_mainWindow) return;

    int itemType = item->data(0, Qt::UserRole).toInt();
    int tabIndex = item->data(0, Qt::UserRole + 10).toInt();

    m_mainWindow->setCurrentTabIndex(tabIndex);

    if (itemType == 2) {
        auto surfacePtr = item->data(0, Qt::UserRole + 11).value<QPointer<TerminalSurface>>();
        if (surfacePtr && !surfacePtr.isNull()) {
            clearAlert(surfacePtr.data());
            WorklaneView* wl = m_mainWindow->currentWorklane();
            if (wl) {
                wl->ensureSurfaceVisible(surfacePtr.data());
            }
            surfacePtr->setFocus();
        }
    }

    // Schedule queued refresh rather than clear() synchronously inside click event dispatch
    QMetaObject::invokeMethod(this, &WorklaneSidebar::refreshTree, Qt::QueuedConnection);
}
