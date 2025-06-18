#define NOMINMAX

#include "ModelViewerWidget.h"
#include "GLCamera.h"
#include "FlyOutViewButton.h"
#include <assimp/postprocess.h>
#include <cfloat>
#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QMatrix4x4>
#include <QMouseEvent>
#include <QPushButton>
#include <QTimer>
#include <QToolButton>
#include <QMenu>
#include <QStyleFactory>


#ifdef _WIN32
#include <windows.h>
#endif
#include <GL/gl.h>
#include <GL/glu.h>

#include <algorithm>


ModelViewerWidget::ModelViewerWidget(QWidget *parent)
    : QOpenGLWidget(parent), m_shader(nullptr), m_backgroundShader(nullptr),
      m_trihedronShader(nullptr), m_camera(nullptr), m_scene(nullptr), m_inertiaActive(false),
	m_isDragging(false), m_mode(InteractionMode::Select), m_multiSelectionEnabled(false)
{
    QSurfaceFormat fmt;
    fmt.setDepthBufferSize(24);
    fmt.setVersion(3, 3);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    fmt.setOption(QSurfaceFormat::DebugContext);
    QSurfaceFormat::setDefaultFormat(fmt);

    loadBgColorSettings();

    m_modelMatrix.setToIdentity();
    m_viewMatrix.setToIdentity();
    m_projectionMatrix.setToIdentity();

    m_viewRadius = 1000;
    m_cameraDistance = 500;
    m_sceneUpdated = false;
    m_inertiaTimer = new QTimer(this);
    m_inertiaTimer->setInterval(16); // ~60 FPS
    connect(m_inertiaTimer, &QTimer::timeout, this, &ModelViewerWidget::onInertiaTimeout);

    setupViewToolbar();

    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QWidget::customContextMenuRequested,
            this, &ModelViewerWidget::showContextMenu);

    _rubberBand = new QRubberBand(QRubberBand::Rectangle, this);
    _rubberBand->setStyle(QStyleFactory::create("Fusion"));

    setFocusPolicy(Qt::StrongFocus);
}

void ModelViewerWidget::setupViewToolbar()
{
    _viewToolbar = new QWidget(this);
    _viewToolbar->setAttribute(Qt::WA_TransparentForMouseEvents, false);
    _viewToolbar->setStyleSheet("background: rgba(255, 255, 255, 100); border: 1px solid gray; border-radius: 4px;");
    _viewToolbar->setFixedHeight(64);

    QHBoxLayout* layout = new QHBoxLayout(_viewToolbar);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(6);

    layout->addWidget(createViewButton(":/icons/res/top.png", "Top View", [this]() { setViewTop(); }, _viewToolbar));
    layout->addWidget(createViewButton(":/icons/res/front.png", "Front View", [this]() { setViewFront(); },
        _viewToolbar));
    layout->addWidget(createViewButton(":/icons/res/left.png", "Left View", [this]() { setViewLeft(); }, _viewToolbar));
    layout->addWidget(createViewButton(":/icons/res/bottom.png", "Bottom View", [this]() { setViewBottom(); },
        _viewToolbar));
    layout->addWidget(createViewButton(":/icons/res/back.png", "Rear View", [this]() { setViewRear(); }, _viewToolbar));
    layout->addWidget(createViewButton(":/icons/res/right.png", "Right View", [this]() { setViewRight(); },
        _viewToolbar));

    m_toolButtonIsometricView = new FlyOutViewButton(_viewToolbar);
    m_toolButtonIsometricView->setObjectName(QString::fromUtf8("toolButtonIsometricView"));
    m_toolButtonIsometricView->setToolTip("Axonometric View");
    m_toolButtonIsometricView->setIcon(QIcon(":/icons/res/isometric.png"));
    m_toolButtonIsometricView->setPopupMode(QToolButton::DelayedPopup);
    m_toolButtonIsometricView->setIconSize(QSize(64, 64));
    m_toolButtonIsometricView->setAutoRaise(true); // Flat appearance


    layout->addWidget(m_toolButtonIsometricView);
    disconnect(m_toolButtonIsometricView, SIGNAL(clicked()), 0, 0);

    m_isometricView = new QAction(QIcon(":/icons/res/isometric.png"), "Isometric", this);
    m_isometricView->setObjectName(QString::fromUtf8("isometricView"));
    m_isometricView->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_1));
    connect(m_isometricView, &QAction::triggered, this, &ModelViewerWidget::onActionIsometricViewTriggered);

    m_dimetricView = new QAction(QIcon(":/icons/res/dimetric.png"), "Dimetric", this);
    m_dimetricView->setObjectName(QString::fromUtf8("dimetricView"));
    m_dimetricView->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_2));
    connect(m_dimetricView, &QAction::triggered, this, &ModelViewerWidget::onActionDimetricViewTriggered);

    m_trimetricView = new QAction(QIcon(":/icons/res/trimetric.png"), "Trimetric", this);
    m_trimetricView->setObjectName(QString::fromUtf8("trimetricView"));
    m_trimetricView->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_3));
    connect(m_trimetricView, &QAction::triggered, this, &ModelViewerWidget::onActionTrimetricViewTriggered);

    // View
    QMenu* axoMenu = new QMenu;
    axoMenu->setStyleSheet(
        "QMenu {"
        "    background-color: rgba(255, 255, 255, 140);"
        "    border: 1px solid gray;"
        "    border-radius: 4px;"
        "    padding: 2px;"
        "    icon-size: 42px;"
        "}"
        "QMenu::item {"
        "    background: transparent;"
        "    background-color: #f0f0f0;"
        "    border: 1px solid #c0c0c0;"
        "    border-radius: 4px;"
        "    padding: 5px 8px;"
        "    margin: 3px;"
        "    min-width: 120px;"
        "    min-height: 30px;"
        "    font-weight: normal;"
        "    color: black;"
        "}"
        "QMenu::item:selected {"
        "    background-color: #e0e0ff;"
        "    border: 1px solid #a0a0ff;"
        "    color: black;"
        "}"
        "QMenu::item:pressed {"
        "    background-color: #d0d0ff;"
        "    border: 1px solid #8080ff;"
        "    color: black;"
        "}"
        "QMenu::icon {"
        "    padding-left: 10px;"
        "    padding-right: 8px;"
        "}"
        "QMenu::separator {"
        "    height: 1px;"
        "    background-color: #c0c0c0;"
        "    margin: 4px 8px;"
        "}"
    );
    axoMenu->addAction(m_isometricView);
    axoMenu->addAction(m_dimetricView);
    axoMenu->addAction(m_trimetricView);
    // add action to widget as well
    addAction(m_isometricView);
    addAction(m_dimetricView);
    addAction(m_trimetricView);

    m_toolButtonIsometricView->setMenu(axoMenu);
    m_toolButtonIsometricView->setDefaultAction(m_isometricView);
    QObject::connect(m_toolButtonIsometricView, SIGNAL(triggered(QAction*)),
        m_toolButtonIsometricView, SLOT(setDefaultAction(QAction*)));

    layout->addWidget(createViewButton(":/icons/res/fit-all.png", "Fit All", [this]() { fitToView(); }, _viewToolbar));

    QToolButton* projToggleButton = new QToolButton(_viewToolbar);
    projToggleButton->setCheckable(true);
    projToggleButton->setIcon(QIcon(":/icons/res/Perspective.png")); // default icon
    projToggleButton->setIconSize(QSize(64, 64));
    projToggleButton->setToolTip("Toggle Projection");

    connect(projToggleButton, &QToolButton::toggled, this, [this, projToggleButton](bool checked) {
        if (checked)
        {
            m_camera->setProjectionType(GLCamera::ProjectionType::ORTHOGRAPHIC);
            projToggleButton->setIcon(QIcon(":/icons/res/Ortho.png"));
            projToggleButton->setIconSize(QSize(64, 64));
            projToggleButton->setToolTip("Switch to Perspective");
        }
        else
        {
            m_camera->setProjectionType(GLCamera::ProjectionType::PERSPECTIVE);
            projToggleButton->setIcon(QIcon(":/icons/res/Perspective.png"));
            projToggleButton->setIconSize(QSize(64, 64));
            projToggleButton->setToolTip("Switch to Orthographic");
        }
        update();
        });

    layout->addWidget(projToggleButton);
}

