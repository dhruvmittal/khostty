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

namespace {
enum ItemType {
  TypeTab = 1,
  TypeLane = 2,
};

enum ItemRole {
  RoleItemType = Qt::UserRole,
  RoleTabIndex,
  RoleSurface,
  RolePaneCount,
  RoleIsCurrentTab,
  RolePwd,
  RoleColTag,
  RoleIsActive,
  RoleHasAlert,
};

QString resolveSemanticTitle(TerminalSurface *surface, const QString &rawTitle,
                             const QString &fallback) {
  QString title = rawTitle.trimmed();
  const QString proc = surface ? surface->processName() : QString();
  if (!proc.isEmpty() &&
      (title.isEmpty() || title == QStringLiteral("~") ||
       title == QStringLiteral("Terminal"))) {
    return proc;
  }
  return title.isEmpty() ? fallback : title;
}
} // namespace

class SidebarTreeWidget : public QTreeWidget {
public:
  explicit SidebarTreeWidget(WorklaneSidebar *sidebar,
                             QWidget *parent = nullptr)
      : QTreeWidget(parent), m_sidebar(sidebar) {
    viewport()->setAcceptDrops(true);
  }

  bool isDragging() const { return m_isDraggingLocal; }
  int draggedType() const { return m_draggedType; }
  int draggedTabIndex() const { return m_draggedTabIndex; }

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

    int targetType = m_dropTargetItem->data(0, RoleItemType).toInt();
    int targetTab = m_dropTargetItem->data(0, RoleTabIndex).toInt();
    TerminalSurface *targetSurface = nullptr;
    if (targetType == TypeLane) {
      targetSurface = m_dropTargetItem->data(0, RoleSurface)
                          .value<QPointer<TerminalSurface>>()
                          .data();
    }

    int srcType = m_draggedType;
    int srcTab = m_draggedTabIndex;
    TerminalSurface *srcSurface = m_draggedSurface.data();
    bool isStack = m_dropIsStack;
    bool insertAfter = m_dropInsertAfter;

    clearDropIndicator();
    event->acceptProposedAction();

