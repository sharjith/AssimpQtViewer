#define NOMINMAX

#include "ModelViewerWidget.h"
#include <assimp/postprocess.h>
#include <cfloat>
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

#ifdef _WIN32
#include <windows.h>
#endif
#include <GL/gl.h>
#include <GL/glu.h>

#include <algorithm>


ModelViewerWidget::ModelViewerWidget(QWidget *parent)
    : QOpenGLWidget(parent) {
    QSurfaceFormat fmt;
    fmt.setDepthBufferSize(24);
    fmt.setVersion(3, 3);
    fmt.setProfile(QSurfaceFormat::CompatibilityProfile);
    QSurfaceFormat::setDefaultFormat(fmt);

    m_viewRadius = 1000;
	m_cameraDistance = 500;
    m_sceneUpdated = false;
	m_zoom = 1.0f;  
    float m_azimuth = 0.0f;     // Horizontal angle in degrees
    float m_elevation = 20.0f;  // Vertical angle in degrees
    m_inertiaTimer = new QTimer(this);
    m_inertiaTimer->setInterval(16); // ~60 FPS
    connect(m_inertiaTimer, &QTimer::timeout, this, &ModelViewerWidget::onInertiaTimeout);

    _viewToolbar = new QWidget(this);
    _viewToolbar->setAttribute(Qt::WA_TransparentForMouseEvents, false);
    _viewToolbar->setStyleSheet("background: rgba(255, 255, 255, 100); border: 1px solid gray; border-radius: 4px;");
    _viewToolbar->setFixedHeight(64);    

    QHBoxLayout* layout = new QHBoxLayout(_viewToolbar);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(6);

    layout->addWidget(createViewButton(":/icons/res/top.png", "Top View", [this]() { setViewTop(); }, _viewToolbar));
    layout->addWidget(createViewButton(":/icons/res/front.png", "Front View", [this]() { setViewFront(); }, _viewToolbar));
    layout->addWidget(createViewButton(":/icons/res/left.png", "Left View", [this]() { setViewLeft(); }, _viewToolbar));
    layout->addWidget(createViewButton(":/icons/res/isometric.png", "Isometric View", [this]() { setViewAxonometric(); }, _viewToolbar));
    layout->addWidget(createViewButton(":/icons/res/fit-all.png", "Fit All", [this]() { fitToView(); }, _viewToolbar));
    
    setFocusPolicy(Qt::StrongFocus);
}

QToolButton* ModelViewerWidget::createViewButton(const QString& iconPath, const QString& tooltip, const std::function<void()>& callback, QWidget* parent) {
    QToolButton* button = new QToolButton(parent);
    button->setIcon(QIcon(iconPath));
    button->setIconSize(QSize(64, 64));
    button->setToolTip(tooltip);
    button->setAutoRaise(true);  // Flat appearance
    QObject::connect(button, &QToolButton::clicked, callback);
    return button;
}


void ModelViewerWidget::initializeGL() {
    initializeOpenGLFunctions();
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_COLOR_MATERIAL);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_NORMALIZE); // Normalize normals for non-uniform scaling
    glEnable(GL_COLOR_MATERIAL);

    glColorMaterial(GL_FRONT_AND_BACK, GL_DIFFUSE);

    // Set light position and color
    GLfloat lightPos[] = { 0.0f, 0.0f, m_viewRadius, 0.0f };
    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);

    GLfloat lightColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightColor);
    glLightfv(GL_LIGHT0, GL_SPECULAR, lightColor);

    GLfloat materialAmbient[] = { 0.2f, 0.2f, 0.2f, 1.0f };
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, materialAmbient);

    // Set specular material color
    GLfloat materialSpecular[] = { 0.5f, 0.5f, 0.5f, 1.0f };
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, materialSpecular);

    // Set shininess (range: 0 to 128; higher = smaller, sharper highlight)
    GLfloat shininess = 64.0f;
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, shininess);

    glShadeModel(GL_SMOOTH);
	glEnable(GL_NORMALIZE); // Normalize normals for non-uniform scaling

    float fovYRadians = 45.0f * M_PI / 180.0f;
    m_cameraDistance = m_viewRadius / std::tan(fovYRadians * 0.5f);
}