QToolButton *ModelViewerWidget::createViewButton(const QString &iconPath, const QString &tooltip,
                                                 const std::function<void()> &callback, QWidget *parent)
{
    QToolButton *button = new QToolButton(parent);
    button->setIcon(QIcon(iconPath));
    button->setIconSize(QSize(64, 64));
    button->setToolTip(tooltip);
    button->setAutoRaise(true); // Flat appearance
    QObject::connect(button, &QToolButton::clicked, callback);
    return button;
}

#include "BackgroundColor.h"
#include <QSettings>
void ModelViewerWidget::setBackgroundColor()
{
    BackgroundColor bgCol(this);
    bgCol.exec();
}

void ModelViewerWidget::initializeGL()
{
    initializeOpenGLFunctions();

    glEnable(GL_DEPTH_TEST);

    m_camera = new GLCamera(height(), width(), m_viewRadius, 45);
    m_camera->setProjectionType(GLCamera::ProjectionType::PERSPECTIVE);
    m_camera->setView(GLCamera::ViewProjection::SE_ISOMETRIC_VIEW);

    if (!m_scene)
    {
        // Set up a default camera view for the test triangle
        m_camera->setPosition(QVector3D(0, 0, 2));
        m_camera->setPosition(QVector3D(0, 0, 0));
        //m_camera->setUpVector(QVector3D(0, 1, 0));
        m_camera->setViewRange(2.0f);
        m_camera->setProjectionType(GLCamera::ProjectionType::PERSPECTIVE);
    }


    // Load shader sources
	m_shader = std::make_unique<ShaderProgram>();
    m_shader->loadCompileAndLinkShaderFromFile(":/shaders/shaders/mesh.vert",
                  ":/shaders/shaders/mesh.frag");

    m_backgroundShader = std::make_unique<ShaderProgram>();
    m_backgroundShader->loadCompileAndLinkShaderFromFile(":/shaders/shaders/gradientbg.vert",
                            ":/shaders/shaders/gradientbg.frag");

    m_trihedronShader = std::make_unique<ShaderProgram>();
    m_trihedronShader->loadCompileAndLinkShaderFromFile(":/shaders/shaders/trihedron.vert",
                           ":/shaders/shaders/trihedron.frag");

    m_trihedron = std::make_unique<Trihedron>(m_trihedronShader.get());
}

void ModelViewerWidget::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);

    if (h == 0) h = 1; // Prevent division by zero

    if (m_camera)
    {
        m_camera->setScreenSize(w, h);
        m_camera->setViewRange(m_viewRadius * 2.1f);
        m_viewMatrix = m_camera->getViewMatrix();
        m_projectionMatrix = m_camera->getProjectionMatrix();
    } else
    {
        // Fallback for when no model/camera is set up
        m_viewMatrix.setToIdentity();
        m_viewMatrix.lookAt(QVector3D(0, 0, 2), QVector3D(0, 0, 0), QVector3D(0, 1, 0));
        m_projectionMatrix.setToIdentity();
        m_projectionMatrix.perspective(45.0f, float(w) / float(h), 0.1f, 10.0f);
    }
}

void ModelViewerWidget::paintGL()
{
    glViewport(0, 0, width(), height());
    glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    drawGradientBackground(m_bgTopColor.redF(), m_bgTopColor.greenF(), m_bgTopColor.blueF(), m_bgTopColor.alphaF(),
        m_bgBotColor.redF(), m_bgBotColor.greenF(), m_bgBotColor.blueF(), m_bgBotColor.alphaF(), m_gradientStyle);

    glEnable(GL_DEPTH_TEST);
    m_viewMatrix = m_camera->getViewMatrix();
    m_projectionMatrix = m_camera->getProjectionMatrix();


    if (!m_glMeshes.empty())
    {
        m_shader->bind();

        QVector3D lightColor(1.0f, 1.0f, 1.0f);
        QVector3D lightPos(0.0f, 0.0f, 1.0f);
        QVector3D ambient(0.3f, 0.3f, 0.3f);
        float shininess = 100.0f;

        QVector3D lightDirWorld(0.577f, 0.577f, 0.577f); // Normalized (1,1,1)
        QVector3D cameraTarget = m_viewCenter + lightDirWorld * m_viewRadius; // Target point in world space
        QVector3D cameraPos = m_camera->getPosition(); // or eye position in world space

        QVector3D viewDir = cameraTarget - cameraPos;
        viewDir.normalize();
        m_shader->setUniformValue("lightDir", lightDirWorld);
        m_shader->setUniformValue("viewPos", cameraPos);
        m_shader->setUniformValue("lightPos", lightPos);
        m_shader->setUniformValue("ambientColor", ambient);
        m_shader->setUniformValue("shininess", shininess);

        m_shader->setUniformValue("mvp", m_projectionMatrix * m_viewMatrix);
        m_shader->setUniformValue("view", m_viewMatrix);

        for (const auto &meshptr: m_glMeshes)
        {
            GLMesh *mesh = meshptr.get(); // Use smart pointer to access raw pointer

            // Skip rendering if the mesh is not visible
            if (!mesh->isVisible())
            {
                continue;
            }


            m_shader->setUniformValue("specularColor", mesh->material().specular);
            m_shader->setUniformValue("model", mesh->modelMatrix());

            // Check if this mesh should be highlighted
            // Fast lookup using reverse mapping
            bool shouldHighlight = false;
            auto it = m_glMeshToIndex.find(mesh);
            if (it != m_glMeshToIndex.end())
            {
                int meshIndex = it->second;
                shouldHighlight = m_selectedMeshIndices.find(meshIndex) != m_selectedMeshIndices.end();
            }

            // Apply highlighting
            if (shouldHighlight)
            {
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                m_shader->setUniformValue("isSelected", true);
            } else
            {
                glDisable(GL_BLEND);
                m_shader->setUniformValue("isSelected", false);
            }

            if (mesh->hasAnyOpacity())
            {
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glDepthMask(GL_FALSE);
            } else
            {
                glDisable(GL_BLEND);
                glDepthMask(GL_TRUE);
            }
            mesh->draw();
        }
    }

    drawTrihedronOverlay(); // Draw trihedron in mini viewport
}


void ModelViewerWidget::updateCamera()
{
    // Update camera position based on the current rotation and zoom
    if (!m_scene)
    {
        return;
    }
    // After computing bounding box:
    aiVector3D minimum(FLT_MAX, FLT_MAX, FLT_MAX);
    aiVector3D maximum(-FLT_MAX, -FLT_MAX, -FLT_MAX);

    aiMatrix4x4 identity;
    computeBoundingBox(m_scene, m_scene->mRootNode, minimum, maximum, identity);
    float maxExtent = std::max({maximum.x - minimum.x, maximum.y - minimum.y, maximum.z - minimum.z});
    QVector3D max(maximum.x, maximum.y, maximum.z);
    QVector3D min(minimum.x, minimum.y, minimum.z);
    m_viewCenter = (max + min) * 0.5f;
    m_viewRadius = maxExtent * 0.5f;

    // Ideal distance from camera to model center based on FOV
    float fovYRadians = 45.0f * M_PI / 180.0f;
    m_cameraDistance = m_viewRadius / std::tan(fovYRadians * 0.5f);

    // Final camera position offset along Z axis (behind object)
    QVector3D center = m_viewCenter;
    QVector3D camPos = center + QVector3D(0, 0, m_cameraDistance);

    m_camera->setViewRange(m_viewRadius * 2.1f);

    QVector3D viewPos = m_viewCenter;
    m_camera->setPosition(viewPos);

    m_viewMatrix = m_camera->getViewMatrix();
    m_projectionMatrix = m_camera->getProjectionMatrix();

    m_sceneUpdated = true;
}

void ModelViewerWidget::computeBoundingBox(const aiScene *scene, const aiNode *node,
                                           aiVector3D &minimum, aiVector3D &maximum, const aiMatrix4x4 &transform)
{
    aiMatrix4x4 currentTransform = transform * node->mTransformation;

    for (unsigned int i = 0; i < node->mNumMeshes; ++i)
    {
        const aiMesh *mesh = scene->mMeshes[node->mMeshes[i]];

        GLMesh *glMesh = m_meshIndexToGLMesh[node->mMeshes[i]];
        if (glMesh && glMesh->isVisible())
        {
            // If the mesh is visible, compute its bounds
            computeBounds(mesh, currentTransform, minimum, maximum);
        }
    }

    for (unsigned int i = 0; i < node->mNumChildren; ++i)
    {
        computeBoundingBox(scene, node->mChildren[i], minimum, maximum, currentTransform);
    }
}

