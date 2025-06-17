#include "MainWindow.h"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <QApplication>
#include <QFileDialog>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QMetaObject>
#include <QProgressBar>
#include <QSplitter>
#include <QtConcurrent>
#include <QVBoxLayout>
#include <QStatusBar>
#include <QCheckBox>
#include <QPushButton>
#include <QAbstractButton>

#include "AssimpProgressHandler.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_viewerWidget(new ModelViewerWidget(this)), m_treeWidget(new ModelViewerTreeWidget(this))
{
    auto *delegate = new HighlightDelegate(m_treeWidget);
    m_treeWidget->setItemDelegate(delegate);
    m_highlightDelegate = delegate; // store as member if needed
    m_treeWidget->setHeaderHidden(true);
    m_treeWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    connect(m_treeWidget, &QTreeWidget::itemChanged, this, &MainWindow::onItemVisibilityChanged);
    connect(m_treeWidget, &QTreeWidget::itemSelectionChanged, this, &MainWindow::onTreeSelectionChanged);

    QVBoxLayout *layout = new QVBoxLayout;
    m_searchBox = new QLineEdit(this);
    m_searchBox->setPlaceholderText("Search...");
    m_searchBox->setClearButtonEnabled(true);
    m_searchBox->hide(); // Hide the search box initially

    layout->addWidget(m_searchBox);
    layout->addWidget(m_treeWidget); // existing QTreeWidget

    QWidget *treePanel = new QWidget;
    treePanel->setLayout(layout);

    QWidget *viewerContainer = new QWidget;
    QVBoxLayout *viewerLayout = new QVBoxLayout(viewerContainer);
    viewerLayout->setContentsMargins(0, 0, 0, 0);
    viewerLayout->setSpacing(0);

    m_progressBar = new QProgressBar;
    m_progressBar->setObjectName("progressBar");
    m_progressBar->setFixedHeight(10);
    m_progressBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_progressBar->setTextVisible(false);
    m_progressBar->setRange(0, 100); // Default range
    m_progressBar->setValue(0); // Default value
    m_progressBar->setVisible(false); // Default value

    // Minimal styling
    m_progressBar->setStyleSheet(R"(
    QProgressBar { border: 0; background-color: transparent; }
    QProgressBar::chunk { background-color: #0078d7; })");

    viewerLayout->addWidget(m_viewerWidget);
    viewerLayout->addWidget(m_progressBar);
    viewerLayout->setStretch(0, 1);
    viewerLayout->setStretch(1, 0);

    QSplitter *splitter = new QSplitter(this);
    splitter->addWidget(treePanel);
    splitter->addWidget(viewerContainer);
    splitter->setStretchFactor(1, 1);
    setCentralWidget(splitter);

    QMenu *fileMenu = menuBar()->addMenu("File");
    QAction *openAct = fileMenu->addAction("Open");
    openAct->setShortcut(QKeySequence::Open);
    connect(openAct, &QAction::triggered, [this]() {
        QString filePath = QFileDialog::getOpenFileName(this, "Open Model", m_lastOpenedDirectory,
                                                        "Model Files (*.obj *.fbx *.dae *.3ds *.stl *.ply *.gltf)");
        if (!filePath.isEmpty())
        {
            openFile(filePath);
        }
    });


    recentFilesMenu = new QMenu(tr("Recent Files"), this);
    separatorAction = recentFilesMenu->addSeparator();
    // Connect the aboutToShow signal to updateRecentFiles method
    connect(recentFilesMenu, &QMenu::aboutToShow, this, &MainWindow::updateRecentFilesMenu);

    for (int i = 0; i < MaxRecentFiles; ++i)
    {
        QAction *action = new QAction(this);
        action->setVisible(false);
        connect(action, &QAction::triggered, this, &MainWindow::openRecentFile);
        recentFileActions.append(action);
        recentFilesMenu->addAction(action);
    }

    recentFilesMenu->addSeparator();
    recentFilesMenu->addAction(tr("Clear Recent Files"), this, &MainWindow::clearRecentFiles);

    fileMenu->addMenu(recentFilesMenu);

    // add aseparator before recent files
    fileMenu->addSeparator();

    updateRecentFilesMenu();

    QAction *exitAct = fileMenu->addAction("Exit");
    exitAct->setShortcut(QKeySequence::Quit);
    connect(exitAct, &QAction::triggered, this, &QWidget::close);

    QMenu *settingsMenu = menuBar()->addMenu("Settings");
    QAction *resetOpenBehavior = new QAction("Reset Open File Behavior", this);
    connect(resetOpenBehavior, &QAction::triggered, this, [this]() {
        QSettings().setValue("openModelBehavior", "Ask");
        QMessageBox::information(this, "Reset", "Open model behavior reset to Ask.");
    });
    settingsMenu->addAction(resetOpenBehavior);


    QMenu *helpMenu = menuBar()->addMenu("Help");
    QAction *aboutAct = helpMenu->addAction("About");
    connect(aboutAct, &QAction::triggered, this, [this]() {
        QMessageBox::about(this, "About Model Viewer", "This is a simple model viewer using Qt and Assimp.");
    });

    QAction *aboutQtAct = helpMenu->addAction("About Qt");
    connect(aboutQtAct, &QAction::triggered, this, &QApplication::aboutQt);


    connect(m_searchBox, &QLineEdit::textChanged, this, &MainWindow::filterTree);

    connect(m_viewerWidget, &ModelViewerWidget::nodePicked,
            this, &MainWindow::selectTreeNodeFor);
    connect(m_viewerWidget, &ModelViewerWidget::selectionChanged,
            this, &MainWindow::onSelectionChanged);

    connect(m_viewerWidget, &ModelViewerWidget::meshVisibilityChanged,
            this, &MainWindow::updateTreeItemVisibility);
    connect(m_viewerWidget, &ModelViewerWidget::allMeshVisibilityChanged,
            this, &MainWindow::updateAllTreeItemVisibility);

    setAcceptDrops(true);

    statusBar()->showMessage("Ready...", 0);
}

void MainWindow::updateRecentFilesMenu()
{
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
    recentFiles = settings.value("recentFiles").toStringList();

    int numRecentFiles = qMin(recentFiles.size(), MaxRecentFiles);

    for (int i = 0; i < numRecentFiles; ++i)
    {
        QString text = tr("&%1 %2").arg(i + 1).arg(QFileInfo(recentFiles[i]).fileName());
        recentFileActions[i]->setText(text);
        recentFileActions[i]->setData(recentFiles[i]);
        recentFileActions[i]->setVisible(true);
    }

    for (int i = numRecentFiles; i < MaxRecentFiles; ++i)
    {
        recentFileActions[i]->setVisible(false);
    }

    separatorAction->setVisible(numRecentFiles > 0);
}

void MainWindow::removeFromRecentFiles(const QString& filePath)
{
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
    QStringList files = settings.value("recentFiles").toStringList();
    files.removeAll(filePath); // Remove duplicates
    settings.setValue("recentFiles", files);
}

void MainWindow::addToRecentFiles(const QString &filePath)
{
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
    QStringList files = settings.value("recentFiles").toStringList();
    files.removeAll(filePath); // Remove duplicates
    files.prepend(filePath); // Add to top
    while (files.size() > MaxRecentFiles)
        files.removeLast();
    settings.setValue("recentFiles", files);
    updateRecentFilesMenu();
}

void MainWindow::clearRecentFiles()
{
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
    settings.remove("recentFiles");
    updateRecentFilesMenu();
}

OpenModelBehavior MainWindow::openModelBehaviorSetting() const
{
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
    QString value = settings.value("openModelBehavior", "Ask").toString();
    if (value == "ThisWindow") return OpenModelBehavior::ThisWindow;
    if (value == "NewWindow") return OpenModelBehavior::NewWindow;
    return OpenModelBehavior::Ask;
}

OpenModelBehavior MainWindow::promptOpenModelBehavior()
{
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("Open Model");
    msgBox.setText("A model is already loaded.");
    msgBox.setInformativeText("Do you want to open the new model in this window or a new one?");
    msgBox.setIcon(QMessageBox::Question);

    // Use standard buttons to avoid geometry issues
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::No);

    // Customize labels AFTER setting standard buttons
    msgBox.button(QMessageBox::Yes)->setText("This Window");
    msgBox.button(QMessageBox::No)->setText("New Window");

    QCheckBox *rememberCheck = new QCheckBox("Don't ask again");
    msgBox.setCheckBox(rememberCheck);

    // Force layout stabilization
    msgBox.adjustSize();
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents); // Flush pending layout events

    // Show dialog (no geometry warning)
    int ret = msgBox.exec();

    // Save setting if needed
    if (rememberCheck->isChecked())
    {
        QSettings().setValue("openModelBehavior",
                             (ret == QMessageBox::Yes) ? "ThisWindow" : "NewWindow");
    }

    return (ret == QMessageBox::Yes) ? OpenModelBehavior::ThisWindow : OpenModelBehavior::NewWindow;
}


