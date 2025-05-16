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
private:
    ModelViewerWidget *_viewer;
    QTreeWidget *_treeWidget;
    void loadModel(const QString &path);
    void populateTree(const aiScene *scene);
private slots:
    void onTreeItemClicked(QTreeWidgetItem *item, int column);
    void filterTree(const QString& text);

private:
    HighlightDelegate* _highlightDelegate;
    QLineEdit* _searchBox;
    QProgressBar* _progressBar;
};
