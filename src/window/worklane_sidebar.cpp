#include "window/worklane_sidebar.h"
#include "terminal/terminal_surface.h"
#include "terminal/worklane_view.h"
#include "window/main_window.h"

#include <KAcceleratorManager>
#include <KColorScheme>
#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>
#include <QApplication>
#include <QDir>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMimeData>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QStyledItemDelegate>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <algorithm>

Q_DECLARE_METATYPE(QPointer<TerminalSurface>)

class WorklaneItemDelegate : public QStyledItemDelegate {
public:
  explicit WorklaneItemDelegate(WorklaneSidebar *sidebar,
                                QObject *parent = nullptr)
      : QStyledItemDelegate(parent), m_sidebar(sidebar) {}

  QSize sizeHint(const QStyleOptionViewItem &option,
                 const QModelIndex &index) const override {
    int itemType = index.data(Qt::UserRole).toInt();
    int fontH = option.fontMetrics.height();
    if (itemType == 1) { // Clean section header with top spacing
      return QSize(option.rect.width(), std::max(28, fontH + 14));
    } else if (itemType == 2) { // Lane card row
      return QSize(option.rect.width(), std::max(46, fontH * 2 + 14));
    }
    return QStyledItemDelegate::sizeHint(option, index);
  }

  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override {
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    if (m_sidebar && m_sidebar->isDragging() &&
        m_sidebar->dragItemType() == 2) {
      int itemTab = index.data(Qt::UserRole + 10).toInt();
      int srcTab = m_sidebar->dragSourceTabIndex();
      if (auto *mw = m_sidebar->mainWindow()) {
        WorklaneView *srcWl = mw->worklaneAt(srcTab);
        WorklaneView *thisWl = mw->worklaneAt(itemTab);
        if (srcWl && thisWl && !thisWl->canAcceptPaneFrom(srcWl)) {
          painter->setOpacity(0.35);
        }
      }
    }

    int itemType = index.data(Qt::UserRole).toInt();
    QRect rect = option.rect;

    KColorScheme viewScheme(QPalette::Active, KColorScheme::View);
    KColorScheme windowScheme(QPalette::Active, KColorScheme::Window);

    if (itemType == 1) {
      // Clean unboxed section header (matching mockup with extra top margin)
      QString tabName = index.data(Qt::DisplayRole).toString();
      int paneCount = index.data(Qt::UserRole + 1).toInt();
      bool isCurrentTab = index.data(Qt::UserRole + 2).toBool();

      QRect headerRect = rect.adjusted(10, 8, -10, 0);

      QColor vibrantAccent = m_sidebar
                                 ? m_sidebar->vibrantAccentColor()
                                 : option.palette.color(QPalette::Highlight);

      int badgeWidth =
          option.fontMetrics.horizontalAdvance(i18n("%1 panes", paneCount)) +
          16;

      int leftOffset = 0;
      if (isCurrentTab) {
        // Vibrant vertical accent indicator pill on the left of active tab
        int pillH = std::max(12, option.fontMetrics.height() - 2);
        QRect accentPill(headerRect.left(),
                         headerRect.top() + (headerRect.height() - pillH) / 2,
                         3, pillH);
        painter->setPen(Qt::NoPen);
        painter->setBrush(vibrantAccent);
        painter->drawRoundedRect(accentPill, 1.5, 1.5);
        leftOffset = 8;
      }

      QRect textRect = headerRect.adjusted(leftOffset, 0, -badgeWidth - 4, 0);

      // Title text (unboxed, clean typography)
      QFont font = option.font;
      font.setBold(true);
      font.setPointSize(std::max(8, font.pointSize() - 1));
      painter->setFont(font);

      QColor textColor =
          isCurrentTab
              ? vibrantAccent
              : viewScheme.foreground(KColorScheme::InactiveText).color();
      painter->setPen(textColor);

      QString elidedTabName = option.fontMetrics.elidedText(
          tabName.toUpper(), Qt::ElideRight, textRect.width());
      painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft,
                        elidedTabName);

      // Subtle count badge pill on the right
      QString badge = i18n("%1 panes", paneCount);
      QFont badgeFont = option.font;
      badgeFont.setBold(false);
      badgeFont.setPointSize(std::max(7, badgeFont.pointSize() - 2));
      painter->setFont(badgeFont);

      int pillW = painter->fontMetrics().horizontalAdvance(badge) + 10;
      int pillH = std::max(15, painter->fontMetrics().height() + 2);
      QRect pillRect(headerRect.right() - pillW,
                     headerRect.top() + (headerRect.height() - pillH) / 2,
                     pillW, pillH);

      if (isCurrentTab) {
        QColor pillBg = vibrantAccent;
        pillBg.setAlpha(40);
        painter->setPen(QPen(vibrantAccent, 1));
        painter->setBrush(pillBg);
        painter->drawRoundedRect(pillRect, 3, 3);

        painter->setPen(vibrantAccent);
        painter->drawText(pillRect, Qt::AlignCenter, badge);
      } else {
        QColor pillBg = option.palette.color(QPalette::Mid);
        pillBg.setAlpha(40);
        painter->setPen(Qt::NoPen);
        painter->setBrush(pillBg);
        painter->drawRoundedRect(pillRect, 3, 3);

        painter->setPen(
            viewScheme.foreground(KColorScheme::InactiveText).color());
        painter->drawText(pillRect, Qt::AlignCenter, badge);
      }

    } else if (itemType == 2) {
      // Lane Card
      QString title = index.data(Qt::DisplayRole).toString();
      QString pwd = index.data(Qt::UserRole + 1).toString();
      QString colTag = index.data(Qt::UserRole + 2).toString();
      bool isActive = index.data(Qt::UserRole + 3).toBool();
      bool hasAlert = index.data(Qt::UserRole + 4).toBool();

      QRect cardRect = rect.adjusted(8, 2, -8, -2);

      QColor vibrantAccent = m_sidebar
                                 ? m_sidebar->accentColor()
                                 : option.palette.color(QPalette::Highlight);

      QColor cardBg = option.palette.color(QPalette::Window);
      QColor borderCol = option.palette.color(QPalette::Mid);

      if (isActive) {
        // Active pane gets elevated background with crisp contrast using accent
        // color
        cardBg = vibrantAccent;
        borderCol = vibrantAccent;
      } else if (hasAlert) {
        cardBg = QColor(235, 160, 20, 35);
        borderCol = QColor(235, 160, 20, 200);
      } else if (option.state & QStyle::State_MouseOver) {
        cardBg = option.palette.color(QPalette::AlternateBase);
        borderCol = vibrantAccent.lighter(120);
      }

      painter->setPen(QPen(borderCol, 1));
      painter->setBrush(cardBg);
      painter->drawRoundedRect(cardRect, 5, 5);

      // Determine readable foreground color on active card background
      bool lightActiveCard = (cardBg.lightnessF() > 0.60);
      QColor activeFg = lightActiveCard
                            ? Qt::black
                            : option.palette.color(QPalette::HighlightedText);
      QColor activePwdCol =
          lightActiveCard ? QColor(0, 0, 0, 190) : QColor(255, 255, 255, 220);

      // Status Indicator Dot
      int dotR = 3;
      int dotX = cardRect.left() + 10;
      int dotY = cardRect.top() + (cardRect.height() / 2) - dotR;
      painter->setPen(Qt::NoPen);
      if (hasAlert) {
        painter->setBrush(QColor(235, 160, 20)); // Amber alert
      } else if (isActive) {
        painter->setBrush(activeFg);
      } else {
        painter->setBrush(
            viewScheme.foreground(KColorScheme::InactiveText).color());
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

        QRect tagPillRect(cardRect.right() - tagPillWidth - 6,
                          cardRect.top() + 5, tagPillWidth, tagPillH);
        QColor tagBg = isActive ? (lightActiveCard ? vibrantAccent.lighter(115)
                                                   : vibrantAccent.darker(115))
                                : option.palette.color(QPalette::AlternateBase);
        painter->setPen(
            QPen(isActive ? activeFg : option.palette.color(QPalette::Mid), 1));
        painter->setBrush(tagBg);
        painter->drawRoundedRect(tagPillRect, 3, 3);

        painter->setFont(tagFont);
        painter->setPen(
            isActive ? activeFg
                     : viewScheme.foreground(KColorScheme::NormalText).color());
        painter->drawText(tagPillRect, Qt::AlignCenter, colTag);
      }

      // Line 1: Title (High contrast: activeFg when active, NormalText when
      // inactive)
      int lineH = std::max(15, option.fontMetrics.height());
      QRect titleRect(leftContentX, cardRect.top() + 4,
                      cardRect.width() - (leftContentX - cardRect.left()) -
                          tagPillWidth - 8,
                      lineH);
      QFont titleFont = option.font;
      titleFont.setBold(isActive);
      painter->setFont(titleFont);
      painter->setPen(
          isActive ? activeFg
                   : viewScheme.foreground(KColorScheme::NormalText).color());
      QString elidedTitle = option.fontMetrics.elidedText(title, Qt::ElideRight,
                                                          titleRect.width());
      painter->drawText(titleRect, Qt::AlignVCenter | Qt::AlignLeft,
                        elidedTitle);

      // Line 2: PWD in System Monospace Font (High contrast: activePwdCol when
      // active, InactiveText when inactive)
      QRect pwdRect(leftContentX, cardRect.top() + lineH + 4,
                    cardRect.width() - (leftContentX - cardRect.left()) - 6,
                    lineH);
      painter->setFont(monoFont);
      painter->setPen(
          isActive ? activePwdCol
                   : viewScheme.foreground(KColorScheme::InactiveText).color());
      QFontMetrics monoFm(monoFont);
      QString elidedPwd =
          monoFm.elidedText(pwd, Qt::ElideRight, pwdRect.width());
      painter->drawText(pwdRect, Qt::AlignVCenter | Qt::AlignLeft, elidedPwd);

    } else {
      QStyledItemDelegate::paint(painter, option, index);
    }