void ModelViewerWidget::computeBounds(const aiMesh *mesh, const aiMatrix4x4 &currentTransform, aiVector3D &minimum,
                                      aiVector3D &maximum)
{
    for (unsigned int j = 0; j < mesh->mNumVertices; ++j)
    {
        aiVector3D v = mesh->mVertices[j];
        v *= currentTransform; // apply transformation

        minimum.x = std::min(minimum.x, v.x);
        minimum.y = std::min(minimum.y, v.y);
        minimum.z = std::min(minimum.z, v.z);

        maximum.x = std::max(maximum.x, v.x);
        maximum.y = std::max(maximum.y, v.y);
        maximum.z = std::max(maximum.z, v.z);
    }
}

void ModelViewerWidget::computeBoundingSphere(const aiMesh *iMesh, const aiMatrix4x4 &transform, aiVector3D &oCenter,
                                              float &oRadius)
{
    aiVector3D minimum(FLT_MAX, FLT_MAX, FLT_MAX);
    aiVector3D maximum(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    computeBounds(iMesh, transform, minimum, maximum);
    oCenter = (aiVector3D(maximum.x, maximum.y, maximum.z) + aiVector3D(minimum.x, minimum.y, minimum.z)) * 0.5f;
    float maxExtent = std::max({maximum.x - minimum.x, maximum.y - minimum.y, maximum.z - minimum.z});
    oRadius = maxExtent * 0.5f;
}


void ModelViewerWidget::drawGradientBackground(float top_r, float top_g, float top_b, float top_a, float bot_r, float bot_g, float bot_b, float bot_a, int gradientStyle)
{
    glViewport(0, 0, width(), height());
    if (!m_bgVAO.isCreated())
    {
        m_bgVAO.create();
    }

    glDisable(GL_DEPTH_TEST);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    m_backgroundShader->bind();

    int GRADIENT_STYLE = 0; // Default gradient style, can be changed based on user preference
    m_backgroundShader->setUniformValue("top_color", QVector4D(top_r, top_g, top_b, top_a));
    m_backgroundShader->setUniformValue("bot_color", QVector4D(bot_r, bot_g, bot_b, bot_a));
    m_backgroundShader->setUniformValue("gradient_style", gradientStyle);  // Pass the gradient style

    m_bgVAO.bind();
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glEnable(GL_DEPTH_TEST);
}

void ModelViewerWidget::loadBgColorSettings()
{
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());

    // Retrieve and validate top color
    QVariant topColorValue = settings.value("Background/TopColor");
    if (topColorValue.isValid() && topColorValue.canConvert<QColor>())
    {
        m_bgTopColor = topColorValue.value<QColor>();
    }
    else
    {
        m_bgTopColor = QColor::fromRgbF(0.45f, 0.45f, 0.45f, 1.0f);
    }

    // Retrieve and validate bottom color
    QVariant bottomColorValue = settings.value("Background/BottomColor");
    if (bottomColorValue.isValid() && bottomColorValue.canConvert<QColor>())
    {
        m_bgBotColor = bottomColorValue.value<QColor>();
    }
    else
    {
        m_bgBotColor = QColor::fromRgbF(0.9f, 0.9f, 0.9f, 1.0f);

    }

    // Retrieve and validate gradient style
    QVariant gradientStyleValue = settings.value("Background/GradientStyle");
    if (gradientStyleValue.isValid() && gradientStyleValue.canConvert<int>())
    {
        int style = gradientStyleValue.toInt();
        if (style >= 0 && style <= 3)
        {
            m_gradientStyle = style;
        }
        else
        {
            m_gradientStyle = 0; // Default to vertical gradient
        }
    }
    else
    {
        m_gradientStyle = 0; // Default to vertical gradient
    }
}

void ModelViewerWidget::drawTrihedronOverlay()
{
    const int overlaySize = 110; // Size of mini viewport
    const int margin = 10; // Margin from the bottom-left corner

    // Set up the mini viewport
    glViewport(margin, margin, overlaySize, overlaySize);
    glClear(GL_DEPTH_BUFFER_BIT); // Clear depth buffer for the overlay
    glEnable(GL_DEPTH_TEST); // Enable depth testing for the overlay

    // Create projection matrix
    QMatrix4x4 projection;
    if (m_camera->getProjectionType() == GLCamera::ProjectionType::PERSPECTIVE)
    {
        // Narrow field of view for better appearance in small viewport
        projection.perspective(30.0, 1.0, 0.1, 10.0);
    } else
    {
        // Orthographic projection for overlay
        float orthoSize = 1.15f;
        projection.ortho(-orthoSize, orthoSize, -orthoSize, orthoSize, 0.1f, 10.0f);
    }

    // Create view matrix for overlay
    QMatrix4x4 view;
    view.lookAt(QVector3D(0.0, 0.0, 5.0), // Eye position
                QVector3D(0.0, 0.0, 0.0), // Target position (origin)
                QVector3D(0.0, 1.0, 0.0)); // Up vector

    // Extract the camera's rotation matrix
    QMatrix4x4 cameraView = m_camera->getViewMatrix();

    // Remove the translation component
    cameraView.setColumn(3, QVector4D(0, 0, 0, 1)); // Zero out the translation part

    // Combine the overlay view matrix with the main camera's rotation matrix
    view = view * cameraView;

    // Draw the trihedron
    if (m_trihedron)
    {
        m_trihedron->draw(view, projection);
    }

    // Restore the viewport to the main scene
    glViewport(0, 0, width(), height());
}


void ModelViewerWidget::loadModel(const QString &filePath)
{
    makeCurrent(); // Ensure OpenGL context is current

    if (m_scene)
    {
        m_importer.FreeScene(); // Free previous scene if exists
        m_scene = nullptr;
        m_glMeshes.clear();
    }

    m_scene = m_importer.ReadFile(filePath.toStdString(),
                                  aiProcess_Triangulate | aiProcess_ValidateDataStructure |
                                  aiProcess_CalcTangentSpace | aiProcess_GenSmoothNormals |
                                  aiProcess_FixInfacingNormals | aiProcess_JoinIdenticalVertices |
                                  aiProcess_OptimizeMeshes | aiProcess_GenUVCoords | aiProcess_SortByPType);

    if (!m_scene || m_scene->mFlags == AI_SCENE_FLAGS_INCOMPLETE || !m_scene->mRootNode)
    {
        qDebug() << "ERROR::ASSIMP:: " << m_importer.GetErrorString();
        m_scene = nullptr;
        return;
    }

    m_lastModelPath = QFileInfo(filePath).absolutePath(); // Store the directory of the loaded model

    m_materialTextureCache.clear(); // Clear previous texture cache

    if (m_scene->mRootNode)
    {
        loadNodeMeshes(m_scene->mRootNode);

        for (const auto &pair: m_meshIndexToGLMesh)
        {
            m_glMeshToIndex[pair.second] = pair.first;
            m_visibilityMap[pair.first] = true; // Initialize visibility map
        }
    }


    updateCamera();
    update();
}