void ModelViewerWidget::resizeGL(int w, int h) {
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    float nearPlane = m_viewRadius * 0.1f;
    float farPlane = m_viewRadius * 10.0f;
    gluPerspective(45.0, float(w)/h, nearPlane, farPlane);
    glMatrixMode(GL_MODELVIEW);
}

void computeBoundingBox(const aiScene* scene, const aiNode* node,
    aiVector3D& minimum, aiVector3D& maximum, const aiMatrix4x4& transform)
{
    aiMatrix4x4 currentTransform = transform * node->mTransformation;

    for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
        const aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];

        for (unsigned int j = 0; j < mesh->mNumVertices; ++j) {
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

    for (unsigned int i = 0; i < node->mNumChildren; ++i) {
        computeBoundingBox(scene, node->mChildren[i], minimum, maximum, currentTransform);
    }
}

void ModelViewerWidget::updateCamera() {
	// Update camera position based on the current rotation and zoom
    if (!m_scene) {        
        return;
    }
    // After computing bounding box:
    aiVector3D minimum(FLT_MAX, FLT_MAX, FLT_MAX);
    aiVector3D maximum(-FLT_MAX, -FLT_MAX, -FLT_MAX);

    aiMatrix4x4 identity;
    computeBoundingBox(m_scene, m_scene->mRootNode, minimum, maximum, identity);
    float maxExtent = std::max({ maximum.x - minimum.x, maximum.y - minimum.y, maximum.z - minimum.z });
	m_viewCenter = (maximum + minimum) * 0.5f;
    m_viewRadius = maxExtent * 0.5f;

    // Ideal distance from camera to model center based on FOV
    float fovYRadians = 45.0f * M_PI / 180.0f;
    m_cameraDistance = m_viewRadius / std::tan(fovYRadians * 0.5f);

    // Set default camera position: looking from +Z axis
    m_cameraPos = aiVector3D(m_viewCenter.x, m_viewCenter.y, m_viewCenter.z + m_cameraDistance);
    m_upVector = aiVector3D(0, 1, 0);

    m_sceneUpdated = true;
    m_zoom = 1.0f;
}

void ModelViewerWidget::paintGL() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glClearColor(0.05f, 0.05f, 0.05f, 1.0f);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    float nearPlane = m_viewRadius * 0.1f;
    float farPlane = m_viewRadius * 10.0f;
    gluPerspective(45.0, float(width()) / height(), nearPlane, farPlane);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Convert spherical coordinates to Cartesian
    float radAzim = qDegreesToRadians(m_azimuth);
    float radElev = qDegreesToRadians(m_elevation);
    float x = m_cameraDistance * m_zoom * std::cos(radElev) * std::sin(radAzim);
    float y = m_cameraDistance * m_zoom * std::sin(radElev);
    float z = m_cameraDistance * m_zoom * std::cos(radElev) * std::cos(radAzim);

    aiVector3D up = (std::cos(radElev) >= 0) ? aiVector3D(0, 1, 0) : aiVector3D(0, -1, 0);

    gluLookAt(
        x + m_viewCenter.x, y + m_viewCenter.y, z + m_viewCenter.z,  // camera position
        m_viewCenter.x, m_viewCenter.y, m_viewCenter.z,              // target
        up.x, up.y, up.z                                          // up vector
    );
        
    if (!m_scene) {
        // draw test triangle
        glBegin(GL_TRIANGLES);
        glColor3f(1, 0, 0); glVertex3f(0, 1, 0);
        glColor3f(0, 1, 0); glVertex3f(-1, -1, 0);
        glColor3f(0, 0, 1); glVertex3f(1, -1, 0);
        glEnd();
        return;
    }

    if (m_scene->mRootNode)
        drawNode(m_scene->mRootNode);
}

