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
	: QMainWindow(parent), m_viewerWidget(new ModelViewerWidget(this)), m_treeWidget(new QTreeWidget(this)) {

	auto* delegate = new HighlightDelegate(m_treeWidget);
	m_treeWidget->setItemDelegate(delegate);
	m_highlightDelegate = delegate; // store as member if needed
	m_treeWidget->setHeaderHidden(true);
	connect(m_treeWidget, &QTreeWidget::itemClicked, this, &MainWindow::onTreeItemClicked);

	QVBoxLayout* layout = new QVBoxLayout;
	m_searchBox = new QLineEdit(this);
	m_searchBox->setPlaceholderText("Search...");
	m_searchBox->setClearButtonEnabled(true);
	m_searchBox->hide(); // Hide the search box initially

	layout->addWidget(m_searchBox);
	layout->addWidget(m_treeWidget); // existing QTreeWidget

	QWidget* treePanel = new QWidget;
	treePanel->setLayout(layout);

	QWidget* viewerContainer = new QWidget;
	QVBoxLayout* viewerLayout = new QVBoxLayout(viewerContainer);
	viewerLayout->setContentsMargins(0, 0, 0, 0);
	viewerLayout->setSpacing(0);

	m_progressBar = new QProgressBar;
	m_progressBar->setObjectName("progressBar");
	m_progressBar->setFixedHeight(10);
	m_progressBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	m_progressBar->setTextVisible(false);
	m_progressBar->setRange(0, 100);  // Default range
	m_progressBar->setValue(0);  // Default value
	// Minimal styling
	m_progressBar->setStyleSheet(R"(
    QProgressBar { border: 0; background-color: transparent; }
    QProgressBar::chunk { background-color: #0078d7; })");

	viewerLayout->addWidget(m_viewerWidget);
	viewerLayout->addWidget(m_progressBar);
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

	QAction* exitAct = fileMenu->addAction("Exit");
	connect(exitAct, &QAction::triggered, this, &QWidget::close);

	connect(m_searchBox, &QLineEdit::textChanged, this, &MainWindow::filterTree);
	connect(m_viewerWidget, &ModelViewerWidget::nodePicked,
		this, &MainWindow::selectTreeNodeFor);

}

void MainWindow::loadModel(const QString& path) {
	m_progressBar->setValue(0);
	m_progressBar->setRange(0, 100);
	m_progressBar->setVisible(true);

	auto* handler = new QtAssimpProgressHandler(this);
	m_viewerWidget->getImporter()->SetProgressHandler(handler);

	connect(handler, &QtAssimpProgressHandler::progressChanged, this, [this](int percent) {
		m_progressBar->setValue(percent);
		});

	m_progressBar->setVisible(false);

	m_treeWidget->clear();
	m_viewerWidget->loadModel(path);

	const aiScene* scene = m_viewerWidget->getScene();
	if (scene) {
		populateTree(scene);
		m_searchBox->show();
		m_treeWidget->expandAll();
	}
	
	m_progressBar->setValue(100);
	m_progressBar->setVisible(false);
}



void MainWindow::populateTree(const aiScene* scene) {
	if (!scene || !scene->mRootNode) return;
	std::function<void(aiNode*, QTreeWidgetItem*)> recurse =
		[&](aiNode* node, QTreeWidgetItem* parentItem) {
		QTreeWidgetItem* item = new QTreeWidgetItem();
		item->setText(0, QString::fromUtf8(node->mName.C_Str()));
		item->setData(0, Qt::UserRole, QVariant::fromValue<void*>(node));
		m_nodeToItem[node] = item;
		if (parentItem) parentItem->addChild(item);
		else m_treeWidget->addTopLevelItem(item);
		for (unsigned i = 0; i < node->mNumChildren; ++i)
			recurse(node->mChildren[i], item);
		};
	recurse(scene->mRootNode, nullptr);
}

void MainWindow::onTreeItemClicked(QTreeWidgetItem* item, int column) {
	aiNode* node = static_cast<aiNode*>(item->data(0, Qt::UserRole).value<void*>());
	m_viewerWidget->highlightNode(node);
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

	QTreeWidgetItemIterator it(m_treeWidget);
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
		m_treeWidget->collapseAll();  // Optional: collapse everything when cleared
	}

	m_treeWidget->viewport()->update();
}

void MainWindow::selectTreeNodeFor(aiNode* node) {
	// If this node is already selected, deselect it
	if (m_currentlySelectedNode == node) {
		m_treeWidget->setCurrentItem(nullptr);
		m_currentlySelectedNode = nullptr;
		return;
	}

	auto it = m_nodeToItem.find(node);
	if (it != m_nodeToItem.end()) {
		QTreeWidgetItem* item = it->second;
		m_treeWidget->setCurrentItem(item);
		m_treeWidget->scrollToItem(item);
		item->setSelected(true);
		m_currentlySelectedNode = node;
	}
}

