#pragma once

#include "HighlightDelegate.h"
#include "ModelViewerWidget.h"
#include <QMainWindow>
#include <QProgressBar>
#include <QTreeWidget>
#include <QDragEnterEvent>
#include <QDropEvent>

#include "ModelViewerTreeWidget.h"

enum class OpenModelBehavior
{
    Ask,
    ThisWindow,
    NewWindow
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);

public slots:
    void openFile(const QString &path);

private slots:
    void onSelectionChanged(const std::unordered_set<int> &selectedMeshIndices);

    void onItemVisibilityChanged(QTreeWidgetItem *item, int column);

    void onTreeSelectionChanged();

    void updateTreeItemVisibility(int meshIndex, bool visible);

    void updateAllTreeItemVisibility(const std::unordered_map<int, bool> &visibilityMap);

    void filterTree(const QString &text);

    void showFileReadingProgress(float percent);

    void updateRecentFilesMenu();

    void clearRecentFiles();

private:
    void selectTreeNodeFor(aiNode *node); // ← this will highlight the node
    void selectTreeMeshFor(int meshIndex); // ← this will highlight the mesh
    void setMeshVisibility(unsigned int meshIndex, bool isVisible);

    QTreeWidgetItem *findTreeItemByMeshIndex(int meshIndex);

    std::unordered_set<int> getMeshIndicesFromNode(aiNode *node);

    void updateChildItems(QTreeWidgetItem *parentItem, Qt::CheckState state);

    void updateParentItem(QTreeWidgetItem *childItem);

    void loadModel(const QString &path);

    void setProgressValue(const int &value);

    void populateTree(const aiScene *scene);

    void addCheckboxToItem(QTreeWidgetItem *item, bool checked = true);

    void addToRecentFiles(const QString &filePath);

    OpenModelBehavior openModelBehaviorSetting() const;

    OpenModelBehavior promptOpenModelBehavior();

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;

    void dropEvent(QDropEvent *event) override;

private:
    bool m_modelLoaded = false;
    QString m_currentModelPath;
    QString m_lastOpenedDirectory;
    ModelViewerWidget *m_viewerWidget;
    ModelViewerTreeWidget *m_treeWidget;
    std::unordered_map<aiNode *, QTreeWidgetItem *> m_nodeToItem;
    std::unordered_map<int, QTreeWidgetItem *> m_meshToItem;
    HighlightDelegate *m_highlightDelegate;
    QLineEdit *m_searchBox;
    QProgressBar *m_progressBar;
    aiNode *m_currentlySelectedNode = nullptr;

    static constexpr int MaxRecentFiles = 15;
    QList<QString> recentFiles;
    QList<QAction *> recentFileActions;
    QMenu *recentFilesMenu;
    QAction *separatorAction;
};