void ModelViewerWidget::loadModel(const QString &filePath) {
    
    if (m_scene)
    {
		m_importer.FreeScene();
        m_scene = nullptr;
    }
	QFileInfo fileInfo(filePath);
	m_lastModelPath = fileInfo.absolutePath();
	resetView();
    for (auto texId : m_materialTextureCache) {
        if (texId.second) glDeleteTextures(1, &texId.second);
    }
    m_materialTextureCache.clear();
    m_scene = m_importer.ReadFile(filePath.toStdString(), 
        aiProcess_Triangulate | aiProcess_ValidateDataStructure | 
        aiProcess_CalcTangentSpace | aiProcess_GenSmoothNormals |
        aiProcess_FixInfacingNormals | aiProcess_JoinIdenticalVertices |
        aiProcess_OptimizeMeshes | aiProcess_GenUVCoords | aiProcess_SortByPType);

    if (!m_scene || m_scene->mFlags == AI_SCENE_FLAGS_INCOMPLETE || !m_scene->mRootNode) // if is Not Zero
    {        
        qDebug() << "ERROR::ASSIMP:: " << m_importer.GetErrorString();
		m_scene = nullptr;
        return;
    }

    updateCamera();
    update();
}


GLuint ModelViewerWidget::loadTextureIfNeeded(const aiMaterial* material, unsigned int materialIndex)
{
    if (m_materialTextureCache.find(materialIndex) != m_materialTextureCache.end())
        return m_materialTextureCache[materialIndex];

    if (material->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
        aiString texturePath;
        if (material->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath) == AI_SUCCESS) {
            QString fullPath = QDir(m_lastModelPath).filePath(QString::fromUtf8(texturePath.C_Str()));
            QImage image(fullPath);
            if (!image.isNull()) {
                image = image.convertToFormat(QImage::Format_RGBA8888).mirrored();
                GLuint texId;
                glGenTextures(1, &texId);
                glBindTexture(GL_TEXTURE_2D, texId);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width(), image.height(), 0,
                    GL_RGBA, GL_UNSIGNED_BYTE, image.bits());
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glGenerateMipmap(GL_TEXTURE_2D);
                m_materialTextureCache[materialIndex] = texId;
                return texId;
            }
        }
    }

    m_materialTextureCache[materialIndex] = 0; // No texture
    return 0;
}


void ModelViewerWidget::highlightNode(aiNode *node) {
    m_highlightedNode = node;
    update();
}

void ModelViewerWidget::drawNode(aiNode* node)
{
    for (unsigned i = 0; i < node->mNumMeshes; ++i) {
        const aiMesh* mesh = m_scene->mMeshes[node->mMeshes[i]];
        if (!mesh) continue;

        const aiMaterial* material = m_scene->mMaterials[mesh->mMaterialIndex];
        GLuint texId = loadTextureIfNeeded(material, mesh->mMaterialIndex);

        if (texId) {
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, texId);
        }
        else {
            glDisable(GL_TEXTURE_2D);
        }

        aiColor4D diffuse;
        if (AI_SUCCESS == aiGetMaterialColor(material, AI_MATKEY_COLOR_DIFFUSE, &diffuse)) {
            glColor4f(diffuse.r, diffuse.g, diffuse.b, diffuse.a);
        }
        else {
            glColor4f(0.8f, 0.8f, 0.8f, 1.0f);
        }

        if(node == m_highlightedNode && !texId) {
			// Set full specular material color for highlighted node
            GLfloat materialSpecular[] = { 1.0f, 1.0f, 1.0f, 1.0f };
            glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, materialSpecular);
		}
		else {
            // Set default specular color
            GLfloat materialSpecular[] = { 0.5f, 0.5f, 0.5f, 1.0f };
			glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, materialSpecular); 
		}

        glBegin(GL_TRIANGLES);
        for (unsigned int j = 0; j < mesh->mNumFaces; ++j) {
            const aiFace& face = mesh->mFaces[j];
            for (unsigned int k = 0; k < face.mNumIndices; ++k) {
                unsigned int index = face.mIndices[k];
                if (mesh->HasNormals()) {
                    const aiVector3D& n = mesh->mNormals[index];
                    glNormal3f(n.x, n.y, n.z);
                }
                if (mesh->HasTextureCoords(0)) {
                    const aiVector3D& uv = mesh->mTextureCoords[0][index];
                    glTexCoord2f(uv.x, uv.y);
                }

                if (node == m_highlightedNode && !texId)
                    glColor3d(204, 255, 0); // Flourescent Yellow highlight

                const aiVector3D& v = mesh->mVertices[index];
                glVertex3f(v.x, v.y, v.z);
            }
        }
        glEnd();       
		glDisable(GL_TEXTURE_2D);

        // Optional second pass: wireframe highlight
        if (node == m_highlightedNode && texId) {
            glPushAttrib(GL_ENABLE_BIT | GL_POLYGON_BIT | GL_LINE_BIT);

            glDisable(GL_LIGHTING);
            glDisable(GL_TEXTURE_2D);
            glEnable(GL_POLYGON_OFFSET_LINE);
            glPolygonOffset(-1.0f, -1.0f); // Pull forward to avoid z-fighting

            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            glLineWidth(0.50f);
            glColor3d(204, 255, 0); // Flourescent Yellow highlight           
            glBegin(GL_TRIANGLES);
            for (unsigned int j = 0; j < mesh->mNumFaces; ++j) {
                const aiFace& face = mesh->mFaces[j];
                for (unsigned int k = 0; k < face.mNumIndices; ++k) {
                    unsigned int index = face.mIndices[k];
                    const aiVector3D& v = mesh->mVertices[index];
                    glVertex3f(v.x, v.y, v.z);
                }
            }
            glEnd();

            glPopAttrib();
        }

        if (texId)
            glBindTexture(GL_TEXTURE_2D, 0);
    }

    for (unsigned i = 0; i < node->mNumChildren; ++i)
        drawNode(node->mChildren[i]);
}


