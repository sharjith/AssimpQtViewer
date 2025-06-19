
#include "ViewToolbar.h"
#include "FlyOutViewButton.h"
#include <QHBoxLayout>
#include <QToolButton>
#include <QMenu>
#include <QAction>

ViewToolbar::ViewToolbar(QWidget* parent)
    : QWidget(parent)
{
    setStyleSheet("background: rgba(255, 255, 255, 100); border: 1px solid gray; border-radius: 4px;");
    setFixedHeight(64);

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(6);

    auto createBtn = [this, layout](const QString& icon, const QString& tooltip, const QString& view) {
        QToolButton* btn = new QToolButton(this);
        btn->setIcon(QIcon(icon));
        btn->setIconSize(QSize(64, 64));
        btn->setToolTip(tooltip);
        btn->setAutoRaise(true);
        layout->addWidget(btn);
        connect(btn, &QToolButton::clicked, this, [this, view]() { emit viewSelected(view); });
    };

    createBtn(":/icons/res/top.png", "Top View", "Top");
    createBtn(":/icons/res/front.png", "Front View", "Front");
    createBtn(":/icons/res/left.png", "Left View", "Left");
    createBtn(":/icons/res/bottom.png", "Bottom View", "Bottom");
    createBtn(":/icons/res/back.png", "Rear View", "Rear");
    createBtn(":/icons/res/right.png", "Right View", "Right");

    m_toolButtonIsometricView = new FlyOutViewButton(this);
    m_toolButtonIsometricView->setIcon(QIcon(":/icons/res/isometric.png"));
    m_toolButtonIsometricView->setIconSize(QSize(64, 64));
    m_toolButtonIsometricView->setToolTip("Axonometric View");
    m_toolButtonIsometricView->setPopupMode(QToolButton::DelayedPopup);
    m_toolButtonIsometricView->setAutoRaise(true);
    layout->addWidget(m_toolButtonIsometricView);

    QMenu* axoMenu = new QMenu;
    axoMenu->setStyleSheet(
        "QMenu {"
        "    background-color: rgba(255, 255, 255, 100);"
        "    border: 1px solid gray;"
        "    border-radius: 4px;"
        "    padding: 2px;"
        "    icon-size: 42px;"
        "}"
        "QMenu::item {"
        "    background: transparent;"
        "    background-color: #f0f0f0;"
        "    border: 1px solid #c0c0c0;"
        "    border-radius: 4px;"
        "    padding: 5px 8px;"
        "    margin: 3px;"
        "    min-width: 120px;"
        "    min-height: 30px;"
        "    font-weight: normal;"
        "    color: black;"
        "}"
        "QMenu::item:selected {"
        "    background-color: #e0e0ff;"
        "    border: 1px solid #a0a0ff;"
        "    color: black;"
        "}"
        "QMenu::item:pressed {"
        "    background-color: #d0d0ff;"
        "    border: 1px solid #8080ff;"
        "    color: black;"
        "}"
        "QMenu::icon {"
        "    padding-left: 10px;"
        "    padding-right: 8px;"
        "}"
        "QMenu::separator {"
        "    height: 1px;"
        "    background-color: #c0c0c0;"
        "    margin: 4px 8px;"
        "}"
    );
    QAction* iso = axoMenu->addAction(QIcon(":/icons/res/isometric.png"), "Isometric");
    QAction* dim = axoMenu->addAction(QIcon(":/icons/res/dimetric.png"), "Dimetric");
    QAction* tri = axoMenu->addAction(QIcon(":/icons/res/trimetric.png"), "Trimetric");

    connect(iso, &QAction::triggered, this, [this]() { emit axonometricSelected("Isometric"); });
    connect(dim, &QAction::triggered, this, [this]() { emit axonometricSelected("Dimetric"); });
    connect(tri, &QAction::triggered, this, [this]() { emit axonometricSelected("Trimetric"); });

    m_toolButtonIsometricView->setMenu(axoMenu);
    m_toolButtonIsometricView->setDefaultAction(iso);

    QToolButton* fitBtn = new QToolButton(this);
    fitBtn->setIcon(QIcon(":/icons/res/fit-all.png"));
    fitBtn->setIconSize(QSize(64, 64));
    fitBtn->setToolTip("Fit All");
    fitBtn->setAutoRaise(true);
    layout->addWidget(fitBtn);
    connect(fitBtn, &QToolButton::clicked, this, [this]() { emit fitToViewRequested(); });

    QToolButton* multiBtn = new QToolButton(this);
    multiBtn->setIcon(QIcon(":/icons/res/multiview.png"));
    multiBtn->setIconSize(QSize(64, 64));
    multiBtn->setToolTip("Toggle Multi-View");
    multiBtn->setCheckable(true);
    multiBtn->setAutoRaise(true);
    layout->addWidget(multiBtn);
    connect(multiBtn, &QToolButton::toggled, this, [this](bool checked) { emit multiViewToggled(checked); });

    QToolButton* projToggleButton = new QToolButton(this);
    projToggleButton->setCheckable(true);
    projToggleButton->setIcon(QIcon(":/icons/res/Perspective.png"));
    projToggleButton->setIconSize(QSize(64, 64));
    projToggleButton->setToolTip("Toggle Projection");
    layout->addWidget(projToggleButton);

    connect(projToggleButton, &QToolButton::toggled, this, [this, projToggleButton](bool checked) {
        if (checked) {
            projToggleButton->setIcon(QIcon(":/icons/res/Ortho.png"));
            projToggleButton->setToolTip("Switch to Perspective");
        } else {
            projToggleButton->setIcon(QIcon(":/icons/res/Perspective.png"));
            projToggleButton->setToolTip("Switch to Orthographic");
        }
        emit projectionToggled(checked);
    });

    m_toolbarAnimation = new QPropertyAnimation(this, "geometry", this);
    m_toolbarAnimation->setDuration(300);
    m_toolbarAnimation->setEasingCurve(QEasingCurve::OutCubic);
}