void MainWindow::openFile(const QString &path)
{
    QFileInfo fi(path);
    if (!fi.exists() || !fi.isFile())
        return;

    QString absolutePath = fi.absoluteFilePath();

    m_lastOpenedDirectory = QFileInfo(path).absolutePath(); // Update last opened directory

    // Skip if already loaded
    if (absolutePath == m_currentModelPath)
    {
        statusBar()->showMessage("Model already loaded", 2000); // 2 sec
        return;
    }

    if (m_modelLoaded)
    {
        OpenModelBehavior behavior = openModelBehaviorSetting();

        if (behavior == OpenModelBehavior::Ask)
            behavior = promptOpenModelBehavior();

        if (behavior == OpenModelBehavior::NewWindow)
        {
            // Open in a new window
            QProcess::startDetached(QCoreApplication::applicationFilePath(), QStringList() << absolutePath);
            return;
        }
    }
	statusBar()->showMessage(QString("Loading model: %1").arg(fi.absoluteFilePath()), 0); // 2 sec
    // Load in this window
    loadModel(absolutePath);
	statusBar()->showMessage("Loaded model...", 2000); // 2 sec
    m_modelLoaded = true;
    m_currentModelPath = absolutePath; // Store current model
}


void MainWindow::loadModel(const QString &path)
{
    m_progressBar->setValue(0);
    m_progressBar->setRange(0, 100);
    m_progressBar->setVisible(true);
    update();

    auto *handler = new AssimpProgressHandler();
    m_viewerWidget->getImporter()->SetProgressHandler(handler);

    connect(handler, &AssimpProgressHandler::progressChanged, this, &MainWindow::showFileReadingProgress);


    m_treeWidget->clear();
    m_viewerWidget->loadModel(path);

    const aiScene *scene = m_viewerWidget->getScene();
    if (scene)
    {
        populateTree(scene);
        m_searchBox->show();
        m_treeWidget->expandAll();
        addToRecentFiles(path);
    }

    m_progressBar->setValue(100);
    m_progressBar->setVisible(false);

    disconnect(handler, SIGNAL(progressChanged(float)), this, SLOT(showFileReadingProgress(float)));
    handler = nullptr;
}

