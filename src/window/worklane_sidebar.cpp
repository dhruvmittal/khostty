#include "window/worklane_sidebar.h"
#include "window/main_window.h"
#include "terminal/worklane_view.h"
#include "terminal/terminal_surface.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QToolButton>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QDir>
#include <QIcon>
#include <QScrollBar>
#include <KLocalizedString>
#include <KAcceleratorManager>

Q_DECLARE_METATYPE(QPointer<TerminalSurface>)

class WorklaneItemDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        int itemType = index.data(Qt::UserRole).toInt();
        int fontH = option.fontMetrics.height();
        if (itemType == 1) { // Tab Group
            return QSize(option.rect.width(), std::max(26, fontH + 8));
        } else if (itemType == 2) { // Lane Row (2-line card)
            return QSize(option.rect.width(), std::max(42, fontH * 2 + 10));
        }
        return QStyledItemDelegate::sizeHint(option, index);
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);

        int itemType = index.data(Qt::UserRole).toInt();
        QRect rect = option.rect;

        if (itemType == 1) {
            // Tab Group Header
            QString tabName = index.data(Qt::DisplayRole).toString();
            int paneCount = index.data(Qt::UserRole + 1).toInt();
            bool isCurrentTab = index.data(Qt::UserRole + 2).toBool();

            QRect bgRect = rect.adjusted(2, 2, -2, -2);
            if (isCurrentTab) {
                painter->setPen(Qt::NoPen);
                painter->setBrush(option.palette.color(QPalette::AlternateBase));
                painter->drawRoundedRect(bgRect, 4, 4);
            }

            QFont font = option.font;
            font.setBold(true);
            painter->setFont(font);

            QColor textColor = isCurrentTab ? option.palette.color(QPalette::Highlight)
                                            : option.palette.color(QPalette::WindowText);
            painter->setPen(textColor);

            int badgeWidth = option.fontMetrics.horizontalAdvance(i18n("%1 lanes", paneCount)) + 12;
            QRect textRect = rect.adjusted(8, 0, -badgeWidth - 4, 0);
            QString elidedTabName = option.fontMetrics.elidedText(tabName, Qt::ElideRight, textRect.width());
            painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, elidedTabName);

            // Count badge
            QString badge = i18n("%1 lanes", paneCount);
            QFont badgeFont = option.font;
            badgeFont.setBold(false);
            painter->setFont(badgeFont);
            painter->setPen(option.palette.color(QPalette::PlaceholderText));
            QRect badgeRect = rect.adjusted(rect.width() - badgeWidth - 4, 0, -6, 0);
            painter->drawText(badgeRect, Qt::AlignVCenter | Qt::AlignRight, badge);

        } else if (itemType == 2) {
            // Lane Card
            QString title = index.data(Qt::DisplayRole).toString();
            QString pwd = index.data(Qt::UserRole + 1).toString();
            QString colTag = index.data(Qt::UserRole + 2).toString();
            bool isActive = index.data(Qt::UserRole + 3).toBool();
            bool hasAlert = index.data(Qt::UserRole + 4).toBool();

            QRect cardRect = rect.adjusted(6, 2, -4, -2);

            // Card background & selection
            if (isActive) {
                QColor selBg = option.palette.color(QPalette::Highlight);
                selBg.setAlpha(45);
                painter->setPen(QPen(option.palette.color(QPalette::Highlight), 1.5));
                painter->setBrush(selBg);
                painter->drawRoundedRect(cardRect, 5, 5);
            } else if (option.state & QStyle::State_MouseOver) {
                painter->setPen(Qt::NoPen);
                painter->setBrush(option.palette.color(QPalette::AlternateBase));
                painter->drawRoundedRect(cardRect, 5, 5);
            }

            // Status Indicator Dot
            int dotR = 3;
            int dotX = cardRect.left() + 8;
            int dotY = cardRect.top() + (cardRect.height() / 2) - dotR;
            painter->setPen(Qt::NoPen);
            if (hasAlert) {
                painter->setBrush(QColor(235, 160, 20)); // Amber alert
            } else if (isActive) {
                painter->setBrush(option.palette.color(QPalette::Highlight));
            } else {
                painter->setBrush(option.palette.color(QPalette::Mid));
            }
            painter->drawEllipse(QPoint(dotX + dotR, dotY + dotR), dotR, dotR);

            int leftContentX = dotX + dotR * 2 + 8;
            int tagWidth = colTag.isEmpty() ? 0 : (option.fontMetrics.horizontalAdvance(colTag) + 12);

            // Line 1: Title
            int lineH = std::max(16, option.fontMetrics.height());
            QRect titleRect(leftContentX, cardRect.top() + 3, cardRect.width() - (leftContentX - cardRect.left()) - tagWidth - 4, lineH);
            QFont titleFont = option.font;
            titleFont.setBold(isActive);
            painter->setFont(titleFont);
            painter->setPen(option.palette.color(QPalette::WindowText));
            QString elidedTitle = option.fontMetrics.elidedText(title, Qt::ElideRight, titleRect.width());
            painter->drawText(titleRect, Qt::AlignVCenter | Qt::AlignLeft, elidedTitle);

            // Tag (Col 1/2, etc.)
            if (!colTag.isEmpty()) {
                QFont tagFont = option.font;
                painter->setFont(tagFont);
                painter->setPen(option.palette.color(QPalette::PlaceholderText));
                QRect tagRect(cardRect.right() - tagWidth - 2, cardRect.top() + 3, tagWidth, lineH);
                painter->drawText(tagRect, Qt::AlignVCenter | Qt::AlignRight, colTag);
            }

            // Line 2: PWD
            QRect pwdRect(leftContentX, cardRect.top() + lineH + 2, cardRect.width() - (leftContentX - cardRect.left()) - 4, lineH);
            QFont pwdFont = option.font;
            painter->setFont(pwdFont);
            painter->setPen(option.palette.color(QPalette::PlaceholderText));
            QString elidedPwd = option.fontMetrics.elidedText(pwd, Qt::ElideRight, pwdRect.width());
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
    setMinimumWidth(180);
    setMaximumWidth(450);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    // Header Toolbar with KDE Native QToolButton
    m_newWorklaneBtn = new QToolButton(this);
    m_newWorklaneBtn->setFocusPolicy(Qt::NoFocus);
    m_newWorklaneBtn->setIcon(QIcon::fromTheme(QStringLiteral("tab-new"), QIcon::fromTheme(QStringLiteral("list-add"))));
    m_newWorklaneBtn->setText(i18n("New Worklane"));
    m_newWorklaneBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_newWorklaneBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(m_newWorklaneBtn, &QToolButton::clicked, this, &WorklaneSidebar::newWorklaneRequested);
    layout->addWidget(m_newWorklaneBtn);

    // Tree Widget for Tabs & Lanes
    m_tree = new QTreeWidget(this);
    m_tree->setHeaderHidden(true);
    m_tree->setRootIsDecorated(false);
    m_tree->setIndentation(0);
    m_tree->setAnimated(true);
    m_tree->setFocusPolicy(Qt::NoFocus);
    m_tree->setItemDelegate(new WorklaneItemDelegate(m_tree));
    m_tree->setStyleSheet(QStringLiteral(
        "QTreeWidget { border: none; background: transparent; }"
        "QTreeWidget::item { padding: 0px; margin: 0px; }"
    ));

    // Suppress Alt accelerator injection into shell paths and titles
    KAcceleratorManager::setNoAccel(m_tree);
    KAcceleratorManager::setNoAccel(this);

    connect(m_tree, &QTreeWidget::itemClicked, this, &WorklaneSidebar::onItemClicked);
    layout->addWidget(m_tree, 1);

    // Debounce timer for terminal title & layout event floods (50ms trailing edge)
    m_debounceTimer.setSingleShot(true);
    m_debounceTimer.setInterval(50);
    connect(&m_debounceTimer, &QTimer::timeout, this, &WorklaneSidebar::refreshTree);
}