void ModelViewerWidget::loadNodeMeshes(aiNode *node)
{
    // Load meshes for this node
    for (unsigned int i = 0; i < node->mNumMeshes; ++i)
    {
        unsigned int meshIndex = node->mMeshes[i];
        aiMesh *mesh = m_scene->mMeshes[meshIndex];

        // compute bounding sphere for the mesh
        aiVector3D oCenter;
        float oRadius;
        computeBoundingSphere(mesh, node->mTransformation, oCenter, oRadius);

        //GLMesh* glMesh = new GLMesh(mesh, m_shader.program());
        auto glMesh = std::make_unique<GLMesh>(mesh, m_shader.get());

        glMesh->setBoundingSphere(QVector3D(oCenter.x, oCenter.y, oCenter.z), oRadius);

        // Set up material
        Material mat;
        GLuint textureId = 0;

        MaterialTextures textures;

        if (mesh->mMaterialIndex >= 0)
        {
            aiMaterial *material = m_scene->mMaterials[mesh->mMaterialIndex];
            aiColor4D c;
            float opacity = 1.0f;
            if (AI_SUCCESS == material->Get(AI_MATKEY_COLOR_AMBIENT, c))
                mat.ambient = QVector4D(c.r, c.g, c.b, c.a);
            if (AI_SUCCESS == material->Get(AI_MATKEY_COLOR_DIFFUSE, c))
                mat.diffuse = QVector4D(c.r, c.g, c.b, c.a);
            if (AI_SUCCESS == material->Get(AI_MATKEY_COLOR_SPECULAR, c))
                mat.specular = QVector4D(c.r, c.g, c.b, c.a);
            float shininess = 0.0f;
            if (AI_SUCCESS == material->Get(AI_MATKEY_SHININESS, shininess))
                mat.shininess = shininess;
            if (AI_SUCCESS == material->Get(AI_MATKEY_OPACITY, opacity) && opacity != 0)
            {
                if (opacity <= 0.0f)
                    std::cout << "Opacity: " << opacity << " - setting to 1" << std::endl;
                mat.opacity = opacity;
            }

            // Load textures - check multiple texture types
            textures = loadMaterialTextures(material, mesh->mMaterialIndex);
        }

        glMesh->setMaterial(mat);
        glMesh->setTextures(textures);

        // Set texture if loaded
        if (textureId > 0)
        {
            glMesh->setTexture(textureId);
        }

        glMesh->setupMesh();

        // Store associations - use raw pointer
        GLMesh *rawPtr = glMesh.get();
        m_nodeMeshes[node].push_back(rawPtr);
        m_meshIndexToGLMesh[meshIndex] = rawPtr; // Store raw pointer

        m_glMeshes.emplace_back(std::move(glMesh));
    }

    // Recursively process child nodes
    for (unsigned int i = 0; i < node->mNumChildren; ++i)
    {
        loadNodeMeshes(node->mChildren[i]);
    }
}


// Enhanced texture loading function that handles multiple texture types
MaterialTextures ModelViewerWidget::loadMaterialTextures(const aiMaterial *material, unsigned int materialIndex)
{
    // Check cache first
    auto cacheIt = m_materialTextureCache.find(materialIndex);
    if (cacheIt != m_materialTextureCache.end())
    {
        return cacheIt->second;
    }

    MaterialTextures textures;

    // Define texture type mappings
    struct TextureMapping
    {
        aiTextureType type;
        GLuint *target;
    };

    TextureMapping mappings[] = {
        {aiTextureType_DIFFUSE, &textures.diffuse},
        {aiTextureType_SPECULAR, &textures.specular},
        {aiTextureType_EMISSIVE, &textures.emissive},
        {aiTextureType_HEIGHT, &textures.height},
        {aiTextureType_DISPLACEMENT, &textures.displacement},
        {aiTextureType_OPACITY, &textures.opacity},
        {aiTextureType_METALNESS, &textures.metallic},
        {aiTextureType_DIFFUSE_ROUGHNESS, &textures.roughness},
        {aiTextureType_NORMALS, &textures.normal}
    };

    // Load each texture type
    for (const auto &mapping: mappings)
    {
        if (material->GetTextureCount(mapping.type) > 0)
        {
            aiString texturePath;
            if (material->GetTexture(mapping.type, 0, &texturePath) == AI_SUCCESS)
            {
                GLuint texId = loadTextureFromPath(texturePath.C_Str());
                if (texId > 0)
                {
                    *(mapping.target) = texId;
                }
            }
        }
    }

    // Cache the result
    m_materialTextureCache[materialIndex] = textures;
    return textures;
}

// Separate function to handle the actual texture loading
GLuint ModelViewerWidget::loadTextureFromPath(const char *texturePath)
{
    // Handle both absolute and relative paths
    QString fullPath;
    QFileInfo pathInfo(QString::fromUtf8(texturePath));

    if (pathInfo.isAbsolute())
    {
        fullPath = pathInfo.absoluteFilePath();
    } else
    {
        // Relative path - combine with model directory
        fullPath = QDir(m_lastModelPath).filePath(QString::fromUtf8(texturePath));
    }

    // Also try common alternative paths if the direct path fails
    QStringList pathsToTry = {
        fullPath,
        QDir(m_lastModelPath).filePath(pathInfo.fileName()), // Just filename in model dir
        QDir(m_lastModelPath).filePath("textures/" + pathInfo.fileName()), // Common subfolder
        QDir(m_lastModelPath).filePath("materials/" + pathInfo.fileName()) // Another common subfolder
    };

    for (const QString &path: pathsToTry)
    {
        QImage image(path);
        if (!image.isNull())
        {
            // Convert to proper format and flip if needed
            image = image.convertToFormat(QImage::Format_RGBA8888);

            // Check if image needs to be flipped (common with different modeling software)
            // You might need to adjust this based on your coordinate system
            image = image.mirrored();

            GLuint texId;
            glGenTextures(1, &texId);
            glBindTexture(GL_TEXTURE_2D, texId);

            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                         image.width(), image.height(), 0,
                         GL_RGBA, GL_UNSIGNED_BYTE, image.bits());

            // Set texture parameters
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

            glGenerateMipmap(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, 0); // Unbind

            //qDebug() << "Loaded texture:" << path;
            return texId;
        }
    }

    qWarning() << "Failed to load texture:" << texturePath;
    return 0;
}

void ModelViewerWidget::mousePressEvent(QMouseEvent *event)
{
    m_isDragging = true;
    m_totalMouseDelta = QPoint(0, 0); // Reset drag distance
    if (m_inertiaTimer->isActive())
        m_inertiaTimer->stop();

    m_leftButtonPoint = m_lastMousePos = event->pos();

    if ((event->modifiers() & Qt::ControlModifier))
    {
        if (event->button() == Qt::LeftButton)
            m_mode = InteractionMode::Rotate;
        else if (event->button() == Qt::RightButton)
            m_mode = InteractionMode::Pan;
        else if (event->button() == Qt::MiddleButton)
            m_mode = InteractionMode::Zoom;
    } else
    {
        m_mode = InteractionMode::Select;
        if (event->modifiers() & Qt::ShiftModifier)
        {
            m_multiSelectionEnabled = true;
        } else
        {
            m_multiSelectionEnabled = false;
        }
        if (event->button() == Qt::LeftButton)
        {
            _rubberBand->setGeometry(QRect(m_lastMousePos, QSize()));
            _rubberBand->show();
        }
    }

    if (event->button() == Qt::LeftButton && m_mode == InteractionMode::Select && m_scene)
    {
        pickAtScreenPosition(event->pos());       
    }
}


void ModelViewerWidget::mouseMoveEvent(QMouseEvent *event)
{
    QPoint delta = event->pos() - m_lastMousePos;
    m_totalMouseDelta += delta;
    QPoint downPoint = event->pos();

    if (m_mode == InteractionMode::Rotate)
    {
        setCursor(QCursor(QPixmap(":/icons/res/rotatecursor.png")));

        QPoint rotate = m_lastMousePos - downPoint;

        m_camera->rotateX(rotate.y() / 2.0);
        m_camera->rotateY(rotate.x() / 2.0);

        // Store rotation velocity
        m_rotationVelocity = QVector2D(delta.x(), delta.y()) / 2.0f;
        m_inertiaActive = false; // Stop inertia while dragging
    } else if (m_mode == InteractionMode::Pan)
    {
        setCursor(QCursor(QPixmap(":/icons/res/pancursor.png")));

        // Pan speed scaled by distance
        float panSpeed = m_cameraDistance * 0.001f;

        // Apply panning to the view center
        QVector3D OP = get3dTranslationVectorFromMousePoints(downPoint, m_lastMousePos);
        m_camera->move(OP.x(), OP.y(), OP.z());

        m_panVelocity = OP;
        m_inertiaActive = false;
    } else if (m_mode == InteractionMode::Zoom)
    {
        setCursor(QCursor(QPixmap(":/icons/res/zoomcursor.png")));

        float zoomDelta = delta.y() * 0.01f;

        if (downPoint.x() > m_lastMousePos.x() || downPoint.y() < m_lastMousePos.y())
            m_viewRadius /= 1.05f;
        else
            m_viewRadius *= 1.05f;

        // Translate to focus on mouse center
        QPoint cen = QRect(0, 0, width(), height()).center();
        float sign = (downPoint.x() > m_lastMousePos.x() || downPoint.y() < m_lastMousePos.y()) ? 1.0f : -1.0f;
        QVector3D OP = get3dTranslationVectorFromMousePoints(cen, event->position().toPoint());
        OP *= sign * 0.05f;
        m_camera->move(OP.x(), OP.y(), OP.z());

        // Store velocity
        m_zoomVelocity = sign * 0.10f;
        m_zoomPanVelocity = OP;
        m_inertiaActive = false;

        resizeGL(width(), height());
    }
    else
    {
        if(event->modifiers() != Qt::ControlModifier)
            _rubberBand->setGeometry(QRect(m_leftButtonPoint, event->pos()).normalized());
    }

    m_lastMousePos = downPoint;

    update();
}