void MainWindow::showFileReadingProgress(float percent)
{
    MainWindow::setProgressValue((int) ((float) percent * 100.0f));
}

void MainWindow::openRecentFile()
{
    if (const QAction* action = qobject_cast<const QAction*>(sender()))
    {
        QString filePath = action->data().toString();
        if (!QFile::exists(filePath))
        {
            QMessageBox::StandardButton reply = QMessageBox::question(
                this,
                tr("File Not Found"),
                tr("The file '%1' no longer exists. Would you like to remove it from the recent files?").arg(filePath),
                QMessageBox::Yes | QMessageBox::No
            );

            if (reply == QMessageBox::Yes)
            {
                removeFromRecentFiles(filePath);
                updateRecentFilesMenu();
            }
            return;
        }

        QApplication::setOverrideCursor(Qt::WaitCursor);
        openFile(filePath);
        QApplication::restoreOverrideCursor();        
    }
}

void MainWindow::setProgressValue(const int &value)
{
    if (value == 0)
        m_progressBar->reset();
    else
        m_progressBar->setValue(value);
    m_progressBar->update();
    qApp->processEvents();
}

void MainWindow::populateTree(const aiScene *scene)
{
    if (!scene || !scene->mRootNode) return;

    // Clear existing mappings
    m_meshToItem.clear();

    std::function<void(aiNode *, QTreeWidgetItem *)> recurse =
            [&](aiNode *node, QTreeWidgetItem *parentItem) {
        // Create the node item
        QTreeWidgetItem *nodeItem = new QTreeWidgetItem();
        nodeItem->setText(0, QString::fromUtf8(node->mName.C_Str()));
        nodeItem->setData(0, Qt::UserRole, QVariant::fromValue<void *>(node));
        addCheckboxToItem(nodeItem);
        m_nodeToItem[node] = nodeItem;

        if (parentItem)
        {
            parentItem->addChild(nodeItem);
        } else
        {
            m_treeWidget->addTopLevelItem(nodeItem);
        }

        // Handle meshes based on count
        if (node->mNumMeshes > 1)
        {
            // Multiple meshes: add each as a child
            for (unsigned int i = 0; i < node->mNumMeshes; ++i)
            {
                unsigned int meshIndex = node->mMeshes[i];
                aiMesh *mesh = scene->mMeshes[meshIndex];

                QTreeWidgetItem *meshItem = new QTreeWidgetItem();

                // Use mesh name if available, otherwise use a generic name
                QString meshName = QString::fromUtf8(mesh->mName.C_Str());

                if (meshName.isEmpty())
                {
                    meshName = QString("Mesh_%1").arg(meshIndex + 1);
                }

                meshItem->setText(0, QString("Mesh %1: %2").arg(meshIndex + 1).arg(meshName));
                meshItem->setData(0, Qt::UserRole, QVariant::fromValue<void *>(node)); // Store parent node
                meshItem->setData(0, Qt::UserRole + 1, meshIndex); // Store mesh index

                nodeItem->addChild(meshItem);
                addCheckboxToItem(meshItem, true);

                // Add mesh mapping
                m_meshToItem[meshIndex] = meshItem;
            }
        } else if (node->mNumMeshes == 1)
        {
            // Single mesh: store mesh info in the node item itself
            unsigned int meshIndex = node->mMeshes[0];
            nodeItem->setData(0, Qt::UserRole + 1, meshIndex); // Store mesh index

            // Add mesh mapping - for single mesh, the node item represents the mesh
            m_meshToItem[meshIndex] = nodeItem;
        }
        // If mNumMeshes == 0, just keep it as a container node (no additional data)

        // Recursively process child nodes
        for (unsigned i = 0; i < node->mNumChildren; ++i)
        {
            recurse(node->mChildren[i], nodeItem);
        }
    };

    recurse(scene->mRootNode, nullptr);
}