void ModelViewerWidget::mousePressEvent(QMouseEvent* event)
{
    m_isDragging = true;
    if (m_inertiaTimer->isActive())
        m_inertiaTimer->stop();

    m_azimuthSpeed = 0.f;
    m_elevationSpeed = 0.f;
    m_panSpeed = { 0, 0 };
    m_zoomSpeed = 0.f;

    m_lastMousePos = event->pos();

    if ((event->modifiers() & Qt::ControlModifier))
    {
        if (event->button() == Qt::LeftButton)
            m_mode = InteractionMode::Rotate;
        else if (event->button() == Qt::RightButton)
            m_mode = InteractionMode::Pan;
        else if (event->button() == Qt::MiddleButton)
            m_mode = InteractionMode::Zoom;       
    }
    else
        m_mode = InteractionMode::Select;

    if (event->button() == Qt::LeftButton && m_mode == InteractionMode::Select) {
        pickAtScreenPosition(event->pos());
    }
}


void ModelViewerWidget::mouseMoveEvent(QMouseEvent* event)
{
    QPoint delta = event->pos() - m_lastMousePos;
    m_lastMousePos = event->pos();

    if (m_mode == InteractionMode::Rotate) {
        m_azimuth -= delta.x() * 0.5f;
        m_elevation += delta.y() * 0.5f;
        
        // Save speeds for inertia
        m_azimuthSpeed = -delta.x() * 0.5f;
        m_elevationSpeed = delta.y() * 0.5f;
    }
    else if (m_mode == InteractionMode::Pan) {
        float radAzim = qDegreesToRadians(m_azimuth);
        float radElev = qDegreesToRadians(m_elevation);

        // Right vector in world space
        aiVector3D right(std::cos(radAzim), 0, -std::sin(radAzim));

        // Approximate up vector (in camera space, global Y up)
        aiVector3D up(0, 1, 0);

        right.Normalize();
        up.Normalize();

        // Pan speed scaled by distance
        float panSpeed = m_cameraDistance * 0.001f;

        // Apply panning to the view center
        m_viewCenter -= right * (delta.x() * panSpeed);
        m_viewCenter += up * (delta.y() * panSpeed);

        // Save pan speed
        m_panSpeed = QPointF(delta.x() * panSpeed, delta.y() * panSpeed);
    }
    else if (m_mode == InteractionMode::Zoom) {
        float zoomDelta = delta.y() * 0.01f;
        m_zoom *= std::exp(-zoomDelta);
        m_zoom = std::clamp(m_zoom, 0.01f, 10.0f);

        m_zoomSpeed = -zoomDelta;
    }

    update();
}

void ModelViewerWidget::mouseReleaseEvent(QMouseEvent* event)
{
    m_isDragging = false;
    if (!m_inertiaTimer->isActive())
        m_inertiaTimer->start();

    Q_UNUSED(event);
    m_mode = InteractionMode::Select;
}