void ModelViewerWidget::mouseReleaseEvent(QMouseEvent *event)
{
    setCursor(QCursor(Qt::ArrowCursor));

    if (event->button() & Qt::LeftButton || 
        (event->button() & Qt::LeftButton && event->modifiers() == Qt::ShiftModifier))
    {
        const int minRectangleSize = 50; // Minimum width and height in pixels
        if (m_scene && _rubberBand->width() >= minRectangleSize && _rubberBand->height() >= minRectangleSize)
        {
            sweepSelection(_rubberBand->geometry());
        }
        _rubberBand->hide();
    }

    m_isDragging = false;

    int movementThreshold = 1; // Keep low for now

    if (m_totalMouseDelta.manhattanLength() > movementThreshold)
    {
        if (!m_inertiaTimer->isActive())
        {
            m_inertiaTimer->start(16); // 60 fps
            m_inertiaActive = true;
        }
    } else
    {
        // No real movement ? don't start inertia
        m_panVelocity = QVector3D(0, 0, 0);
        m_zoomPanVelocity = QVector3D(0, 0, 0);
        m_zoomVelocity = 0.0f;
        m_rotationVelocity = QVector2D(0, 0);
        m_inertiaTimer->stop();
    }

    Q_UNUSED(event);
    if (!(event->modifiers() & Qt::ControlModifier))
        m_mode = InteractionMode::Select;
}

void ModelViewerWidget::wheelEvent(QWheelEvent *event)
{
    QPoint numDegrees = event->angleDelta() / 8;
    if (!numDegrees.isNull())
    {
        if (!m_inertiaTimer->isActive())
            m_inertiaTimer->start(16);

        QPoint numSteps = numDegrees / 30;
        float zoomStep = numSteps.y();
        float zoomFactor = abs(zoomStep) + 0.05;

        if (zoomStep < 0)
            m_viewRadius *= zoomFactor;
        else
            m_viewRadius /= zoomFactor;

        // Translate to focus on mouse center
        QPoint cen = QRect(0, 0, width(), height()).center();
        float sign = (event->position().x() > cen.x() || event->position().y() < cen.y() ||
                      (event->position().x() < cen.x() && event->position().y() > cen.y())) && (zoomStep > 0)
                         ? 1.0f
                         : -1.0f;
        QVector3D OP = get3dTranslationVectorFromMousePoints(cen, event->position().toPoint());
        OP *= sign * 0.05f;
        m_camera->move(OP.x(), OP.y(), OP.z());

        // Add to velocities instead of overriding
        m_zoomVelocity += sign * 0.1f; // Tune factor
        m_zoomPanVelocity += OP * sign * 0.05f;
    }
    resizeGL(width(), height());
    update();
}

void ModelViewerWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier)
    {
        if (event->key() == Qt::Key_T)
        {
            setViewProjection(ViewProjection::Top);
        } else if (event->key() == Qt::Key_F)
        {
            setViewProjection(ViewProjection::Front);
        } else if (event->key() == Qt::Key_L)
        {
            setViewProjection(ViewProjection::Left);
        }
        if (event->key() == Qt::Key_B)
        {
            setViewProjection(ViewProjection::Bottom);
        } else if (event->key() == Qt::Key_R)
        {
            setViewProjection(ViewProjection::Rear);
        } else if (event->key() == Qt::Key_J)
        {
            setViewProjection(ViewProjection::Right);
        } else if (event->key() == Qt::Key_A)
        {
            setViewProjection(ViewProjection::Axonometric);
        }
    } else if (event->modifiers() & Qt::ShiftModifier)
    {
        if (event->key() == Qt::Key_A)
        {
            showAllMeshes();
            emit allMeshVisibilityChanged(m_visibilityMap);
        }
    } else
    {
        if (event->key() == Qt::Key_F)
        {
            fitToView();
        }
        if (event->key() == Qt::Key_Home)
        {
            // Reset view to default
            resetView();
            update();
        }
        if (event->key() == Qt::Key_Escape)
        {
            // Clear selection
            clearSelection();
            update();
			emit selectionChanged(m_selectedMeshIndices);            
        }
        if (event->key() == Qt::Key_Space)
        {
            hideSelectedMeshes();
            emit allMeshVisibilityChanged(m_visibilityMap);
        }
    }
}

void ModelViewerWidget::keyReleaseEvent(QKeyEvent *event)
{
    event->accept();
}

void ModelViewerWidget::resizeEvent(QResizeEvent *event)
{
    QOpenGLWidget::resizeEvent(event);
    QWidget::resizeEvent(event);
    if (_viewToolbar)
    {
        _viewToolbar->adjustSize();
        QSize toolbarSize = _viewToolbar->size();
        int margin = 10;

        int x = (width() - toolbarSize.width()) / 2;
        int y = height() - toolbarSize.height() - margin;

        // Clamp to ensure it's inside bounds
        x = std::max(0, x);
        y = std::max(0, y);

        _viewToolbar->move(x, y);
    }
}


void ModelViewerWidget::resetView()
{
    setViewProjection(ViewProjection::Axonometric);
    fitToView(); // Reset camera position and zoom
}

void ModelViewerWidget::setViewProjection(ViewProjection view)
{
    m_viewProjection = view;
    switch (view)
    {
        case ViewProjection::Top:
            setViewTop();
            break;
        case ViewProjection::Front:
            setViewFront();
            break;
        case ViewProjection::Left:
            setViewLeft();
            break;
        case ViewProjection::Bottom:
            setViewBottom();
            break;
        case ViewProjection::Rear:
            setViewRear();
            break;
        case ViewProjection::Right:
            setViewRight();
            break;
        case ViewProjection::Axonometric:
            setViewAxonometric();
            break;
        case ViewProjection::Custom:
        default:
            // Do nothing or reset to user-controlled
            break;
    }

    m_viewMatrix = m_camera->getViewMatrix();
    m_projectionMatrix = m_camera->getProjectionMatrix();
    update();
}


void ModelViewerWidget::setViewTop()
{
    m_camera->setView(GLCamera::ViewProjection::TOP_VIEW);
    update();
}

void ModelViewerWidget::setViewFront()
{
    m_camera->setView(GLCamera::ViewProjection::FRONT_VIEW);
    update();
}

void ModelViewerWidget::setViewLeft()
{
    m_camera->setView(GLCamera::ViewProjection::LEFT_VIEW);
    update();
}

void ModelViewerWidget::setViewBottom()
{
    m_camera->setView(GLCamera::ViewProjection::BOTTOM_VIEW);
    update();
}

void ModelViewerWidget::setViewRear()
{
    m_camera->setView(GLCamera::ViewProjection::REAR_VIEW);
    update();
}

void ModelViewerWidget::setViewRight()
{
    m_camera->setView(GLCamera::ViewProjection::RIGHT_VIEW);
    update();
}

void ModelViewerWidget::setViewAxonometric()
{
    m_camera->setView(GLCamera::ViewProjection::SE_ISOMETRIC_VIEW);
    update();
}

void ModelViewerWidget::setViewDimetric()
{
    m_camera->setView(GLCamera::ViewProjection::DIMETRIC_VIEW);
    update();
}

void ModelViewerWidget::setViewTrimetric()
{
    m_camera->setView(GLCamera::ViewProjection::TRIMETRIC_VIEW);
    update();
}