void MainWindow::addCheckboxToItem(QTreeWidgetItem *item, bool checked)
{
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable); // Enable checkbox
    item->setCheckState(0, checked ? Qt::Checked : Qt::Unchecked); // Set initial state
}

void MainWindow::onSelectionChanged(const std::unordered_set<int> &selectedMeshIndices)
{
    // Block tree signals to prevent recursion
    m_treeWidget->blockSignals(true);

    // Clear current tree selection
    m_treeWidget->clearSelection();

    // Select corresponding tree items
    for (int meshIndex: selectedMeshIndices)
    {
        QTreeWidgetItem *item = findTreeItemByMeshIndex(meshIndex);
        if (item)
        {
            item->setSelected(true);
        }
    }

    m_treeWidget->blockSignals(false);
}


void MainWindow::onItemVisibilityChanged(QTreeWidgetItem *item, int column)
{
    if (column != 0) return; // Only handle the first column (checkbox column)

    Qt::CheckState state = item->checkState(0); // Get the current checkbox state
    // Cascade the checkbox state to child items
    updateChildItems(item, state);
    updateParentItem(item); // Propagate changes to parent items

    // Retrieve the mesh/node information from the item's data
    bool isVisible = (state == Qt::Checked);

    if (item->data(0, Qt::UserRole + 1).isValid())
    {
        // Handle mesh visibility
        unsigned int meshIndex = item->data(0, Qt::UserRole + 1).toUInt();
        setMeshVisibility(meshIndex, isVisible);
    }
}

