
// ModelViewerTreeWidget.cpp
#include "ModelViewerTreeWidget.h"
#include <QHeaderView>
#include <QDebug>

ModelViewerTreeWidget::ModelViewerTreeWidget(QWidget* parent)
    : QTreeWidget(parent)
    , m_contextMenu(nullptr)
    , m_expandAction(nullptr)
    , m_collapseAction(nullptr)
    , m_contextMenuItem(nullptr)
{
    // Setup tree widget
    setHeaderLabel("Model Hierarchy");
    setContextMenuPolicy(Qt::CustomContextMenu);
    setAlternatingRowColors(true);
    setSelectionBehavior(QAbstractItemView::SelectRows);

    // Create context menu
    createContextMenu();

    // Connect signals
    connect(this, &QTreeWidget::customContextMenuRequested,
        this, &ModelViewerTreeWidget::showContextMenu);

    // Note: setupSampleData() removed - use your existing tree population logic
}

ModelViewerTreeWidget::~ModelViewerTreeWidget()
{
    // Cleanup is handled by Qt's parent-child relationship
}

void ModelViewerTreeWidget::createContextMenu()
{
    m_contextMenu = new QMenu(this);

    // Create expand action
    m_expandAction = new QAction("Expand All Children", this);
    m_expandAction->setIcon(QIcon(":/icons/res/expandall.png")); 
    connect(m_expandAction, &QAction::triggered,
        this, &ModelViewerTreeWidget::expandAllChildren);

    // Create expand to first level action
    m_expandFirstLevelAction = new QAction("Expand to 1st Level", this);
    m_expandFirstLevelAction->setIcon(QIcon(":/icons/res/expand.png"));
    connect(m_expandFirstLevelAction, &QAction::triggered,
        this, &ModelViewerTreeWidget::expandToFirstLevel);

    // Create collapse action
    m_collapseAction = new QAction("Collapse All Children", this);
    m_collapseAction->setIcon(QIcon(":/icons/res/collapse.png"));
    connect(m_collapseAction, &QAction::triggered,
        this, &ModelViewerTreeWidget::collapseAllChildren);

    // Add actions to menu
    m_contextMenu->addAction(m_expandFirstLevelAction);
    m_contextMenu->addAction(m_expandAction);
    m_contextMenu->addSeparator();
    m_contextMenu->addAction(m_collapseAction);
}

void ModelViewerTreeWidget::contextMenuEvent(QContextMenuEvent* event)
{
    QTreeWidgetItem* item = itemAt(event->pos());

    // Only show context menu for parent nodes
    if (item && isParentNode(item)) {
        m_contextMenuItem = item;
        m_contextMenu->exec(event->globalPos());
    }
    else {
        QTreeWidget::contextMenuEvent(event);
    }
}

void ModelViewerTreeWidget::showContextMenu(const QPoint& pos)
{
    QTreeWidgetItem* item = itemAt(pos);

    // Only show context menu for parent nodes
    if (item && isParentNode(item)) {
        m_contextMenuItem = item;
        m_contextMenu->exec(mapToGlobal(pos));
    }
}

void ModelViewerTreeWidget::expandAllChildren()
{
    if (m_contextMenuItem) {
        expandItemRecursively(m_contextMenuItem);
    }
}

void ModelViewerTreeWidget::collapseAllChildren()
{
    if (m_contextMenuItem) {
        collapseItemRecursively(m_contextMenuItem);
    }
}

bool ModelViewerTreeWidget::isParentNode(QTreeWidgetItem* item)
{
    return item && item->childCount() > 0;
}

void ModelViewerTreeWidget::expandToFirstLevel()
{
    if (m_contextMenuItem) {
        // First expand the selected item itself
        expandItem(m_contextMenuItem);

        // Ensure all immediate children are visible but NOT expanded
        for (int i = 0; i < m_contextMenuItem->childCount(); ++i) {
            QTreeWidgetItem* child = m_contextMenuItem->child(i);
            // Make sure the child is collapsed (don't expand its children)
            collapseItem(child);
        }
    }
}


void ModelViewerTreeWidget::expandItemRecursively(QTreeWidgetItem* item)
{
    if (!item) return;

    // Expand this item
    expandItem(item);

    // Recursively expand all children
    for (int i = 0; i < item->childCount(); ++i) {
        expandItemRecursively(item->child(i));
    }
}

void ModelViewerTreeWidget::collapseItemRecursively(QTreeWidgetItem* item)
{
    if (!item) return;

    // Recursively collapse all children first
    for (int i = 0; i < item->childCount(); ++i) {
        collapseItemRecursively(item->child(i));
    }

    // Then collapse this item
    collapseItem(item);
}
