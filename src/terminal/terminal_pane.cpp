#include "terminal/terminal_pane.h"
#include "terminal/terminal_surface.h"

#include <QPainter>
#include <QMouseEvent>
#include <QIcon>
#include <QApplication>
#include <KLocalizedString>

TerminalPane::TerminalPane(TerminalSurface* surface, QWidget* parent)
    : QFrame(parent)
    , m_surface(surface)
{
    setFrameShape(QFrame::NoFrame);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumSize(200, 80);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // Header bar (Window Chrome)
    m_header = new QWidget(this);
    m_header->setFixedHeight(26);
    m_header->setCursor(Qt::ArrowCursor);

    auto* headerLayout = new QHBoxLayout(m_header);
    headerLayout->setContentsMargins(8, 2, 6, 2);
    headerLayout->setSpacing(6);

    // Focus indicator dot
    m_dotIndicator = new QLabel(QStringLiteral("●"), m_header);
    m_dotIndicator->setFixedWidth(12);

    // Title label
    m_titleLabel = new QLabel(i18n("Terminal"), m_header);
    m_titleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    // Actions on header
    m_splitRightBtn = new QToolButton(m_header);
    m_splitRightBtn->setIcon(QIcon::fromTheme(QStringLiteral("view-split-left-right"), QIcon::fromTheme(QStringLiteral("list-add"))));
    m_splitRightBtn->setToolTip(i18n("Split Right (New Column, Ctrl+Shift+O)"));
    m_splitRightBtn->setAutoRaise(true);
    m_splitRightBtn->setFixedSize(20, 20);
    m_splitRightBtn->setIconSize(QSize(14, 14));
    connect(m_splitRightBtn, &QToolButton::clicked, this, &TerminalPane::splitRightRequested);

    m_splitDownBtn = new QToolButton(m_header);
    m_splitDownBtn->setIcon(QIcon::fromTheme(QStringLiteral("view-split-top-bottom"), QIcon::fromTheme(QStringLiteral("list-add"))));
    m_splitDownBtn->setToolTip(i18n("Split Down (Ctrl+Shift+E)"));
    m_splitDownBtn->setAutoRaise(true);
    m_splitDownBtn->setFixedSize(20, 20);
    m_splitDownBtn->setIconSize(QSize(14, 14));
    connect(m_splitDownBtn, &QToolButton::clicked, this, &TerminalPane::splitDownRequested);

    m_closeBtn = new QToolButton(m_header);
    m_closeBtn->setIcon(QIcon::fromTheme(QStringLiteral("window-close"), QIcon::fromTheme(QStringLiteral("application-exit"))));
    m_closeBtn->setToolTip(i18n("Close Split (Ctrl+Shift+W)"));
    m_closeBtn->setAutoRaise(true);
    m_closeBtn->setFixedSize(20, 20);
    m_closeBtn->setIconSize(QSize(14, 14));
    connect(m_closeBtn, &QToolButton::clicked, this, &TerminalPane::closeRequested);

    headerLayout->addWidget(m_dotIndicator);
    headerLayout->addWidget(m_titleLabel);
    headerLayout->addWidget(m_splitRightBtn);
    headerLayout->addWidget(m_splitDownBtn);
    headerLayout->addWidget(m_closeBtn);

    rootLayout->addWidget(m_header);

    // Terminal Surface
    if (m_surface) {
        m_surface->setParent(this);
        rootLayout->addWidget(m_surface, 1);
        m_surface->installEventFilter(this);
        connect(m_surface, &TerminalSurface::titleChanged, this, &TerminalPane::setTitle);
        if (!m_surface->title().isEmpty()) {
            setTitle(m_surface->title());
        }
    }

    updateHeaderStyle();
}

TerminalPane::~TerminalPane() = default;

void TerminalPane::setTitle(const QString& title) {
    m_title = title.isEmpty() ? i18n("Terminal") : title;
    m_titleLabel->setText(m_title);
    m_titleLabel->setToolTip(m_title);
}

void TerminalPane::setActive(bool active) {
    if (m_isActive == active) return;
    m_isActive = active;
    updateHeaderStyle();
    update();
}

void TerminalPane::updateHeaderStyle() {
    QColor accent = palette().color(QPalette::Active, QPalette::Highlight);
    QColor windowColor = palette().color(QPalette::Window);
    QColor textMuted = palette().color(QPalette::Disabled, QPalette::WindowText);
    QColor textBright = palette().color(QPalette::Active, QPalette::WindowText);

    if (m_isActive) {
        m_dotIndicator->setStyleSheet(QStringLiteral("color: %1; font-weight: bold; font-size: 11px;").arg(accent.name()));
        m_titleLabel->setStyleSheet(QStringLiteral("color: %1; font-weight: bold; font-size: 11px;").arg(textBright.name()));
        m_header->setStyleSheet(QStringLiteral("background-color: %1; border-bottom: 1px solid %2;")
            .arg(windowColor.lighter(108).name(), accent.name()));
    } else {
        m_dotIndicator->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;").arg(textMuted.name()));
        m_titleLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;").arg(textMuted.name()));
        m_header->setStyleSheet(QStringLiteral("background-color: %1; border-bottom: 1px solid %2;")
            .arg(windowColor.name(), windowColor.darker(115).name()));
    }
}

void TerminalPane::paintEvent(QPaintEvent* event) {
    QFrame::paintEvent(event);
    QPainter painter(this);
    if (m_isActive) {
        QColor accent = palette().color(QPalette::Active, QPalette::Highlight);
        QPen pen(accent, 2);
        painter.setPen(pen);
        painter.drawRect(rect().adjusted(1, 1, -1, -1));
    } else {
        QColor subtle = palette().color(QPalette::Window).darker(115);
        QPen pen(subtle, 1);
        painter.setPen(pen);
        painter.drawRect(rect().adjusted(0, 0, -1, -1));
    }
}

void TerminalPane::mousePressEvent(QMouseEvent* event) {
    QFrame::mousePressEvent(event);
    if (m_surface) {
        m_surface->setFocus();
    }
}

bool TerminalPane::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_surface) {
        if (event->type() == QEvent::FocusIn) {
            setActive(true);
        } else if (event->type() == QEvent::FocusOut) {
            // Keep active state if focus is moving to one of our header buttons
            if (!m_header->isAncestorOf(QApplication::focusWidget())) {
                setActive(false);
            }
        }
    }
    return QFrame::eventFilter(watched, event);
}