    painter->restore();
  }

private:
  WorklaneSidebar *m_sidebar = nullptr;
};

class SidebarTreeWidget : public QTreeWidget {
public:
  explicit SidebarTreeWidget(WorklaneSidebar *sidebar,
                             QWidget *parent = nullptr)
      : QTreeWidget(parent), m_sidebar(sidebar) {
    viewport()->setAcceptDrops(true);
  }

  void clearDropIndicator() {
    m_dropTargetItem = nullptr;
    m_dropIsStack = false;
    m_dropInsertAfter = false;
    viewport()->update();
  }

protected:
  void mousePressEvent(QMouseEvent *event) override {
    if (event->button() == Qt::LeftButton) {
      m_dragStartPos = event->pos();
    }
    QTreeWidget::mousePressEvent(event);
  }

  void mouseMoveEvent(QMouseEvent *event) override {
    if ((event->buttons() & Qt::LeftButton) && !m_isDraggingLocal) {
      if ((event->pos() - m_dragStartPos).manhattanLength() >=
          QApplication::startDragDistance()) {
        QTreeWidgetItem *item = itemAt(m_dragStartPos);
        if (item) {
          startSidebarDrag(item);
          return;
        }
      }
    }
    QTreeWidget::mouseMoveEvent(event);
  }