void ViewToolbar::showAnimated() {
    if (m_toolbarAnimation->state() == QAbstractAnimation::Running)
        m_toolbarAnimation->stop();
    m_toolbarAnimation->setStartValue(geometry());
    m_toolbarAnimation->setEndValue(m_visibleRect);
    m_toolbarAnimation->start();
}

void ViewToolbar::hideAnimated() {
    if (m_toolbarAnimation->state() == QAbstractAnimation::Running)
        m_toolbarAnimation->stop();
    m_toolbarAnimation->setStartValue(geometry());
    m_toolbarAnimation->setEndValue(m_hiddenRect);
    m_toolbarAnimation->start();
}

void ViewToolbar::reposition(int widgetWidth, int widgetHeight) {
    adjustSize();
    QSize sz = size();
    int x = (widgetWidth - sz.width()) / 2;
    int y = widgetHeight - sz.height() - 10;
    move(x, y);
    m_visibleRect = QRect(x, y, sz.width(), sz.height());
    m_hiddenRect = m_visibleRect.translated(0, 80);
}

QRect ViewToolbar::visibleRect() const { return m_visibleRect; }
QRect ViewToolbar::hiddenRect() const { return m_hiddenRect; }

bool ViewToolbar::isFlyoutMenuVisible() const
{
    return m_toolButtonIsometricView &&
        m_toolButtonIsometricView->menu() &&
        m_toolButtonIsometricView->menu()->isVisible();
}


#include <QPainter>

void ViewToolbar::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QRect r = rect();
    QColor bg(255, 255, 255, 180);
    QColor border(100, 100, 100, 160);

    // Draw rounded rectangle background
    painter.setBrush(bg);
    painter.setPen(QPen(border, 1));
    painter.drawRoundedRect(r.adjusted(0, 0, -1, -1), 6, 6);

    QWidget::paintEvent(event); // Optional, not strictly needed here
}
