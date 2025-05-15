#include "MainWindow.h"
#include <QSplitter>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QMenuBar>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), viewer(new ModelViewerWidget(this)), tree(new QTreeWidget(this)) {
    tree->setHeaderHidden(true);
    connect(tree, &QTreeWidget::itemClicked, this, &MainWindow::onTreeItemClicked);

    QSplitter *splitter = new QSplitter(this);
    splitter->addWidget(tree);
    splitter->addWidget(viewer);
    splitter->setStretchFactor(1, 1);
    setCentralWidget(splitter);

    QMenu *fileMenu = menuBar()->addMenu("File");
    QAction *openAct = fileMenu->addAction("Open");
    connect(openAct, &QAction::triggered, [this]() {
        QString filePath = QFileDialog::getOpenFileName(this, "Open Model", "", "Model Files (*.obj *.fbx *.dae *.3ds *.stl *.ply *.gltf)");
        if (!filePath.isEmpty()) loadModel(filePath);
    });
}

void MainWindow::loadModel(const QString &path) {
    viewer->loadModel(path);
    tree->clear();
    populateTree(viewer->getScene());
}

void MainWindow::populateTree(const aiScene *scene) {
    if (!scene || !scene->mRootNode) return;
    std::function<void(aiNode*, QTreeWidgetItem*)> recurse =
        [&](aiNode *node, QTreeWidgetItem *parentItem) {
            QTreeWidgetItem *item = new QTreeWidgetItem();
            item->setText(0, QString::fromUtf8(node->mName.C_Str()));
            item->setData(0, Qt::UserRole, QVariant::fromValue<void*>(node));
            if (parentItem) parentItem->addChild(item);
            else tree->addTopLevelItem(item);
            for (unsigned i = 0; i < node->mNumChildren; ++i)
                recurse(node->mChildren[i], item);
        };
    recurse(scene->mRootNode, nullptr);
}

void MainWindow::onTreeItemClicked(QTreeWidgetItem *item, int column) {
    aiNode *node = static_cast<aiNode*>(item->data(0, Qt::UserRole).value<void*>());
    viewer->highlightNode(node);
}