void ModelViewerWidget::onActionIsometricViewTriggered(bool /*checked*/)
{
    /*buttonGroupViews->setExclusive(false);
    QList<QAbstractButton*> buttons = buttonGroupViews->buttons();
    for (auto b : buttons)
    {
        b->setChecked(false);
    }
    buttonGroupViews->setExclusive(true);*/

    setViewAxonometric();

    m_toolButtonIsometricView->setDefaultAction(dynamic_cast<QAction*>(sender()));
}

void ModelViewerWidget::onActionDimetricViewTriggered(bool /*checked*/)
{
    /*buttonGroupViews->setExclusive(false);
    QList<QAbstractButton*> buttons = buttonGroupViews->buttons();
    for (auto b : buttons)
    {
        b->setChecked(false);
    }
    buttonGroupViews->setExclusive(true);*/

	setViewDimetric();   

    m_toolButtonIsometricView->setDefaultAction(dynamic_cast<QAction*>(sender()));
}

void ModelViewerWidget::onActionTrimetricViewTriggered(bool /*checked*/)
{
    /*buttonGroupViews->setExclusive(false);
    QList<QAbstractButton*> buttons = buttonGroupViews->buttons();
    for (auto b : buttons)
    {
        b->setChecked(false);
    }
    buttonGroupViews->setExclusive(true);*/

    setViewTrimetric();	

    m_toolButtonIsometricView->setDefaultAction(dynamic_cast<QAction*>(sender()));
}


void ModelViewerWidget::fitToView()
{
    // Optional: adjust _cameraDistance or zoom to fit model bounds
    updateCamera();
    update();
}

void ModelViewerWidget::onInertiaTimeout()
{
    if (m_mode == InteractionMode::Select)
        return; // Don't apply inertia while selecting
    if (!m_inertiaActive)
        return;

    bool stillActive = false;

    // Apply rotation inertia
    if (!m_rotationVelocity.isNull())
    {
        m_camera->rotateX(-m_rotationVelocity.y());
        m_camera->rotateY(-m_rotationVelocity.x());
        m_rotationVelocity *= m_inertiaFactor;
        if (m_rotationVelocity.length() > 0.01f)
            stillActive = true;
        else
            m_rotationVelocity = QVector2D();
    }

    // Apply pan inertia
    if (!m_panVelocity.isNull())
    {
        m_camera->move(m_panVelocity.x(), m_panVelocity.y(), m_panVelocity.z());
        m_panVelocity *= m_inertiaFactor;
        if (m_panVelocity.length() > 0.0001f)
            stillActive = true;
        else
            m_panVelocity = QVector3D();
    }

    // Apply zoom inertia
    if (std::abs(m_zoomVelocity) > 0.001f)
    {
        float zoomFactor = 1.005f;

        if (m_zoomVelocity > 0)
            m_viewRadius /= zoomFactor;
        else
            m_viewRadius *= zoomFactor;

        // Zoom-centric pan
        QPoint cen = rect().center();
        QVector3D OP = get3dTranslationVectorFromMousePoints(cen, cen);
        OP *= -m_zoomPanVelocity * 0.05f;
        m_camera->move(OP.x(), OP.y(), OP.z());

        m_zoomVelocity *= m_inertiaFactor * 0.75f;
        if (std::abs(m_zoomVelocity) > 0.001f)
            stillActive = true;
        else
            m_zoomVelocity = 0.0f;

        resizeGL(width(), height());
    }

    if (!stillActive)
    {
        m_inertiaActive = false;
        m_inertiaTimer->stop();
    }

    update();
}


void ModelViewerWidget::pickAtScreenPosition(const QPoint &pos)
{
    makeCurrent(); // Needed if using Qt with OpenGL

    // Get the viewport dimensions
    QRect viewportRect(0, 0, width(), height());

    float x = pos.x();
    float y = viewportRect.height() - pos.y(); // Flip Y for OpenGL
    float normalizedX = (x / viewportRect.width()) * 2.0f - 1.0f; // Normalize to [-1, 1]
    float normalizedY = (y / viewportRect.height()) * 2.0f - 1.0f;

    // Create normalized device coordinates (NDC) for near and far points
    QVector4D nearPointNDC(normalizedX, normalizedY, -1.0f, 1.0f); // NDC z = -1 for near
    QVector4D farPointNDC(normalizedX, normalizedY, 1.0f, 1.0f); // NDC z = 1 for far

    // Compute the inverse of the transformation matrix
    QMatrix4x4 viewProjectionMatrix = m_projectionMatrix * m_viewMatrix;
    QMatrix4x4 inverseViewProjectionMatrix = viewProjectionMatrix.inverted();

    // Unproject the NDC points to world space
    QVector4D nearPointWorld = inverseViewProjectionMatrix * nearPointNDC;
    QVector4D farPointWorld = inverseViewProjectionMatrix * farPointNDC;

    // Perform perspective divide to convert from homogeneous coordinates
    nearPointWorld /= nearPointWorld.w();
    farPointWorld /= farPointWorld.w();

    // Extract ray origin and direction
    QVector3D rayOrigin = nearPointWorld.toVector3D();
    QVector3D rayDirection = (farPointWorld.toVector3D() - rayOrigin).normalized();

    // Convert to aiVector3D if needed
    aiVector3D aiRayOrigin(rayOrigin.x(), rayOrigin.y(), rayOrigin.z());
    aiVector3D aiRayDirection(rayDirection.x(), rayDirection.y(), rayDirection.z());

    // Handle the pick ray
    pickRay(aiRayOrigin, aiRayDirection);
}

void ModelViewerWidget::pickRay(const aiVector3D &origin, const aiVector3D &dir)
{
    float minDistance = std::numeric_limits<float>::max();
    int hitMeshIndex = -1;
    aiMatrix4x4 hitTransform;

    // Track intersected mesh index
    std::function<void(aiNode *, const aiMatrix4x4 &)> traverse;
    traverse = [&](aiNode *node, const aiMatrix4x4 &parentTransform) {
        aiMatrix4x4 transform = parentTransform * node->mTransformation;
        for (unsigned i = 0; i < node->mNumMeshes; ++i)
        {
            const int meshIndex = node->mMeshes[i];
            const aiMesh *mesh = m_scene->mMeshes[meshIndex];

            // Bounding sphere intersection check
            QVector3D cen;
            float radius;
            GLMesh *glMesh = m_meshIndexToGLMesh[meshIndex];

            if (glMesh->isVisible() == false)
            {
                continue; // Skip invisible meshes
            }

            glMesh->getBoundingSphere(cen, radius);
            aiVector3D center(cen.x(), cen.y(), cen.z()); // Convert to aiVector3D
            center *= transform; // Apply transformation to center
            if (!rayIntersectsSphere(origin, dir, center, radius))
            {
                continue; // Skip this mesh entirely
            }

            for (unsigned f = 0; f < mesh->mNumFaces; ++f)
            {
                const aiFace &face = mesh->mFaces[f];
                if (face.mNumIndices != 3) continue;
                aiVector3D v0 = mesh->mVertices[face.mIndices[0]];
                aiVector3D v1 = mesh->mVertices[face.mIndices[1]];
                aiVector3D v2 = mesh->mVertices[face.mIndices[2]];
                v0 *= transform;
                v1 *= transform;
                v2 *= transform;
                float t;
                if (rayIntersectsTriangle(origin, dir, v0, v1, v2, t))
                {
                    if (t < minDistance)
                    {
                        minDistance = t;
                        hitMeshIndex = meshIndex;
                        hitTransform = transform;
                    }
                }
            }
        }
        for (unsigned i = 0; i < node->mNumChildren; ++i)
            traverse(node->mChildren[i], transform);
    };

    traverse(m_scene->mRootNode, aiMatrix4x4());

    if (hitMeshIndex != -1)
    {
        if (m_multiSelectionEnabled)
        {
            // Multi-selection mode
            if (m_selectedMeshIndices.find(hitMeshIndex) != m_selectedMeshIndices.end())
            {
                m_selectedMeshIndices.erase(hitMeshIndex);
            } else
            {
                m_selectedMeshIndices.insert(hitMeshIndex);
            }
        } else
        {
            // Single selection mode
            if (m_selectedMeshIndices.size() == 1 &&
                m_selectedMeshIndices.find(hitMeshIndex) != m_selectedMeshIndices.end())
            {
                m_selectedMeshIndices.clear();
            } else
            {
                m_selectedMeshIndices.clear();
                m_selectedMeshIndices.insert(hitMeshIndex);
            }
        }
    } else
    {
        if (!m_multiSelectionEnabled)
        {
            m_selectedMeshIndices.clear();
        }
    }

    // Single signal with complete selection state
    emit selectionChanged(m_selectedMeshIndices);
    update();
}

