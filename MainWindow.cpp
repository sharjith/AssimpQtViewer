#include "MainWindow.h"
#include <QSplitter>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QLineEdit>
#include <QMenuBar>
#include <QProgressBar>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <QMetaObject>
#include <QtConcurrent>

#include "QtAssimpProgressHandler.h"

MainWindow::MainWindow(QWidget* parent)
	: QMainWindow(parent), _viewer(new ModelViewerWidget(this)), _treeWidget(new QTreeWidget(this)) {

	auto* delegate = new HighlightDelegate(_treeWidget);
	_treeWidget->setItemDelegate(delegate);
	_highlightDelegate = delegate; // store as member if needed
	_treeWidget->setHeaderHidden(true);
	connect(_treeWidget, &QTreeWidget::itemClicked, this, &MainWindow::onTreeItemClicked);

	QVBoxLayout* layout = new QVBoxLayout;
	_searchBox = new QLineEdit(this);
	_searchBox->setPlaceholderText("Search...");
	_searchBox->setClearButtonEnabled(true);
	_searchBox->hide(); // Hide the search box initially

	layout->addWidget(_searchBox);
	layout->addWidget(_treeWidget); // existing QTreeWidget

	QWidget* treePanel = new QWidget;
	treePanel->setLayout(layout);

	QWidget* viewerContainer = new QWidget;
	QVBoxLayout* viewerLayout = new QVBoxLayout(viewerContainer);
	viewerLayout->setContentsMargins(0, 0, 0, 0);
	viewerLayout->setSpacing(0);

	_progressBar = new QProgressBar;
	_progressBar->setObjectName("progressBar");
	_progressBar->setFixedHeight(10);
	_progressBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	_progressBar->setTextVisible(false);
	_progressBar->setRange(0, 100);  // Default range
	_progressBar->setValue(0);  // Default value
	// Minimal styling
	_progressBar->setStyleSheet(R"(
    QProgressBar { border: 0; background-color: transparent; }
    QProgressBar::chunk { background-color: #0078d7; })");

	viewerLayout->addWidget(_viewer);
	viewerLayout->addWidget(_progressBar);
	viewerLayout->setStretch(0, 1);
	viewerLayout->setStretch(1, 0);

	QSplitter* splitter = new QSplitter(this);
	splitter->addWidget(treePanel);
	splitter->addWidget(viewerContainer);
	splitter->setStretchFactor(1, 1);
	setCentralWidget(splitter);

	QMenu* fileMenu = menuBar()->addMenu("File");
	QAction* openAct = fileMenu->addAction("Open");
	connect(openAct, &QAction::triggered, [this]() {
		QString filePath = QFileDialog::getOpenFileName(this, "Open Model", "", "Model Files (*.obj *.fbx *.dae *.3ds *.stl *.ply *.gltf)");
		if (!filePath.isEmpty()) loadModel(filePath);
		});

	connect(_searchBox, &QLineEdit::textChanged, this, &MainWindow::filterTree);
}

void MainWindow::loadModel(const QString& path) {
	_progressBar->setValue(0);
	_progressBar->setRange(0, 100);
	_progressBar->setVisible(true);

	auto* handler = new QtAssimpProgressHandler(this);
	_viewer->getImporter()->SetProgressHandler(handler);

	connect(handler, &QtAssimpProgressHandler::progressChanged, this, [this](int percent) {
		_progressBar->setValue(percent);
		});

	_progressBar->setVisible(false);

	_treeWidget->clear();
	_viewer->loadModel(path);

	const aiScene* scene = _viewer->getScene();
	if (scene) {
		populateTree(scene);
		_searchBox->show();
		_treeWidget->expandAll();
	}
	
	_progressBar->setValue(100);
	_progressBar->setVisible(false);
}



void MainWindow::populateTree(const aiScene* scene) {
	if (!scene || !scene->mRootNode) return;
	std::function<void(aiNode*, QTreeWidgetItem*)> recurse =
		[&](aiNode* node, QTreeWidgetItem* parentItem) {
		QTreeWidgetItem* item = new QTreeWidgetItem();
		item->setText(0, QString::fromUtf8(node->mName.C_Str()));
		item->setData(0, Qt::UserRole, QVariant::fromValue<void*>(node));
		if (parentItem) parentItem->addChild(item);
		else _treeWidget->addTopLevelItem(item);
		for (unsigned i = 0; i < node->mNumChildren; ++i)
			recurse(node->mChildren[i], item);
		};
	recurse(scene->mRootNode, nullptr);
}

void MainWindow::onTreeItemClicked(QTreeWidgetItem* item, int column) {
	aiNode* node = static_cast<aiNode*>(item->data(0, Qt::UserRole).value<void*>());
	_viewer->highlightNode(node);
}


void MainWindow::filterTree(const QString& text) {

	if (_highlightDelegate)
		_highlightDelegate->setPattern(text);
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

	QTreeWidgetItemIterator it(_treeWidget);
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
		_treeWidget->collapseAll();  // Optional: collapse everything when cleared
	}

	_treeWidget->viewport()->update();
}