  void dragEnterEvent(QDragEnterEvent *event) override {
    if (event->mimeData()->hasFormat(
            QStringLiteral("application/x-khostty-sidebar-drag"))) {
      event->acceptProposedAction();
    } else {
      event->ignore();
    }
  }

  void dragMoveEvent(QDragMoveEvent *event) override {
    if (!event->mimeData()->hasFormat(
            QStringLiteral("application/x-khostty-sidebar-drag"))) {
      event->ignore();
      return;
    }

    QPoint pos = event->position().toPoint();
    if (pos.y() < 24 && verticalScrollBar()) {
      verticalScrollBar()->setValue(verticalScrollBar()->value() - 10);
    } else if (pos.y() > viewport()->height() - 24 && verticalScrollBar()) {
      verticalScrollBar()->setValue(verticalScrollBar()->value() + 10);
    }

    bool isCtrl = (event->modifiers() & Qt::ControlModifier) ||
                  (QGuiApplication::keyboardModifiers() & Qt::ControlModifier);
    if (updateDropState(pos, isCtrl)) {
      event->acceptProposedAction();
    } else {
      event->ignore();
    }
  }

  void dragLeaveEvent(QDragLeaveEvent *event) override {
    Q_UNUSED(event);
    clearDropIndicator();
  }