bool ModelViewerWidget::rayIntersectsSphere(const aiVector3D &origin, const aiVector3D &dir, const aiVector3D &center,
                                            float radius)
{
    // Vector from the ray origin to the sphere center
    aiVector3D oc = origin - center;

    // Quadratic equation coefficients
    float a = dir.SquareLength(); // Since dir should be normalized, a = 1
    float b = 2.0f * oc * dir; // Dot product in Assimp
    float c = oc.SquareLength() - radius * radius;

    // Discriminant of the quadratic equation
    float discriminant = b * b - 4 * a * c;

    // Ray intersects the sphere if the discriminant is non-negative
    return discriminant >= 0;
}

bool ModelViewerWidget::rayIntersectsTriangle(
    const aiVector3D &orig, const aiVector3D &dir,
    const aiVector3D &v0, const aiVector3D &v1, const aiVector3D &v2,
    float &outT
)
{
    const float EPSILON = 1e-5f;
    aiVector3D edge1 = v1 - v0;
    aiVector3D edge2 = v2 - v0;
    aiVector3D h = dir ^ edge2;
    float a = edge1 * h;
    if (fabs(a) < EPSILON) return false;

    float f = 1.0f / a;
    aiVector3D s = orig - v0;
    float u = f * (s * h);
    if (u < 0.0 || u > 1.0) return false;

    aiVector3D q = s ^ edge1;
    float v = f * (dir * q);
    if (v < 0.0 || u + v > 1.0) return false;

    float t = f * (edge2 * q);
    if (t > EPSILON)
    {
        outT = t;
        return true;
    }

    return false;
}

void ModelViewerWidget::sweepSelection(const QRect& rubberBandRect)
{
    QMatrix4x4 projMatrix = m_camera->getProjectionMatrix();
    QMatrix4x4 viewMatrix = m_camera->getViewMatrix();
    QRect viewport(0, 0, width(), height()); // Screen space rectangle

    // Determine if we're using perspective or orthographic projection
    bool isPerspective = (projMatrix(3, 2) != 0.0f); // Check if w component is affected by z

    std::function<void(aiNode*, const aiMatrix4x4&)> traverse;
    traverse = [&](aiNode* node, const aiMatrix4x4& parentTransform) {
        aiMatrix4x4 transform = parentTransform * node->mTransformation;
        for (unsigned i = 0; i < node->mNumMeshes; ++i)
        {
            const int meshIndex = node->mMeshes[i];
            const aiMesh* mesh = m_scene->mMeshes[meshIndex];
            QVector3D cen;
            float radius;
            GLMesh* glMesh = m_meshIndexToGLMesh[meshIndex];
            if (!glMesh->isVisible())
            {
                continue; // Skip invisible meshes
            }
            glMesh->getBoundingSphere(cen, radius);
            aiVector3D center(cen.x(), cen.y(), cen.z());
            center *= transform; // Apply transformation to center

            // Project center to screen space
            QVector4D worldCenter(center.x, center.y, center.z, 1.0f);
            QVector4D clipCenter = projMatrix * viewMatrix * worldCenter;

            // Skip objects behind the near plane (negative w for perspective, negative z for orthographic)
            if (isPerspective)
            {
                if (clipCenter.w() <= 0) continue;
            }
            else
            {
                if (clipCenter.z() <= -1.0f || clipCenter.z() >= 1.0f) continue; // Outside NDC z range
            }

            // Convert to NDC
            QVector3D ndcCenter;
            if (isPerspective)
            {
                ndcCenter = QVector3D(clipCenter.x() / clipCenter.w(),
                    clipCenter.y() / clipCenter.w(),
                    clipCenter.z() / clipCenter.w());
            }
            else
            {
                ndcCenter = QVector3D(clipCenter.x(), clipCenter.y(), clipCenter.z());
            }

            // Convert NDC to screen coordinates
            QVector2D screenCenter;
            screenCenter.setX(ndcCenter.x() * viewport.width() / 2 + viewport.width() / 2);
            screenCenter.setY(-ndcCenter.y() * viewport.height() / 2 + viewport.height() / 2);

            // Calculate screen space radius
            QVector4D worldRadiusPoint(center.x + radius, center.y, center.z, 1.0f);
            QVector4D clipRadiusPoint = projMatrix * viewMatrix * worldRadiusPoint;

            QVector3D ndcRadiusPoint;
            if (isPerspective)
            {
                if (clipRadiusPoint.w() <= 0) continue;
                ndcRadiusPoint = QVector3D(clipRadiusPoint.x() / clipRadiusPoint.w(),
                    clipRadiusPoint.y() / clipRadiusPoint.w(),
                    clipRadiusPoint.z() / clipRadiusPoint.w());
            }
            else
            {
                ndcRadiusPoint = QVector3D(clipRadiusPoint.x(), clipRadiusPoint.y(), clipRadiusPoint.z());
            }

            QVector2D screenRadiusPoint;
            screenRadiusPoint.setX(ndcRadiusPoint.x() * viewport.width() / 2 + viewport.width() / 2);
            screenRadiusPoint.setY(-ndcRadiusPoint.y() * viewport.height() / 2 + viewport.height() / 2);

            float screenRadius = (screenRadiusPoint - screenCenter).length();

            // Approximate the circle as a QRect
            QRect projectedBoundingRect(
                QPoint(screenCenter.x() - screenRadius, screenCenter.y() - screenRadius),
                QPoint(screenCenter.x() + screenRadius, screenCenter.y() + screenRadius)
            );

            // Refined intersection check
            if (rubberBandRect.contains(projectedBoundingRect))
            {
                // If the projected bounding rectangle is fully inside the rubberband rectangle
                m_selectedMeshIndices.insert(meshIndex);
            }
            else if (rubberBandRect.intersects(projectedBoundingRect))
            {
                // If there's an intersection, check how much of the projected rectangle is inside
                float overlapArea = computeOverlapArea(rubberBandRect, projectedBoundingRect);
                float projectedArea = projectedBoundingRect.width() * projectedBoundingRect.height();
                if ((overlapArea / projectedArea) > 0.5f)
                {
                    // Only select if at least 50% of the projected rectangle is inside
                    m_selectedMeshIndices.insert(meshIndex);
                }
            }
        }
        for (unsigned i = 0; i < node->mNumChildren; ++i)
            traverse(node->mChildren[i], transform);
        };
    traverse(m_scene->mRootNode, aiMatrix4x4());
    emit selectionChanged(m_selectedMeshIndices);
    update();
}

float ModelViewerWidget::computeOverlapArea(const QRect& rect1, const QRect& rect2)
{
    int x1 = std::max(rect1.left(), rect2.left());
    int y1 = std::max(rect1.top(), rect2.top());
    int x2 = std::min(rect1.right(), rect2.right());
    int y2 = std::min(rect1.bottom(), rect2.bottom());

    if (x2 > x1 && y2 > y1)
    {
        return (x2 - x1) * (y2 - y1); // Overlap area
    }

    return 0.0f; // No overlap
}

aiNode *ModelViewerWidget::findNodeForMesh(aiNode *node, int meshIndex)
{
    for (unsigned i = 0; i < node->mNumMeshes; ++i)
    {
        if (node->mMeshes[i] == meshIndex)
            return node;
    }

    for (unsigned i = 0; i < node->mNumChildren; ++i)
    {
        aiNode *result = findNodeForMesh(node->mChildren[i], meshIndex);
        if (result)
            return result;
    }

    return nullptr;
}

void ModelViewerWidget::setMeshVisibility(int meshIndex, bool visible)
{
    auto it = m_meshIndexToGLMesh.find(meshIndex);
    if (it != m_meshIndexToGLMesh.end())
    {
        GLMesh *glMesh = it->second;
        glMesh->setVisible(visible);
        update();
    } else
    {
        qDebug() << "Mesh index" << meshIndex << "not found.";
    }
}

// In ModelViewerWidget
void ModelViewerWidget::setSelection(const std::unordered_set<int> &meshIndices)
{
    m_selectedMeshIndices = meshIndices;
    update();
}

