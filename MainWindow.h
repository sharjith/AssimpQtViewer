#pragma once

#include <QMainWindow>
#include "ModelViewerWidget.h"
#include <QTreeWidget>
#include <QProgressBar>
#include "HighlightDelegate.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(QWidget *parent = nullptr);

private slots:
    void onTreeItemClicked(QTreeWidgetItem* item, int column);
    void filterTree(const QString& text);

private:
    void selectTreeNodeFor(aiNode* node); // ← this will highlight the item
    void loadModel(const QString& path);
    void populateTree(const aiScene* scene);

private:
    ModelViewerWidget* m_viewerWidget;
    QTreeWidget* m_treeWidget;
    std::unordered_map<aiNode*, QTreeWidgetItem*> m_nodeToItem;
    HighlightDelegate* m_highlightDelegate;
    QLineEdit* m_searchBox;
    QProgressBar* m_progressBar;
    aiNode* m_currentlySelectedNode = nullptr;
};