void ModelViewerWidget::wheelEvent(QWheelEvent* event)
{
    QPoint numDegrees = event->angleDelta() / 8;
    if (!numDegrees.isNull()) {
        m_zoomSpeed += numDegrees.y() / 800.0f;

        if (!m_inertiaTimer->isActive())
            m_inertiaTimer->start();
    }
    update();
}

void ModelViewerWidget::keyPressEvent(QKeyEvent* event) {
    switch (event->key()) {
    case Qt::Key_T:  // Top view (looking down -Y axis)
		setViewProjection(ViewProjection::Top);
        break;
    case Qt::Key_F:  // Front view (looking along +Z)
		setViewProjection(ViewProjection::Front);
        break;
    case Qt::Key_L:  // Left view (looking along -X)
		setViewProjection(ViewProjection::Left);
        break;
    case Qt::Key_A:  // Axonometric (isometric) view
		setViewProjection(ViewProjection::Axonometric);
        break;
    case Qt::Key_H:  // Home / Fit all
        updateCamera();
        break;
    }
    update();
}

void ModelViewerWidget::resizeEvent(QResizeEvent* event) {
    QOpenGLWidget::resizeEvent(event);
    QWidget::resizeEvent(event);
    if (_viewToolbar) {
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


void ModelViewerWidget::resetView() {
    m_rotationX = 0.0f;
    m_rotationY = 0.0f;
    m_panX = 0.0f;
    m_panY = 0.0f;
    m_zoom = 1.0f;
    m_cameraDistance = 0.0f;
    m_viewCenter = aiVector3D(0, 0, 0);
    m_viewRadius = 1.0f;
}

void ModelViewerWidget::setViewProjection(ViewProjection view)
{
    m_viewProjection = view;
    switch (view) {
    case ViewProjection::Top:
		setViewTop();
        break;
    case ViewProjection::Front:
		setViewFront();
        break;
    case ViewProjection::Left:
		setViewLeft();
        break;
    case ViewProjection::Axonometric:
		setViewAxonometric();        
        break;
    case ViewProjection::Custom:
    default:
        // Do nothing or reset to user-controlled
        break;
    }
    m_zoom = 1.0f;
    update();
}


void ModelViewerWidget::setViewTop() {
    m_azimuth = 0;
    m_elevation = 90;   // looking straight down
    m_zoom = 1.0f;
    update();
}

void ModelViewerWidget::setViewFront() {
    m_azimuth = 0;
    m_elevation = 0;
    m_zoom = 1.0f;
    update();
}

void ModelViewerWidget::setViewLeft() {
    m_azimuth = 90;
    m_elevation = 0;
    m_zoom = 1.0f;
    update();
}

void ModelViewerWidget::setViewAxonometric() {
    m_azimuth = 45;
    m_elevation = 35;
    m_zoom = 1.0f;
    update();
}

void ModelViewerWidget::fitToView() {
    // Optional: adjust _cameraDistance or zoom to fit model bounds
	updateCamera();
    update();
}

void ModelViewerWidget::onInertiaTimeout()
{
    if (m_isDragging) return;

    constexpr float damping = 0.90f;

    // Rotation inertia
    m_azimuth += m_azimuthSpeed;
    m_elevation += m_elevationSpeed;
    m_azimuthSpeed *= damping;
    m_elevationSpeed *= damping;
    
    // Pan inertia
    if (!m_panSpeed.isNull()) {
        float radAzim = qDegreesToRadians(m_azimuth);
        aiVector3D right(std::cos(radAzim), 0, -std::sin(radAzim));
        aiVector3D up(0, 1, 0);
        right.Normalize();
        up.Normalize();

        m_viewCenter -= right * static_cast<float>(m_panSpeed.x());
        m_viewCenter += up * static_cast<float>(m_panSpeed.y());

        m_panSpeed *= damping;
        if (std::abs(m_panSpeed.x()) < 1e-5f && std::abs(m_panSpeed.y()) < 1e-5f)
            m_panSpeed = { 0, 0 };
    }

    // Zoom inertia
    if (std::abs(m_zoomSpeed) > 1e-5f) {
        m_zoom *= std::exp(m_zoomSpeed);
        m_zoom = std::clamp(m_zoom, 0.01f, 10.0f);
        m_zoomSpeed *= damping;

        if (std::abs(m_zoomSpeed) < 1e-5f)
            m_zoomSpeed = 0.f;
    }

    // Stop if everything has slowed
    if (std::abs(m_azimuthSpeed) < 0.01f &&
        std::abs(m_elevationSpeed) < 0.01f &&
        m_panSpeed.isNull() &&
        std::abs(m_zoomSpeed) < 1e-5f)
    {
        m_inertiaTimer->stop();
    }

    update();
}

void ModelViewerWidget::pickAtScreenPosition(const QPoint& pos) {
    makeCurrent(); // Needed if using Qt with OpenGL

    GLint viewport[4];
    GLdouble modelview[16], projection[16];
    glGetDoublev(GL_MODELVIEW_MATRIX, modelview);
    glGetDoublev(GL_PROJECTION_MATRIX, projection);
    glGetIntegerv(GL_VIEWPORT, viewport);

    float x = pos.x();
    float y = viewport[3] - pos.y(); // Flip Y for OpenGL

    // Near and far points
    double nearX, nearY, nearZ;
    double farX, farY, farZ;

    gluUnProject(x, y, 0.0, modelview, projection, viewport, &nearX, &nearY, &nearZ);
    gluUnProject(x, y, 1.0, modelview, projection, viewport, &farX, &farY, &farZ);

    aiVector3D rayOrigin((float)nearX, (float)nearY, (float)nearZ);
    aiVector3D rayDirection((float)(farX - nearX), (float)(farY - nearY), (float)(farZ - nearZ));
    rayDirection.Normalize();

    pickRay(rayOrigin, rayDirection);
}

void ModelViewerWidget::pickRay(const aiVector3D& origin, const aiVector3D& dir) {
    float minDistance = std::numeric_limits<float>::max();
    int hitMeshIndex = -1;
    aiMatrix4x4 hitTransform;

    // Track intersected mesh index
    std::function<void(aiNode*, const aiMatrix4x4&)> traverse;
    traverse = [&](aiNode* node, const aiMatrix4x4& parentTransform) {
        aiMatrix4x4 transform = parentTransform * node->mTransformation;

        for (unsigned i = 0; i < node->mNumMeshes; ++i) {
            const int meshIndex = node->mMeshes[i];
            const aiMesh* mesh = m_scene->mMeshes[meshIndex];

            for (unsigned f = 0; f < mesh->mNumFaces; ++f) {
                const aiFace& face = mesh->mFaces[f];
                if (face.mNumIndices != 3) continue;

                aiVector3D v0 = mesh->mVertices[face.mIndices[0]];
                aiVector3D v1 = mesh->mVertices[face.mIndices[1]];
                aiVector3D v2 = mesh->mVertices[face.mIndices[2]];

                v0 *= transform;
                v1 *= transform;
                v2 *= transform;

                float t;
                if (rayIntersectsTriangle(origin, dir, v0, v1, v2, t)) {
                    if (t < minDistance) {
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

    if (hitMeshIndex != -1) {
        aiNode* hitNode = findNodeForMesh(m_scene->mRootNode, hitMeshIndex);
        if (m_lastPickedNode == hitNode) {
            emit nodePicked(nullptr); // Signal to deselect
            m_lastPickedNode = nullptr;
        }
        else {
            emit nodePicked(hitNode);
            m_lastPickedNode = hitNode;
        }
		highlightNode(m_lastPickedNode);
    }
    else {
        highlightNode(nullptr);
    }
}


bool ModelViewerWidget::rayIntersectsTriangle(
    const aiVector3D& orig, const aiVector3D& dir,
    const aiVector3D& v0, const aiVector3D& v1, const aiVector3D& v2,
    float& outT
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
    if (t > EPSILON) {
        outT = t;
        return true;
    }

    return false;
}

aiNode* ModelViewerWidget::findNodeForMesh(aiNode* node, int meshIndex) {
    for (unsigned i = 0; i < node->mNumMeshes; ++i) {
        if (node->mMeshes[i] == meshIndex)
            return node;
    }

    for (unsigned i = 0; i < node->mNumChildren; ++i) {
        aiNode* result = findNodeForMesh(node->mChildren[i], meshIndex);
        if (result)
            return result;
    }

    return nullptr;
}