  void dropEvent(QDropEvent *event) override {
    if (!m_dropTargetItem || !m_sidebar || !m_sidebar->mainWindow()) {
      clearDropIndicator();
      event->ignore();
      return;
    }

    int targetType = m_dropTargetItem->data(0, Qt::UserRole).toInt();
    int targetTab = m_dropTargetItem->data(0, Qt::UserRole + 10).toInt();
    TerminalSurface *targetSurface = nullptr;
    if (targetType == 2) {
      targetSurface = m_dropTargetItem->data(0, Qt::UserRole + 11)
                          .value<QPointer<TerminalSurface>>()
                          .data();
    }

    int srcType = m_draggedType;
    int srcTab = m_draggedTabIndex;
    TerminalSurface *srcSurface = m_draggedSurface;
    bool isStack = m_dropIsStack;
    bool insertAfter = m_dropInsertAfter;

    clearDropIndicator();
    event->acceptProposedAction();

    MainWindow *mw = m_sidebar->mainWindow();
    if (srcType == 1) { // Tab Header
      if (srcTab != targetTab) {
        int destTab = targetTab;
        if (insertAfter && targetTab > srcTab) {
          destTab = targetTab;
        } else if (!insertAfter && targetTab > srcTab) {
          destTab = targetTab - 1;
        } else if (insertAfter && targetTab < srcTab) {
          destTab = targetTab + 1;
        } else if (!insertAfter && targetTab < srcTab) {
          destTab = targetTab;
        }
        mw->moveTab(srcTab, destTab);
      }
    } else if (srcType == 2 && srcSurface) { // Pane Card
      mw->movePane(srcSurface, targetTab, targetSurface, insertAfter, isStack);
    }
  }

  void keyPressEvent(QKeyEvent *event) override {
    if (m_isDraggingLocal && (event->key() == Qt::Key_Control)) {
      updateDropState(m_lastDragPos, true);
    }
    QTreeWidget::keyPressEvent(event);
  }

  void keyReleaseEvent(QKeyEvent *event) override {
    if (m_isDraggingLocal && (event->key() == Qt::Key_Control)) {
      updateDropState(m_lastDragPos, false);
    }
    QTreeWidget::keyReleaseEvent(event);
  }

  void paintEvent(QPaintEvent *event) override {
    QTreeWidget::paintEvent(event);

    if (!m_dropTargetItem)
      return;

    QRect rect = visualItemRect(m_dropTargetItem);
    if (!rect.isValid() || rect.isEmpty())
      return;

    QPainter painter(viewport());
    painter.setRenderHint(QPainter::Antialiasing, true);

    QColor accent = m_sidebar ? m_sidebar->vibrantAccentColor()
                              : palette().color(QPalette::Highlight);

    if (m_dropIsStack) {
      QRect cardRect = rect.adjusted(8, 2, -8, -2);
      QColor fillCol = accent;
      fillCol.setAlpha(35);
      painter.setPen(QPen(accent, 2));
      painter.setBrush(fillCol);
      painter.drawRoundedRect(cardRect, 5, 5);

      painter.setPen(Qt::NoPen);
      painter.setBrush(accent);
      int pillW = std::min(60, cardRect.width() - 20);
      int pillH = 3;
      QRect pill(cardRect.center().x() - pillW / 2,
                 cardRect.bottom() - pillH - 1, pillW, pillH);
      painter.drawRoundedRect(pill, 1.5, 1.5);
    } else {
      int y = m_dropInsertAfter ? rect.bottom() : rect.top();
      y = std::clamp(y, 2, viewport()->height() - 2);
      int x1 = rect.left() + 8;
      int x2 = rect.right() - 8;

      painter.setPen(QPen(accent, 2));
      painter.drawLine(QPoint(x1, y), QPoint(x2, y));

      painter.setPen(Qt::NoPen);
      painter.setBrush(accent);
      painter.drawEllipse(QPoint(x1 + 2, y), 3, 3);
    }
  }

private:
  bool updateDropState(const QPoint &pos, bool ctrlPressed) {
    m_lastDragPos = pos;
    QTreeWidgetItem *targetItem = itemAt(pos);
    if (!targetItem || !m_sidebar || !m_sidebar->mainWindow()) {
      clearDropIndicator();
      return false;
    }

    int targetTab = targetItem->data(0, Qt::UserRole + 10).toInt();
    int srcType = m_draggedType;
    int srcTab = m_draggedTabIndex;

    if (srcType == 2) {
      MainWindow *mw = m_sidebar->mainWindow();
      WorklaneView *srcWl = mw->worklaneAt(srcTab);
      WorklaneView *tgtWl = mw->worklaneAt(targetTab);
      if (!srcWl || !tgtWl || !tgtWl->canAcceptPaneFrom(srcWl)) {
        clearDropIndicator();
        return false;
      }
    }

    QRect itemRect = visualItemRect(targetItem);
    bool inBottomHalf = (pos.y() > itemRect.center().y());

    m_dropTargetItem = targetItem;
    m_dropIsStack = (srcType == 2 && ctrlPressed);
    m_dropInsertAfter = inBottomHalf;

    viewport()->update();
    return true;
  }

