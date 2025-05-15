#pragma once

#include <QMainWindow>
#include "ModelViewerWidget.h"
#include <QTreeWidget>

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
};
