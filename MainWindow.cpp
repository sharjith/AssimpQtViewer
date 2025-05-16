#include "MainWindow.h"
#include <QSplitter>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QLineEdit>
#include <QMenuBar>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), viewer(new ModelViewerWidget(this)), tree(new QTreeWidget(this)) {

    auto* delegate = new HighlightDelegate(tree);
    tree->setItemDelegate(delegate);
    m_highlightDelegate = delegate; // store as member if needed
    tree->setHeaderHidden(true);
    connect(tree, &QTreeWidget::itemClicked, this, &MainWindow::onTreeItemClicked);

    QVBoxLayout* layout = new QVBoxLayout;
    QLineEdit* searchBox = new QLineEdit(this);
    searchBox->setPlaceholderText("Search...");
	searchBox->setClearButtonEnabled(true);

    layout->addWidget(searchBox);
    layout->addWidget(tree); // existing QTreeWidget

    QWidget* treePanel = new QWidget;
    treePanel->setLayout(layout);

    QSplitter *splitter = new QSplitter(this);
    splitter->addWidget(treePanel);
    splitter->addWidget(viewer);
    splitter->setStretchFactor(1, 1);
    setCentralWidget(splitter);

    QMenu *fileMenu = menuBar()->addMenu("File");
    QAction *openAct = fileMenu->addAction("Open");
    connect(openAct, &QAction::triggered, [this]() {
        QString filePath = QFileDialog::getOpenFileName(this, "Open Model", "", "Model Files (*.obj *.fbx *.dae *.3ds *.stl *.ply *.gltf)");
        if (!filePath.isEmpty()) loadModel(filePath);
    });

    connect(searchBox, &QLineEdit::textChanged, this, &MainWindow::filterTree);
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


void MainWindow::filterTree(const QString& text) {

    if (m_highlightDelegate)
        m_highlightDelegate->setPattern(text);
    auto matches = [=](const QString& pattern, const QString& value) -> int {
        int score = 0;
        int patternIndex = 0;
        for (int i = 0; i < value.size(); ++i) {
            if (patternIndex < pattern.size() &&
                pattern[patternIndex].toLower() == value[i].toLower()) {
                ++score;
                ++patternIndex;
            }
        }
        return (patternIndex == pattern.size()) ? score : 0;
        };

    QTreeWidgetItemIterator it(tree);
    while (*it) {
        QTreeWidgetItem* item = *it;
        const QString itemText = item->text(0);
        int score = matches(text, itemText);
        bool match = (text.isEmpty() || score > 0);
        item->setHidden(!match);

        if (match) {
            // Expand all ancestors so this item is visible
            QTreeWidgetItem* parent = item->parent();
            while (parent) {
                parent->setExpanded(true);
                parent->setHidden(false);
                parent = parent->parent();
            }
        }

        ++it;
    }

    if (text.isEmpty()) {
        tree->collapseAll();  // Optional: collapse everything when cleared
    }

    tree->viewport()->update();
}