  void startSidebarDrag(QTreeWidgetItem *item) {
    m_draggedType = item->data(0, Qt::UserRole).toInt();
    m_draggedTabIndex = item->data(0, Qt::UserRole + 10).toInt();
    m_draggedSurface = (m_draggedType == 2)
                           ? item->data(0, Qt::UserRole + 11)
                                 .value<QPointer<TerminalSurface>>()
                                 .data()
                           : nullptr;

    m_isDraggingLocal = true;
    if (m_sidebar) {
      m_sidebar->setDragging(true);
      m_sidebar->setDragSourceTabIndex(m_draggedTabIndex);
      m_sidebar->setDragItemType(m_draggedType);
    }

    auto *drag = new QDrag(this);
    auto *mimeData = new QMimeData();
    mimeData->setData(QStringLiteral("application/x-khostty-sidebar-drag"),
                      QByteArray("khostty"));
    drag->setMimeData(mimeData);

    QRect rect = visualItemRect(item);
    if (rect.isValid() && !rect.isEmpty()) {
      QPixmap pixmap(rect.size());
      pixmap.fill(Qt::transparent);
      QPainter p(&pixmap);
      p.setOpacity(0.85);
      render(&p, QPoint(0, 0), QRegion(rect));
      p.end();
      drag->setPixmap(pixmap);
      drag->setHotSpot(m_dragStartPos - rect.topLeft());
    }

    viewport()->update();
    drag->exec(Qt::MoveAction);

    m_isDraggingLocal = false;
    m_draggedSurface = nullptr;
    clearDropIndicator();
    if (m_sidebar) {
      m_sidebar->setDragging(false);
      m_sidebar->setDragSourceTabIndex(-1);
      m_sidebar->setDragItemType(0);
      m_sidebar->scheduleRefresh();
    }
    viewport()->update();
  }

  WorklaneSidebar *m_sidebar = nullptr;
  QPoint m_dragStartPos;
  QPoint m_lastDragPos;
  bool m_isDraggingLocal = false;
  int m_draggedType = 0;
  int m_draggedTabIndex = -1;
  TerminalSurface *m_draggedSurface = nullptr;

  QTreeWidgetItem *m_dropTargetItem = nullptr;
  bool m_dropIsStack = false;
  bool m_dropInsertAfter = false;
};