WorklaneSidebar::~WorklaneSidebar() = default;

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

    // Use clean semantic activeTitle from WorklaneView, not decorated tabText
    QString semanticTitle = worklane->activeTitle().trimmed();
    if (semanticTitle.isEmpty()) {
        semanticTitle = i18n("Tab %1", tabIndex + 1);
    }
    tabItem->setData(0, Qt::DisplayRole, semanticTitle);
    tabItem->setData(0, Qt::UserRole + 1, worklane->paneCount());
    tabItem->setData(0, Qt::UserRole + 2, (worklane == m_mainWindow->currentWorklane()));
    tabItem->setData(0, Qt::UserRole + 10, tabIndex);

    int colCount = worklane->columnCount();

    // Iterate columns and rows hierarchically for mathematically accurate column tags
    for (int c = 0; c < colCount; ++c) {
        int rCount = worklane->rowCount(c);
        for (int r = 0; r < rCount; ++r) {
            TerminalSurface* s = worklane->surfaceAt(c, r);
            if (!s) continue;

            auto* laneItem = new QTreeWidgetItem(tabItem);
            laneItem->setData(0, Qt::UserRole, 2); // Lane Row

            QString sTitle = s->title().isEmpty() ? i18n("Terminal") : s->title();
            laneItem->setData(0, Qt::DisplayRole, sTitle);
            laneItem->setData(0, Qt::UserRole + 1, formatPwd(s->currentWorkingDirectory()));

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
            laneItem->setData(0, Qt::UserRole + 2, colTag);

            bool isActive = (s == m_mainWindow->activeSurface());
            laneItem->setData(0, Qt::UserRole + 3, isActive);

            bool alert = m_alertSurfaces.contains(s);
            laneItem->setData(0, Qt::UserRole + 4, alert);

            laneItem->setData(0, Qt::UserRole + 10, tabIndex);
            laneItem->setData(0, Qt::UserRole + 11, QVariant::fromValue(QPointer<TerminalSurface>(s)));
        }
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