void MainWindow::onTreeSelectionChanged()
{
    // Get all currently selected items in the tree
    QList<QTreeWidgetItem *> selectedItems = m_treeWidget->selectedItems();

    // Collect all mesh indices from selected items
    std::unordered_set<int> meshIndices;

    for (QTreeWidgetItem *item: selectedItems)
    {
        // Check if this item represents a specific mesh
        QVariant meshIndexData = item->data(0, Qt::UserRole + 1);
        if (meshIndexData.isValid())
        {
            // This item represents a specific mesh
            unsigned int meshIndex = meshIndexData.toUInt();
            meshIndices.insert(meshIndex);
        } else
        {
            // This is a node item - get all meshes under this node
            aiNode *node = static_cast<aiNode *>(item->data(0, Qt::UserRole).value<void *>());
            if (node)
            {
                // Collect all mesh indices from this node
                std::unordered_set<int> nodeMeshes = getMeshIndicesFromNode(node);
                meshIndices.insert(nodeMeshes.begin(), nodeMeshes.end());
            }
        }
    }

    // Update the 3D viewer selection
    m_viewerWidget->setSelection(meshIndices);
}

void MainWindow::updateTreeItemVisibility(int meshIndex, bool visible)
{
    QTreeWidgetItem *item = findTreeItemByMeshIndex(meshIndex);
    if (item)
    {
        item->setCheckState(0, visible ? Qt::Checked : Qt::Unchecked);
        updateParentItem(item);
    }
}

void MainWindow::updateAllTreeItemVisibility(const std::unordered_map<int, bool> &visibilityMap)
{
    // Block signals to prevent recursion
    m_treeWidget->blockSignals(true);

    for (const auto &[meshIndex, visible]: visibilityMap)
    {
        QTreeWidgetItem *item = findTreeItemByMeshIndex(meshIndex);
        if (item)
        {
            item->setCheckState(0, visible ? Qt::Checked : Qt::Unchecked);
            updateParentItem(item);
        }
    }

    m_treeWidget->blockSignals(false);
}

// Find tree item by mesh index
QTreeWidgetItem *MainWindow::findTreeItemByMeshIndex(int meshIndex)
{
    std::function<QTreeWidgetItem*(QTreeWidgetItem *)> searchItem;
    searchItem = [&](QTreeWidgetItem *item) -> QTreeWidgetItem * {
        // Check if this item matches the mesh index
        QVariant meshIndexData = item->data(0, Qt::UserRole + 1);
        if (meshIndexData.isValid() && meshIndexData.toUInt() == meshIndex)
        {
            return item;
        }

        // Search children
        for (int i = 0; i < item->childCount(); ++i)
        {
            QTreeWidgetItem *found = searchItem(item->child(i));
            if (found) return found;
        }
        return nullptr;
    };

    // Search from root
    for (int i = 0; i < m_treeWidget->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem *found = searchItem(m_treeWidget->topLevelItem(i));
        if (found) return found;
    }
    return nullptr;
}

// Get all mesh indices from a node (recursively)
std::unordered_set<int> MainWindow::getMeshIndicesFromNode(aiNode *node)
{
    std::unordered_set<int> meshIndices;

    // Add direct meshes from this node
    for (unsigned int i = 0; i < node->mNumMeshes; ++i)
    {
        meshIndices.insert(node->mMeshes[i]);
    }

    // Recursively add meshes from child nodes
    for (unsigned int i = 0; i < node->mNumChildren; ++i)
    {
        std::unordered_set<int> childMeshes = getMeshIndicesFromNode(node->mChildren[i]);
        meshIndices.insert(childMeshes.begin(), childMeshes.end());
    }

    return meshIndices;
}

void MainWindow::updateChildItems(QTreeWidgetItem *parentItem, Qt::CheckState state)
{
    for (int i = 0; i < parentItem->childCount(); ++i)
    {
        QTreeWidgetItem *childItem = parentItem->child(i);
        childItem->setCheckState(0, state); // Update the child item's checkbox state
        updateChildItems(childItem, state); // Recursively update its children
    }
}

void MainWindow::updateParentItem(QTreeWidgetItem *childItem)
{
    QTreeWidgetItem *parentItem = childItem->parent();
    if (!parentItem) return; // No parent, stop recursion

    // Temporarily block signals to avoid infinite loops
    m_treeWidget->blockSignals(true);

    bool allChecked = true;
    bool allUnchecked = true;

    for (int i = 0; i < parentItem->childCount(); ++i)
    {
        QTreeWidgetItem *siblingItem = parentItem->child(i);
        Qt::CheckState siblingState = siblingItem->checkState(0);

        if (siblingState == Qt::Checked)
        {
            allUnchecked = false;
        } else if (siblingState == Qt::Unchecked)
        {
            allChecked = false;
        } else if (siblingState == Qt::PartiallyChecked)
        {
            allChecked = false;
            allUnchecked = false;
        }
    }

    if (allChecked)
    {
        parentItem->setCheckState(0, Qt::Checked);
    } else if (allUnchecked)
    {
        parentItem->setCheckState(0, Qt::Unchecked);
    } else
    {
        parentItem->setCheckState(0, Qt::PartiallyChecked);
    }

    // Unblock signals after modifications
    m_treeWidget->blockSignals(false);

    // Recursively update the parent's parent
    updateParentItem(parentItem);
}