WorklaneSidebar::WorklaneSidebar(MainWindow *mainWindow, QWidget *parent)
    : QWidget(parent), m_mainWindow(mainWindow) {
  setMinimumWidth(200);
  setMaximumWidth(450);

  auto *rootLayout = new QVBoxLayout(this);
  rootLayout->setContentsMargins(0, 0, 0, 0);
  rootLayout->setSpacing(0);

  // Header Toolbar Area with Clean KDE Layout
  auto *headerBar = new QWidget(this);
  headerBar->setFixedHeight(36);
  auto *headerLayout = new QHBoxLayout(headerBar);
  headerLayout->setContentsMargins(12, 4, 8, 4);
  headerLayout->setSpacing(8);

  auto *headerLabel = new QLabel(i18n("Worklanes"), headerBar);
  QFont labelFont = headerLabel->font();
  labelFont.setBold(true);
  headerLabel->setFont(labelFont);
  headerLayout->addWidget(headerLabel);

  headerLayout->addStretch(1);

  m_newWorklaneBtn = new QToolButton(headerBar);
  m_newWorklaneBtn->setFocusPolicy(Qt::NoFocus);
  m_newWorklaneBtn->setIcon(QIcon::fromTheme(
      QStringLiteral("tab-new"), QIcon::fromTheme(QStringLiteral("list-add"))));
  m_newWorklaneBtn->setText(i18n("New Worklane"));
  m_newWorklaneBtn->setToolTip(i18n("Create new tab worklane (Ctrl+Shift+T)"));
  m_newWorklaneBtn->setToolButtonStyle(Qt::ToolButtonIconOnly);
  m_newWorklaneBtn->setAutoRaise(true);
  m_newWorklaneBtn->setFixedSize(26, 26);
  connect(m_newWorklaneBtn, &QToolButton::clicked, this,
          &WorklaneSidebar::newWorklaneRequested);
  headerLayout->addWidget(m_newWorklaneBtn);

  rootLayout->addWidget(headerBar);

  // Tree Widget for Tabs & Lanes
  m_tree = new SidebarTreeWidget(this, this);
  m_tree->setHeaderHidden(true);
  m_tree->setRootIsDecorated(false);
  m_tree->setIndentation(0);
  m_tree->setAnimated(true);
  m_tree->setFocusPolicy(Qt::NoFocus);
  m_tree->setItemDelegate(new WorklaneItemDelegate(this, m_tree));
  m_tree->setStyleSheet(QStringLiteral(
      "QTreeWidget { border: none; background: transparent; padding: 4px; }"
      "QTreeWidget::item { padding: 0px; margin: 0px; }"));

  // Suppress Alt accelerator injection into shell paths and titles
  KAcceleratorManager::setNoAccel(m_tree);
  KAcceleratorManager::setNoAccel(this);

  connect(m_tree, &QTreeWidget::itemClicked, this,
          &WorklaneSidebar::onItemClicked);
  rootLayout->addWidget(m_tree, 1);

  // Debounce timer for terminal title & layout event floods (50ms trailing
  // edge)
  m_debounceTimer.setSingleShot(true);
  m_debounceTimer.setInterval(50);
  connect(&m_debounceTimer, &QTimer::timeout, this,
          &WorklaneSidebar::refreshTree);
}

WorklaneSidebar::~WorklaneSidebar() = default;

void WorklaneSidebar::paintEvent(QPaintEvent *event) {
  QWidget::paintEvent(event);

  QPainter painter(this);
  KColorScheme windowScheme(QPalette::Active, KColorScheme::Window);

  // Plasma theme Window background (consistent with window background, not
  // black)
  painter.fillRect(
      rect(), windowScheme.background(KColorScheme::NormalBackground).color());

  // 1px Right divider border separating sidebar from terminal canvas
  QColor borderCol = palette().color(QPalette::Mid);
  painter.setPen(QPen(borderCol, 1));
  painter.drawLine(rect().topRight(), rect().bottomRight());

  // 1px Bottom divider under the header bar
  painter.drawLine(QPoint(0, 35), QPoint(rect().width() - 1, 35));
}

void WorklaneSidebar::scheduleRefresh() {
  if (!isVisible() || m_isDragging)
    return;
  if (!m_debounceTimer.isActive()) {
    m_debounceTimer.start();
  }
}

void WorklaneSidebar::markSurfaceAlert(TerminalSurface *surface, bool alert) {
  if (!surface)
    return;
  if (alert) {
    if (!m_alertSurfaces.contains(surface)) {
      m_alertSurfaces.append(surface);
      connect(
          surface, &QObject::destroyed, this,
          [this, surface]() { m_alertSurfaces.removeAll(surface); },
          Qt::UniqueConnection);
    }
  } else {
    m_alertSurfaces.removeAll(surface);
  }
  scheduleRefresh();
}

