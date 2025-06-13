#pragma once

#include "HighlightDelegate.h"
#include "ModelViewerWidget.h"
#include <QMainWindow>
#include <QProgressBar>
#include <QTreeWidget>
#include <QDragEnterEvent>
#include <QDropEvent>

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(QWidget *parent = nullptr);

public slots:
    void openFile(const QString& path);

private slots:    
    void onTreeItemClicked(QTreeWidgetItem* item, int column);
    void filterTree(const QString& text);
    void showFileReadingProgress(float percent);
    void updateRecentFilesMenu();
    void clearRecentFiles();

private:
    void selectTreeNodeFor(aiNode* node); // ← this will highlight the node
    void selectTreeMeshFor(int meshIndex);// ← this will highlight the mesh
    void loadModel(const QString& path);    
    void setProgressValue(const int& value);
    void populateTree(const aiScene* scene);
    void addToRecentFiles(const QString& filePath);
    
protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    
private: 
	bool m_modelLoaded = false;
    QString m_currentModelPath;
    QString m_lastOpenedDirectory;
    ModelViewerWidget* m_viewerWidget;
    QTreeWidget* m_treeWidget;
    std::unordered_map<aiNode*, QTreeWidgetItem*> m_nodeToItem;
    std::unordered_map<int, QTreeWidgetItem*> m_meshToItem;
    HighlightDelegate* m_highlightDelegate;
    QLineEdit* m_searchBox;
    QProgressBar* m_progressBar;
    aiNode* m_currentlySelectedNode = nullptr;

    static const int MaxRecentFiles = 15;
    QList<QString> recentFiles;
    QList<QAction*> recentFileActions;
    QMenu* recentFilesMenu;
    QAction* separatorAction;
};