    MainWindow *mw = m_sidebar->mainWindow();
    if (srcType == TypeTab) {
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
    } else if (srcType == TypeLane && srcSurface) {
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

    int targetTab = targetItem->data(0, RoleTabIndex).toInt();
    int srcType = m_draggedType;
    int srcTab = m_draggedTabIndex;

    if (srcType == TypeLane) {
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
    m_dropIsStack = (srcType == TypeLane && ctrlPressed);
    m_dropInsertAfter = inBottomHalf;

    viewport()->update();
    return true;
  }

  void startSidebarDrag(QTreeWidgetItem *item) {
    m_draggedType = item->data(0, RoleItemType).toInt();
    m_draggedTabIndex = item->data(0, RoleTabIndex).toInt();
    m_draggedSurface = (m_draggedType == TypeLane)
                           ? item->data(0, RoleSurface)
                                 .value<QPointer<TerminalSurface>>()
                           : QPointer<TerminalSurface>();

    m_isDraggingLocal = true;

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
    m_draggedSurface.clear();
    clearDropIndicator();
    if (m_sidebar) {
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
  QPointer<TerminalSurface> m_draggedSurface;

  QTreeWidgetItem *m_dropTargetItem = nullptr;
  bool m_dropIsStack = false;
  bool m_dropInsertAfter = false;
};

class WorklaneItemDelegate : public QStyledItemDelegate {
public:
  WorklaneItemDelegate(WorklaneSidebar *sidebar, SidebarTreeWidget *tree,
                       QObject *parent = nullptr)
      : QStyledItemDelegate(parent), m_sidebar(sidebar), m_tree(tree) {}

  QSize sizeHint(const QStyleOptionViewItem &option,
                 const QModelIndex &index) const override {
    int itemType = index.data(RoleItemType).toInt();
    int fontH = option.fontMetrics.height();
    if (itemType == TypeTab) { // Clean section header with top spacing
      return QSize(option.rect.width(), std::max(28, fontH + 14));
    } else if (itemType == TypeLane) { // Lane card row
      return QSize(option.rect.width(), std::max(46, fontH * 2 + 14));
    }
    return QStyledItemDelegate::sizeHint(option, index);
  }

  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override {
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    if (m_sidebar && m_tree && m_tree->isDragging() &&
        m_tree->draggedType() == TypeLane) {
      int itemTab = index.data(RoleTabIndex).toInt();
      int srcTab = m_tree->draggedTabIndex();
      if (auto *mw = m_sidebar->mainWindow()) {
        WorklaneView *srcWl = mw->worklaneAt(srcTab);
        WorklaneView *thisWl = mw->worklaneAt(itemTab);
        if (srcWl && thisWl && !thisWl->canAcceptPaneFrom(srcWl)) {
          painter->setOpacity(0.35);
        }
      }
    }

    int itemType = index.data(RoleItemType).toInt();
    QRect rect = option.rect;

    KColorScheme viewScheme(QPalette::Active, KColorScheme::View);

    if (itemType == TypeTab) {
      QString tabName = index.data(Qt::DisplayRole).toString();
      int paneCount = index.data(RolePaneCount).toInt();
      bool isCurrentTab = index.data(RoleIsCurrentTab).toBool();

      QRect headerRect = rect.adjusted(10, 8, -10, 0);

      QColor vibrantAccent = m_sidebar
                                 ? m_sidebar->vibrantAccentColor()
                                 : option.palette.color(QPalette::Highlight);

      QString badge = i18n("%1 panes", paneCount);
      int badgeWidth = option.fontMetrics.horizontalAdvance(badge) + 16;

      int leftOffset = 0;
      if (isCurrentTab) {
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

    } else if (itemType == TypeLane) {
      QString title = index.data(Qt::DisplayRole).toString();
      QString pwd = index.data(RolePwd).toString();
      QString colTag = index.data(RoleColTag).toString();
      bool isActive = index.data(RoleIsActive).toBool();
      bool hasAlert = index.data(RoleHasAlert).toBool();

      QRect cardRect = rect.adjusted(8, 2, -8, -2);

      QColor vibrantAccent = m_sidebar
                                 ? m_sidebar->accentColor()
                                 : option.palette.color(QPalette::Highlight);

      QColor cardBg = option.palette.color(QPalette::Window);
      QColor borderCol = option.palette.color(QPalette::Mid);

      if (isActive) {
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

      bool lightActiveCard = (cardBg.lightnessF() > 0.60);
      QColor activeFg = lightActiveCard
                            ? Qt::black
                            : option.palette.color(QPalette::HighlightedText);
      QColor activePwdCol =
          lightActiveCard ? QColor(0, 0, 0, 190) : QColor(255, 255, 255, 220);

      int dotR = 3;
      int dotX = cardRect.left() + 10;
      int dotY = cardRect.top() + (cardRect.height() / 2) - dotR;
      painter->setPen(Qt::NoPen);
      if (hasAlert) {
        painter->setBrush(QColor(235, 160, 20));
      } else if (isActive) {
        painter->setBrush(activeFg);
      } else {
        painter->setBrush(
            viewScheme.foreground(KColorScheme::InactiveText).color());
      }
      painter->drawEllipse(QPoint(dotX + dotR, dotY + dotR), dotR, dotR);

      int leftContentX = dotX + dotR * 2 + 8;

      int tagPillWidth = 0;
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

      QRect pwdRect(leftContentX, cardRect.top() + lineH + 4,
                    cardRect.width() - (leftContentX - cardRect.left()) - 6,
                    lineH);
      QFont monoFont = m_sidebar ? m_sidebar->monoFont()
                                 : QFontDatabase::systemFont(QFontDatabase::FixedFont);
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
  SidebarTreeWidget *m_tree = nullptr;
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
  m_tree->setItemDelegate(new WorklaneItemDelegate(this, m_tree, m_tree));
  m_tree->setStyleSheet(QStringLiteral(
      "QTreeWidget { border: none; background: transparent; padding: 4px; }"
      "QTreeWidget::item { padding: 0px; margin: 0px; }"));

  // Suppress Alt accelerator injection into shell paths and titles
  KAcceleratorManager::setNoAccel(m_tree);
  KAcceleratorManager::setNoAccel(this);

  connect(m_tree, &QTreeWidget::itemClicked, this,
          &WorklaneSidebar::onItemClicked);
  rootLayout->addWidget(m_tree, 1);

  refreshThemeCache();

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

void WorklaneSidebar::changeEvent(QEvent *event) {
  QWidget::changeEvent(event);
  switch (event->type()) {
  case QEvent::PaletteChange:
  case QEvent::ApplicationPaletteChange:
  case QEvent::FontChange:
  case QEvent::ApplicationFontChange:
    updateAccentColor();
    break;
  default:
    break;
  }
}

void WorklaneSidebar::scheduleRefresh() {
  if (!isVisible() || (m_tree && m_tree->isDragging()))
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

void WorklaneSidebar::refreshThemeCache() {
  KConfigGroup appCfg(KSharedConfig::openConfig(QStringLiteral("khosttyrc")),
                      QStringLiteral("Appearance"));
  const QString customHex = appCfg.readEntry("CustomAccentColor", QString());
  const QColor customCol(customHex);
  const bool hasValidCustom = !customHex.isEmpty() && customCol.isValid();

  if (hasValidCustom) {
    m_accentColor = customCol;
    m_vibrantAccentColor = customCol;
  } else {
    QColor col = palette().color(QPalette::Accent);
    if (!col.isValid() || col == palette().color(QPalette::Window)) {
      KColorScheme viewScheme(QPalette::Active, KColorScheme::View);
      col = viewScheme.decoration(KColorScheme::FocusColor).color();
    }
    if (!col.isValid() || col == palette().color(QPalette::Window)) {
      col = palette().color(QPalette::Highlight);
    }
    m_accentColor = col;

    KColorScheme windowScheme(QPalette::Active, KColorScheme::Window);
    const QColor bg =
        windowScheme.background(KColorScheme::NormalBackground).color();
    const bool isDark = bg.lightnessF() < 0.5;

    float h = 0.0f, s = 0.0f, l = 0.0f;
    col.getHslF(&h, &s, &l);
    if (h < 0.0f) {
      h = 0.57f;
    }

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
    m_vibrantAccentColor = col;
  }

  m_monoFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
  m_monoFont.setPointSize(std::max(8, font().pointSize() - 1));
}

void WorklaneSidebar::updateAccentColor() {
  refreshThemeCache();
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
  if (m_updating || !m_mainWindow || !isVisible() ||
      (m_tree && m_tree->isDragging()))
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
  tabItem->setData(0, RoleItemType, TypeTab);

  const QString semanticTitle = resolveSemanticTitle(
      worklane->activeSurface(), worklane->activeTitle(),
      i18n("Tab %1", tabIndex + 1));

  tabItem->setData(0, Qt::DisplayRole, semanticTitle);
  tabItem->setData(0, RolePaneCount, worklane->paneCount());
  tabItem->setData(0, RoleIsCurrentTab,
                   (worklane == m_mainWindow->currentWorklane()));
  tabItem->setData(0, RoleTabIndex, tabIndex);

  const int colCount = worklane->columnCount();
  for (int c = 0; c < colCount; ++c) {
    const int rCount = worklane->rowCount(c);
    for (int r = 0; r < rCount; ++r) {
      TerminalSurface *s = worklane->surfaceAt(c, r);
      if (!s)
        continue;

      auto *laneItem = new QTreeWidgetItem(tabItem);
      laneItem->setData(0, RoleItemType, TypeLane);
      laneItem->setData(0, Qt::DisplayRole,
                        resolveSemanticTitle(s, s->title(), i18n("Terminal")));
      laneItem->setData(0, RolePwd, formatPwd(s->currentWorkingDirectory()));
      laneItem->setData(0, RoleColTag,
                        WorklaneView::formatPositionTag(c, colCount, r, rCount));
      laneItem->setData(0, RoleIsActive, (s == m_mainWindow->activeSurface()));
      laneItem->setData(0, RoleHasAlert, m_alertSurfaces.contains(s));
      laneItem->setData(0, RoleTabIndex, tabIndex);
      laneItem->setData(0, RoleSurface,
                        QVariant::fromValue(QPointer<TerminalSurface>(s)));
    }
  }
}

void WorklaneSidebar::onItemClicked(QTreeWidgetItem *item, int column) {
  Q_UNUSED(column);
  if (!item || !m_mainWindow)
    return;

  int itemType = item->data(0, RoleItemType).toInt();
  int tabIndex = item->data(0, RoleTabIndex).toInt();

  m_mainWindow->setCurrentTabIndex(tabIndex);

  if (itemType == TypeLane) {
    auto surfacePtr =
        item->data(0, RoleSurface).value<QPointer<TerminalSurface>>();
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