void WorklaneSidebar::clearAlert(TerminalSurface *surface) {
  if (!surface)
    return;
  if (m_alertSurfaces.contains(surface)) {
    m_alertSurfaces.removeAll(surface);
    scheduleRefresh();
  }
}

QColor WorklaneSidebar::accentColor() const {
  KConfigGroup appCfg(KSharedConfig::openConfig(QStringLiteral("khosttyrc")),
                      QStringLiteral("Appearance"));
  QString customHex = appCfg.readEntry("CustomAccentColor", QString());
  if (!customHex.isEmpty()) {
    QColor customCol(customHex);
    if (customCol.isValid()) {
      return customCol;
    }
  }

  // Try QPalette::Accent (Qt 6.6+ native accent support)
  QColor col = palette().color(QPalette::Accent);
  if (!col.isValid() || col == palette().color(QPalette::Window)) {
    // Fallback to KDE FocusColor decoration
    KColorScheme viewScheme(QPalette::Active, KColorScheme::View);
    col = viewScheme.decoration(KColorScheme::FocusColor).color();
  }
  if (!col.isValid() || col == palette().color(QPalette::Window)) {
    col = palette().color(QPalette::Highlight);
  }
  return col;
}

QColor WorklaneSidebar::vibrantAccentColor() const {
  QColor col = accentColor();
  KConfigGroup appCfg(KSharedConfig::openConfig(QStringLiteral("khosttyrc")),
                      QStringLiteral("Appearance"));
  if (appCfg.hasKey("CustomAccentColor")) {
    // Explicit user preference: use their exact selected color
    return col;
  }

  // System theme: dynamically tune vibrancy and contrast against the sidebar
  // canvas
  KColorScheme windowScheme(QPalette::Active, KColorScheme::Window);
  QColor bg = windowScheme.background(KColorScheme::NormalBackground).color();
  bool isDark = bg.lightnessF() < 0.5;

  float h = 0.0f, s = 0.0f, l = 0.0f;
  col.getHslF(&h, &s, &l);
  if (h < 0.0f)
    h = 0.57f; // Default cyan/blue hue if achromatic

  if (isDark) {
    if (l < 0.65f) {
      l = std::clamp(l * 1.5f, 0.68f, 0.85f);
      s = std::clamp(s * 1.1f, 0.70f, 1.0f);
      col.setHslF(h, s, l);
    }
  } else {
    if (l > 0.40f) {
      l = std::clamp(l * 0.7f, 0.25f, 0.38f);
      s = std::clamp(s * 1.1f, 0.70f, 1.0f);
      col.setHslF(h, s, l);
    }
  }
  return col;
}

void WorklaneSidebar::updateAccentColor() {
  if (m_tree) {
    m_tree->viewport()->update();
  }
  update();
}

QString WorklaneSidebar::formatPwd(const QString &fullPath) const {
  if (fullPath.isEmpty())
    return QStringLiteral("~");
  QString home = QDir::homePath();
  if (fullPath == home)
    return QStringLiteral("~");
  if (fullPath.startsWith(home + QLatin1Char('/'))) {
    return QStringLiteral("~") + fullPath.mid(home.length());
  }
  return fullPath;
}