void MainWindow::setMeshVisibility(unsigned int meshIndex, bool isVisible)
{
    m_viewerWidget->setMeshVisibility(meshIndex, isVisible);
}


void MainWindow::filterTree(const QString &text)
{
    if (m_highlightDelegate)
        m_highlightDelegate->setPattern(text);
    auto matches = [=](const QString &pattern, const QString &value) -> int {
        int score = 0;
        int patternIndex = 0;
        for (int i = 0; i < value.size(); ++i)
        {
            if (patternIndex < pattern.size() &&
                pattern[patternIndex].toLower() == value[i].toLower())
            {
                ++score;
                ++patternIndex;
            }
        }
        return (patternIndex == pattern.size()) ? score : 0;
    };

    QTreeWidgetItemIterator it(m_treeWidget);
    while (*it)
    {
        QTreeWidgetItem *item = *it;
        const QString itemText = item->text(0);
        int score = matches(text, itemText);
        bool match = (text.isEmpty() || score > 0);
        item->setHidden(!match);

        if (match)
        {
            // Expand all ancestors so this item is visible
            QTreeWidgetItem *parent = item->parent();
            while (parent)
            {
                parent->setExpanded(true);
                parent->setHidden(false);
                parent = parent->parent();
            }
        }

        ++it;
    }

    m_treeWidget->viewport()->update();
}

void MainWindow::selectTreeNodeFor(aiNode *node)
{
    // If this node is already selected, deselect it
    if (m_currentlySelectedNode == node)
    {
        m_treeWidget->setCurrentItem(nullptr);
        m_currentlySelectedNode = nullptr;
        return;
    }

    auto it = m_nodeToItem.find(node);
    if (it != m_nodeToItem.end())
    {
        QTreeWidgetItem *item = it->second;
        m_treeWidget->setCurrentItem(item);
        m_treeWidget->scrollToItem(item);
        item->setSelected(true);
        m_currentlySelectedNode = node;
    }
}

void MainWindow::selectTreeMeshFor(int meshIndex)
{
    // If meshIndex is -1, deselect
    if (meshIndex == -1)
    {
        m_treeWidget->setCurrentItem(nullptr);
        m_currentlySelectedNode = nullptr;
        return;
    }

    // Find the tree item for this specific mesh
    auto meshIt = m_meshToItem.find(meshIndex);
    if (meshIt != m_meshToItem.end())
    {
        QTreeWidgetItem *meshItem = meshIt->second;

        // Select and scroll to the mesh item
        m_treeWidget->setCurrentItem(meshItem);
        m_treeWidget->scrollToItem(meshItem);
        meshItem->setSelected(true);

        // If this is a child mesh item (not a single-mesh node), expand the parent
        QTreeWidgetItem *parentItem = meshItem->parent();
        if (parentItem)
        {
            parentItem->setExpanded(true);
        }

        // Update the currently selected node
        aiNode *nodeContainingMesh = m_viewerWidget->findNodeForMesh(m_viewerWidget->getScene()->mRootNode, meshIndex);
        m_currentlySelectedNode = nodeContainingMesh;
    }
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
    {
        QList<QUrl> urlList = event->mimeData()->urls();
        if (!urlList.isEmpty() && urlList.first().isLocalFile())
        {
            event->acceptProposedAction();
        }
    }
}

void MainWindow::dropEvent(QDropEvent *event)
{
    const QMimeData *mimeData = event->mimeData();
    if (mimeData->hasUrls())
    {
        QList<QUrl> urlList = mimeData->urls();
        if (!urlList.isEmpty())
        {
            QString filePath = urlList.first().toLocalFile();
            if (!filePath.isEmpty())
            {
                openFile(filePath);
            }
        }
    }
}