void ModelViewerWidget::clearSelection()
{
    m_selectedMeshIndices.clear();
    update();
}

void ModelViewerWidget::setupContextMenu()
{
    m_contextMenu = new QMenu(this);

    m_hideSelectedAction = m_contextMenu->addAction("Hide Selected",
                                                    this, &ModelViewerWidget::hideSelectedMeshes);
    m_showOnlySelectedAction = m_contextMenu->addAction("Show Only Selected",
                                                        this, &ModelViewerWidget::showOnlySelectedMeshes);
    m_centerSelectedAction = m_contextMenu->addAction("Center Selected",
                                                      this, &ModelViewerWidget::centerSelectedMeshes);
    m_separatorAction = m_contextMenu->addSeparator();
    m_showAllAction = m_contextMenu->addAction("Show All",
                                               this, &ModelViewerWidget::showAllMeshes);
}

void ModelViewerWidget::showContextMenu(const QPoint &position)
{
    // Get current keyboard modifiers
    Qt::KeyboardModifiers modifiers = QApplication::keyboardModifiers();

    // Only show context menu for pure right-click (no modifiers)
    if (modifiers != Qt::NoModifier)
    {
        return;
    }

    if (m_glMeshes.empty())
        return; // No meshes loaded, no context menu

    if (!m_contextMenu)
    {
        setupContextMenu();
    }

    bool hasSelection = !m_selectedMeshIndices.empty();
    int selectedCount = m_selectedMeshIndices.size();

    if (hasSelection)
    {
        // Update action text to show selection count
        m_hideSelectedAction->setText(QString("Hide Selected (%1)").arg(selectedCount));
        m_showOnlySelectedAction->setText(QString("Show Only Selected (%1)").arg(selectedCount));
        m_centerSelectedAction->setText(QString("Center Selected (%1)").arg(selectedCount));

        m_hideSelectedAction->setVisible(true);
        m_showOnlySelectedAction->setVisible(true);
        m_centerSelectedAction->setVisible(true);
        m_separatorAction->setVisible(true);
    } else
    {
        m_hideSelectedAction->setVisible(false);
        m_showOnlySelectedAction->setVisible(false);
        m_centerSelectedAction->setVisible(false);
        m_separatorAction->setVisible(false);
    }

    m_showAllAction->setVisible(checkIfAnyMeshIsHidden()); // Show "Show All" only if any mesh is hidden

    m_contextMenu->exec(mapToGlobal(position));
}

bool ModelViewerWidget::checkIfAnyMeshIsHidden() const
{
    // Check if any mesh is hidden
    for (const auto &[meshIndex, visible]: m_visibilityMap)
    {
        if (!visible)
        {
            return true;
        }
    }
    return false;
}

void ModelViewerWidget::hideSelectedMeshes()
{
    for (int meshIndex: m_selectedMeshIndices)
    {
        setMeshVisibility(meshIndex, false);
        m_visibilityMap[meshIndex] = false;
    }
    m_selectedMeshIndices.clear(); // Clear selection after hiding

    // Signal MainWindow to update tree checkboxes
    for (const auto &[meshIndex, visible]: m_visibilityMap)
    {
        emit meshVisibilityChanged(meshIndex, visible);
    }

    update();
}

void ModelViewerWidget::showOnlySelectedMeshes()
{
    for (const auto &pair: m_meshIndexToGLMesh)
    {
        pair.second->setVisible(false); // Hide all meshes
        m_visibilityMap[pair.first] = false;
    }
    for (int meshIndex: m_selectedMeshIndices)
    {
        setMeshVisibility(meshIndex, true); // Show only selected meshes
        m_visibilityMap[meshIndex] = true;
    }
    emit allMeshVisibilityChanged(m_visibilityMap);
    centerSelectedMeshes(); // Center on selected meshes
    clearSelection(); // Clear selection after showing
    update();
}

void ModelViewerWidget::centerSelectedMeshes()
{
    if (m_selectedMeshIndices.empty()) return;
    // Calculate the bounding box of selected meshes

    QVector3D sceneCenter;
    float sceneRadius = 0.0f;

    bool first = true;

    for (int meshIndex: m_selectedMeshIndices)
    {
        auto it = m_meshIndexToGLMesh.find(meshIndex);
        if (it != m_meshIndexToGLMesh.end())
        {
            GLMesh *glMesh = it->second;
            QVector3D center;
            float radius;
            glMesh->getBoundingSphere(center, radius);

            if (first)
            {
                sceneCenter = center;
                sceneRadius = radius;
                first = false;
                continue;
            }

            QVector3D toNew = center - sceneCenter;
            float dist = toNew.length();

            // If the new sphere is already inside the current one, skip
            if (dist + radius <= sceneRadius)
            {
                continue;
            }

            // If current sphere is inside the new one, adopt new sphere
            if (dist + sceneRadius <= radius)
            {
                sceneCenter = center;
                sceneRadius = radius;
                continue;
            }

            // Otherwise, compute new bounding sphere
            float newRadius = (sceneRadius + dist + radius) * 0.5f;
            QVector3D dir = toNew.normalized();
            if (dist > 1e-5f)
            {
                sceneCenter += dir * (newRadius - sceneRadius);
            }
            sceneRadius = newRadius;
        }
    }

    m_camera->setViewRange(sceneRadius * 2.1f);

    QVector3D viewPos = sceneCenter;
    m_camera->setPosition(viewPos);

    m_viewMatrix = m_camera->getViewMatrix();
    m_projectionMatrix = m_camera->getProjectionMatrix();

    m_sceneUpdated = true;
    update();
}

void ModelViewerWidget::showAllMeshes()
{
    for (const auto &pair: m_meshIndexToGLMesh)
    {
        pair.second->setVisible(true); // Hide all meshes
        m_visibilityMap[pair.first] = true;
    }
    fitToView(); // Reset camera to fit all meshes
    // Signal MainWindow to check all tree checkboxes
    emit allMeshVisibilityChanged(m_visibilityMap);
}

QVector3D ModelViewerWidget::get3dTranslationVectorFromMousePoints(const QPoint &start, const QPoint &end)
{
    QRect viewport(0, 0, width(), height());
    GLCamera *camera = m_camera;
    QMatrix4x4 view = camera->getViewMatrix();
    QMatrix4x4 projection = camera->getProjectionMatrix();
    QMatrix4x4 inv = (projection * view).inverted();

    float ndcZ = 0.0f;
    if (camera->getProjectionType() == GLCamera::ProjectionType::ORTHOGRAPHIC)
    {
        QVector4D refWorld(0, 0, m_viewCenter.z(), 1.0f);
        QVector4D refClip = projection * view * refWorld;
        ndcZ = refClip.w() != 0.0f ? refClip.z() / refClip.w() : 0.0f;
    } else
    {
        // Project the actual model center to get its NDC Z
        QVector4D modelCenterWorld(m_viewCenter.x(), m_viewCenter.y(), m_viewCenter.z(), 1.0f);
        QVector4D modelCenterClip = projection * view * modelCenterWorld;
        ndcZ = modelCenterClip.w() != 0.0f ? modelCenterClip.z() / modelCenterClip.w() : 0.0f;
    }

    // Convert screen points to NDC
    auto toNDC = [&](const QPoint &pt) {
        int yInverted = height() - pt.y() - 1;
        float ndcX = (2.0f * (pt.x() - viewport.x())) / viewport.width() - 1.0f;
        float ndcY = (2.0f * (yInverted - viewport.y())) / viewport.height() - 1.0f;
        return QVector2D(ndcX, ndcY);
    };

    QVector2D ndcStart2 = toNDC(start);
    QVector2D ndcEnd2 = toNDC(end);
    QVector4D ndcStart(ndcStart2.x(), ndcStart2.y(), ndcZ, 1.0f);
    QVector4D ndcEnd(ndcEnd2.x(), ndcEnd2.y(), ndcZ, 1.0f);

    QVector4D worldStart = inv * ndcStart;
    QVector4D worldEnd = inv * ndcEnd;

    if (worldStart.w() != 0.0f) worldStart /= worldStart.w();
    if (worldEnd.w() != 0.0f) worldEnd /= worldEnd.w();

    return worldEnd.toVector3D() - worldStart.toVector3D();
}
