// ModelViewerTreeWidget.h
#ifndef MODELVIEWERTREEWIDGET_H
#define MODELVIEWERTREEWIDGET_H

#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QMenu>
#include <QAction>
#include <QContextMenuEvent>
#include <QPoint>

class ModelViewerTreeWidget : public QTreeWidget
{
    Q_OBJECT

public:
    explicit ModelViewerTreeWidget(QWidget* parent = nullptr);
    ~ModelViewerTreeWidget();

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;

private slots:
    void expandAllChildren();
    void collapseAllChildren();
    void showContextMenu(const QPoint& pos);

private:
    void createContextMenu();
    bool isParentNode(QTreeWidgetItem* item);
    void expandItemRecursively(QTreeWidgetItem* item);
    void collapseItemRecursively(QTreeWidgetItem* item);

    QMenu* m_contextMenu;
    QAction* m_expandAction;
    QAction* m_collapseAction;
    QTreeWidgetItem* m_contextMenuItem;
};

#endif // MODELVIEWERTREEWIDGET_H
