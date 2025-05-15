#define NOMINMAX

#include "ModelViewerWidget.h"
#include <QMatrix4x4>
#include <QMouseEvent>
#include <QDebug>
#include <assimp/postprocess.h>
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

    _viewRadius = 1000;
	_cameraDistance = 500;
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

    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

    // Set light position and color
    GLfloat lightPos[] = { 0.0f, 0.0f, 1.0f, 0.0f };
    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);

    GLfloat lightColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightColor);
    glLightfv(GL_LIGHT0, GL_SPECULAR, lightColor);

    float fovYRadians = 45.0f * M_PI / 180.0f;
    _cameraDistance = _viewRadius / std::tan(fovYRadians * 0.5f);
}

void ModelViewerWidget::resizeGL(int w, int h) {
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    float nearPlane = _viewRadius * 0.1f;
    float farPlane = _viewRadius * 10.0f;
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
    if (!scene) {        
        return;
    }
    // After computing bounding box:
    aiVector3D minimum(FLT_MAX, FLT_MAX, FLT_MAX);
    aiVector3D maximum(-FLT_MAX, -FLT_MAX, -FLT_MAX);

    aiMatrix4x4 identity;
    computeBoundingBox(scene, scene->mRootNode, minimum, maximum, identity);
    float maxExtent = std::max({ maximum.x - minimum.x, maximum.y - minimum.y, maximum.z - minimum.z });
	_viewCenter = (maximum + minimum) * 0.5f;
    _viewRadius = maxExtent * 0.5f;

    // Ideal distance from camera to model center based on FOV
    float fovYRadians = 45.0f * M_PI / 180.0f;
    _cameraDistance = _viewRadius / std::tan(fovYRadians * 0.5f);
}

void ModelViewerWidget::paintGL() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    // Apply transform    	
    glTranslatef(0, 0, -_cameraDistance * m_zoom);
    glTranslatef(m_panX, m_panY, 0);
    glRotatef(m_rotationX, 1, 0, 0);
    glRotatef(m_rotationY, 0, 1, 0);

    glTranslatef(-_viewCenter.x, -_viewCenter.y, -_viewCenter.z);

    if (!scene) {
        // draw test triangle
        glBegin(GL_TRIANGLES);
        glColor3f(1, 0, 0); glVertex3f(0, 1, 0);
        glColor3f(0, 1, 0); glVertex3f(-1, -1, 0);
        glColor3f(0, 0, 1); glVertex3f(1, -1, 0);
        glEnd();
        return;
    }

    if (scene->mRootNode)
        drawNode(scene->mRootNode);
}

void ModelViewerWidget::loadModel(const QString &filePath) {
    scene = importer.ReadFile(filePath.toStdString(), aiProcess_Triangulate | aiProcess_GenNormals);
    updateCamera();
	resizeGL(width(), height());
    update();
}

void ModelViewerWidget::highlightNode(aiNode *node) {
    highlightedNode = node;
    update();
}

void ModelViewerWidget::drawNode(aiNode *node) {
    for (unsigned i = 0; i < node->mNumMeshes; ++i) {
        const aiMesh *mesh = scene->mMeshes[node->mMeshes[i]];
        if (!mesh) continue;

        const aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];

        aiColor4D diffuse;
        if (AI_SUCCESS == aiGetMaterialColor(material, AI_MATKEY_COLOR_DIFFUSE, &diffuse)) {
            glColor4f(diffuse.r, diffuse.g, diffuse.b, diffuse.a);
        }
        else {
            glColor4f(0.8f, 0.8f, 0.8f, 1.0f); // default gray
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
                const aiVector3D& v = mesh->mVertices[index];
                if (node == highlightedNode)
                    glColor3f(1, 1, 0); // highlight                
                glVertex3f(v.x, v.y, v.z);
            }
        }
        glEnd();
    }

    for (unsigned i = 0; i < node->mNumChildren; ++i)
        drawNode(node->mChildren[i]);
}

void ModelViewerWidget::mousePressEvent(QMouseEvent* event)
{
    m_lastMousePos = event->pos();

    if (event->button() == Qt::LeftButton)
        m_mode = InteractionMode::Rotate;
    else if (event->button() == Qt::RightButton)
        m_mode = InteractionMode::Pan;
}


void ModelViewerWidget::mouseMoveEvent(QMouseEvent* event)
{
    QPoint delta = event->pos() - m_lastMousePos;
    m_lastMousePos = event->pos();

    if (m_mode == InteractionMode::Rotate) {
        m_rotationX += delta.y();
        m_rotationY += delta.x();
    }
    else if (m_mode == InteractionMode::Pan) {
        m_panX += delta.x() * 0.5f;
        m_panY -= delta.y() * 0.5f;
    }

    update();
}

void ModelViewerWidget::mouseReleaseEvent(QMouseEvent* event)
{
    Q_UNUSED(event);
    m_mode = InteractionMode::None;
}

void ModelViewerWidget::wheelEvent(QWheelEvent* event)
{
    QPoint numDegrees = event->angleDelta() / 8;
    if (!numDegrees.isNull()) {
        m_zoom *= 1.0f + numDegrees.y() / 240.0f;
    }
    update();
}