void WorklaneSidebar::refreshTree() {
  if (m_updating || !m_mainWindow || !isVisible() || m_isDragging)
    return;
  m_updating = true;

  // Prune any dead pointers from alert list
  m_alertSurfaces.removeIf(
      [](const QPointer<TerminalSurface> &ptr) { return ptr.isNull(); });

  // Save vertical scrollbar position
  int savedScroll =
      m_tree->verticalScrollBar() ? m_tree->verticalScrollBar()->value() : 0;

  m_tree->clear();

  int tabCount = m_mainWindow->tabCount();
  for (int i = 0; i < tabCount; ++i) {
    WorklaneView *wl = m_mainWindow->worklaneAt(i);
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

void WorklaneSidebar::populateTab(int tabIndex, WorklaneView *worklane) {
  auto *tabItem = new QTreeWidgetItem(m_tree);
  tabItem->setData(0, Qt::UserRole, 1); // Group

  QString proc = worklane->activeSurface()
                     ? worklane->activeSurface()->processName()
                     : QString();
  QString semanticTitle = worklane->activeTitle().trimmed();
  if (!proc.isEmpty() &&
      (semanticTitle.isEmpty() || semanticTitle == QStringLiteral("~") ||
       semanticTitle == QStringLiteral("Terminal"))) {
    semanticTitle = proc;
  } else if (semanticTitle.isEmpty()) {
    semanticTitle = i18n("Tab %1", tabIndex + 1);
  }

  tabItem->setData(0, Qt::DisplayRole, semanticTitle);
  tabItem->setData(0, Qt::UserRole + 1, worklane->paneCount());
  tabItem->setData(0, Qt::UserRole + 2,
                   (worklane == m_mainWindow->currentWorklane()));
  tabItem->setData(0, Qt::UserRole + 10, tabIndex);

  int colCount = worklane->columnCount();

  // Flatten child panes inside this tab container
  QList<TerminalSurface *> tabSurfaces;
  QList<QString> tabColTags;

  for (int c = 0; c < colCount; ++c) {
    int rCount = worklane->rowCount(c);
    for (int r = 0; r < rCount; ++r) {
      TerminalSurface *s = worklane->surfaceAt(c, r);
      if (!s)
        continue;
      tabSurfaces.append(s);

      QString colTag;
      if (colCount > 1) {
        if (rCount > 1) {
          colTag = QStringLiteral("Col %1/%2 · %3/%4")
                       .arg(c + 1)
                       .arg(colCount)
                       .arg(r + 1)
                       .arg(rCount);
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
    TerminalSurface *s = tabSurfaces[i];
    auto *laneItem = new QTreeWidgetItem(tabItem);
    laneItem->setData(0, Qt::UserRole, 2); // Lane Row

    QString pName = s->processName();
    QString sTitle = s->title().trimmed();
    if (!pName.isEmpty() &&
        (sTitle.isEmpty() || sTitle == QStringLiteral("~") ||
         sTitle == QStringLiteral("Terminal"))) {
      sTitle = pName;
    } else if (sTitle.isEmpty()) {
      sTitle = i18n("Terminal");
    }

    laneItem->setData(0, Qt::DisplayRole, sTitle);
    laneItem->setData(0, Qt::UserRole + 1,
                      formatPwd(s->currentWorkingDirectory()));
    laneItem->setData(0, Qt::UserRole + 2, tabColTags[i]);

    bool isActive = (s == m_mainWindow->activeSurface());
    laneItem->setData(0, Qt::UserRole + 3, isActive);

    bool alert = m_alertSurfaces.contains(s);
    laneItem->setData(0, Qt::UserRole + 4, alert);

    bool isLast = (i == tabSurfaces.size() - 1);
    laneItem->setData(0, Qt::UserRole + 5, isLast);

    laneItem->setData(0, Qt::UserRole + 10, tabIndex);
    laneItem->setData(0, Qt::UserRole + 11,
                      QVariant::fromValue(QPointer<TerminalSurface>(s)));
  }
}

void WorklaneSidebar::onItemClicked(QTreeWidgetItem *item, int column) {
  Q_UNUSED(column);
  if (!item || !m_mainWindow)
    return;

  int itemType = item->data(0, Qt::UserRole).toInt();
  int tabIndex = item->data(0, Qt::UserRole + 10).toInt();

  m_mainWindow->setCurrentTabIndex(tabIndex);

  if (itemType == 2) {
    auto surfacePtr =
        item->data(0, Qt::UserRole + 11).value<QPointer<TerminalSurface>>();
    if (surfacePtr && !surfacePtr.isNull()) {
      clearAlert(surfacePtr.data());
      WorklaneView *wl = m_mainWindow->currentWorklane();
      if (wl) {
        wl->ensureSurfaceVisible(surfacePtr.data());
      }
      surfacePtr->setFocus();
    }
  }

  // Schedule queued refresh rather than clear() synchronously inside click
  // event dispatch
  QMetaObject::invokeMethod(this, &WorklaneSidebar::refreshTree,
                            Qt::QueuedConnection);
}
