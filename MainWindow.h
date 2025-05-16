#pragma once

#include <QMainWindow>
#include "ModelViewerWidget.h"
#include <QTreeWidget>
#include "HighlightDelegate.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(QWidget *parent = nullptr);
private:
    ModelViewerWidget *viewer;
    QTreeWidget *tree;
    void loadModel(const QString &path);
    void populateTree(const aiScene *scene);
private slots:
    void onTreeItemClicked(QTreeWidgetItem *item, int column);
    void filterTree(const QString& text);

private:
    HighlightDelegate* m_highlightDelegate;
};
