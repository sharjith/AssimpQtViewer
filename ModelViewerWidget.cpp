#define NOMINMAX

#include "ModelViewerWidget.h"
#include "GLCamera.h"
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


ModelViewerWidget::ModelViewerWidget(QWidget* parent)
	: QOpenGLWidget(parent) {
	QSurfaceFormat fmt;
	fmt.setDepthBufferSize(24);
	fmt.setVersion(3, 3);
	fmt.setProfile(QSurfaceFormat::CompatibilityProfile);
	QSurfaceFormat::setDefaultFormat(fmt);

	m_modelMatrix.setToIdentity();
	m_viewMatrix.setToIdentity();
	m_projectionMatrix.setToIdentity();

	m_viewRadius = 1000;
	m_cameraDistance = 500;
	m_sceneUpdated = false;
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

	m_camera = new GLCamera(height(), width(), m_viewRadius, 45);
	m_camera->setProjectionType(GLCamera::ProjectionType::ORTHOGRAPHIC);
	m_camera->setView(GLCamera::ViewProjection::SE_ISOMETRIC_VIEW);
}

void ModelViewerWidget::resizeGL(int w, int h) {
	glViewport(0, 0, w, h);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	if (h == 0) h = 1; // Prevent division by zero

	m_camera->setScreenSize(w, h);
	m_camera->setViewRange(m_viewRadius * 2.1f);
	m_camera->setProjectionType(GLCamera::ProjectionType::ORTHOGRAPHIC);
	m_viewMatrix = m_camera->getViewMatrix();
	m_projectionMatrix = m_camera->getProjectionMatrix();
	glMatrixMode(GL_MODELVIEW);
}

void ModelViewerWidget::paintGL() {

	glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	drawGradientBackground();

	m_viewMatrix.setToIdentity();
	m_viewMatrix = m_camera->getViewMatrix();
	m_projectionMatrix = m_camera->getProjectionMatrix();

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glLoadMatrixf(m_projectionMatrix.constData());

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
		
	QMatrix4x4 modelViewMatrix = m_viewMatrix * m_modelMatrix;
	glLoadMatrixf(modelViewMatrix.constData());

	if (m_scene) {

		if (m_scene->mRootNode)
			drawNode(m_scene->mRootNode);
	}

	drawTrihedronOverlay();
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
	m_cameraPos = aiVector3D(m_viewCenter.x, m_viewCenter.y, m_viewCenter.z);
	m_upVector = aiVector3D(0, 1, 0);

	m_camera->setViewRange(m_viewRadius * 2.1f);

	QVector3D viewPos(m_viewCenter.x, m_viewCenter.y, m_viewCenter.z);

	m_camera->setPosition(viewPos);
	m_camera->setZoom(1.0f);

	m_viewMatrix = m_camera->getViewMatrix();
	m_projectionMatrix = m_camera->getProjectionMatrix();

	m_sceneUpdated = true;
}

void ModelViewerWidget::computeBoundingBox(const aiScene* scene, const aiNode* node,
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

void ModelViewerWidget::drawGradientBackground() {
	glPushAttrib(GL_ENABLE_BIT | GL_TRANSFORM_BIT | GL_CURRENT_BIT); // Save state

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_LIGHTING); // Disable lighting if enabled by model draw

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glOrtho(-1, 1, -1, 1, -1, 1);

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	glBegin(GL_QUADS);
	glColor3f(0.3f, 0.3f, 0.3f); glVertex2f(-1.0f, 1.0f);   // Top left
	glColor3f(0.55f, 0.55f, 0.55f); glVertex2f(1.0f, 1.0f);    // Top right
	glColor3f(0.95f, 0.95f, 0.95f); glVertex2f(1.0f, -1.0f);   // Bottom right
	glColor3f(0.7f, 0.7f, 0.7f); glVertex2f(-1.0f, -1.0f);     // Bottom left
	glEnd();

	glPopMatrix(); // MODELVIEW
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);

	glPopAttrib(); // Restore GL state
}

void ModelViewerWidget::drawTrihedron(float axisLength, float axisRadius, float coneHeight, float coneRadius, float sphereRadius) {
	GLUquadric* quad = gluNewQuadric();

	glPushAttrib(GL_ENABLE_BIT | GL_CURRENT_BIT | GL_LIGHTING_BIT | GL_TRANSFORM_BIT);
	glPushMatrix();

	glDisable(GL_LIGHTING);

	// Draw center sphere
	glColor3f(0.8f, 0.8f, 0.8f);
	gluSphere(quad, sphereRadius, 16, 16);

	// ===== X Axis (Red) =====
	glColor3f(1.0f, 0.0f, 0.0f);  // Red
	glPushMatrix();
	glRotatef(90, 0, 1, 0); // Point along +X
	gluCylinder(quad, axisRadius, axisRadius, axisLength, 12, 1);
	glTranslatef(0, 0, axisLength);
	gluCylinder(quad, coneRadius, 0.0, coneHeight, 12, 1); // Cone tip
	glPopMatrix();

	// ===== Y Axis (Green) =====
	glColor3f(0.0f, 1.0f, 0.0f);  // Green
	glPushMatrix();
	glRotatef(-90, 1, 0, 0); // Point along +Y
	gluCylinder(quad, axisRadius, axisRadius, axisLength, 12, 1);
	glTranslatef(0, 0, axisLength);
	gluCylinder(quad, coneRadius, 0.0, coneHeight, 12, 1); // Cone tip
	glPopMatrix();

	// ===== Z Axis (Blue) =====
	glColor3f(0.0f, 0.0f, 1.0f);  // Blue
	glPushMatrix();
	// Already aligned with +Z
	gluCylinder(quad, axisRadius, axisRadius, axisLength, 12, 1);
	glTranslatef(0, 0, axisLength);
	gluCylinder(quad, coneRadius, 0.0, coneHeight, 12, 1); // Cone tip
	glPopMatrix();

	gluDeleteQuadric(quad);

	glPopMatrix();
	glPopAttrib();
}


void ModelViewerWidget::drawTrihedronOverlay() {
	int size = 100; // Size of mini viewport
	int margin = 10;

	glPushAttrib(GL_VIEWPORT_BIT | GL_ENABLE_BIT | GL_CURRENT_BIT | GL_DEPTH_BUFFER_BIT);
	glViewport(margin, margin, size, size);

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	gluPerspective(30.0, 1.0, 0.1, 10.0); // Narrow FOV

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	gluLookAt(0.0, 0.0, 5.0,  // Eye position
		0.0, 0.0, 0.0,  // Look at origin
		0.0, 1.0, 0.0); // Up vector

	// Extract the camera's rotation matrix
	QMatrix4x4 view = m_camera->getViewMatrix();

	// Remove translation component
	view.setColumn(3, QVector4D(0, 0, 0, 1));

	// Apply only rotation part of the main camera
	glMultMatrixf(view.constData());

	drawTrihedron();

	// Restore OpenGL state
	glPopMatrix(); // ModelView
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopAttrib();
}



void ModelViewerWidget::loadModel(const QString& filePath) {

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


void ModelViewerWidget::highlightNode(aiNode* node) {
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

		if (node == m_highlightedNode && !texId) {
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
	m_totalMouseDelta = QPoint(0, 0); // Reset drag distance
	if (m_inertiaTimer->isActive())
		m_inertiaTimer->stop();

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

	if (event->button() == Qt::LeftButton && m_mode == InteractionMode::Select && m_scene) {
		pickAtScreenPosition(event->pos());
	}
}


void ModelViewerWidget::mouseMoveEvent(QMouseEvent* event)
{
	QPoint delta = event->pos() - m_lastMousePos;
	m_totalMouseDelta += delta;
	QPoint downPoint = event->pos();

	if (m_mode == InteractionMode::Rotate) {

		QPoint rotate = m_lastMousePos - downPoint;

		m_camera->rotateX(rotate.y() / 2.0);
		m_camera->rotateY(rotate.x() / 2.0);		

		// Store rotation velocity
		m_rotationVelocity = QVector2D(delta.x(), delta.y()) / 2.0f;
		m_inertiaActive = false; // Stop inertia while dragging
	}
	else if (m_mode == InteractionMode::Pan) {

		// Pan speed scaled by distance
		float panSpeed = m_cameraDistance * 0.001f;

		// Apply panning to the view center
		QVector3D OP = get3dTranslationVectorFromMousePoints(downPoint, m_lastMousePos);
		m_camera->move(OP.x(), OP.y(), OP.z());

		m_panVelocity = OP;
		m_inertiaActive = false;
				
	}
	else if (m_mode == InteractionMode::Zoom) {
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
		m_panVelocity = OP;
		m_inertiaActive = false;

		resizeGL(width(), height());
	}

	m_lastMousePos = downPoint;

	update();
}

void ModelViewerWidget::mouseReleaseEvent(QMouseEvent* event)
{
	m_isDragging = false;
	
	int movementThreshold = 1; // Keep low for now
	
	if (m_totalMouseDelta.manhattanLength() > movementThreshold) 
	{			
		if (!m_inertiaTimer->isActive())
		{
			m_inertiaTimer->start(16); // 60 fps
			m_inertiaActive = true;
		}
	}
	else {
		// No real movement — don't start inertia		
		m_panVelocity = QVector3D(0, 0, 0);
		m_zoomVelocity = 0.0f;
		m_rotationVelocity = QVector2D(0, 0);
		m_inertiaTimer->stop();
	}

	Q_UNUSED(event);
	if (!(event->modifiers() & Qt::ControlModifier))
		m_mode = InteractionMode::Select;
}

void ModelViewerWidget::wheelEvent(QWheelEvent* event)
{
	QPoint numDegrees = event->angleDelta() / 8;
	if (!numDegrees.isNull()) {
		
		if (!m_inertiaTimer->isActive())
			m_inertiaTimer->start(16);

		QPoint numSteps = numDegrees / 15;
		float zoomStep = numSteps.y();
		float zoomFactor = abs(zoomStep) + 0.05;

		if (zoomStep < 0)
			m_viewRadius *= zoomFactor;
		else
			m_viewRadius /= zoomFactor;

		// Translate to focus on mouse center
		QPoint cen = QRect(0, 0, width(), height()).center();
		float sign = (event->position().x() > cen.x() || event->position().y() < cen.y() ||
			(event->position().x() < cen.x() && event->position().y() > cen.y())) && (zoomStep > 0) ? 1.0f : -1.0f;
		QVector3D OP = get3dTranslationVectorFromMousePoints(cen, event->position().toPoint());
		OP *= sign * 0.05f;
		m_camera->move(OP.x(), OP.y(), OP.z());

		// Add to velocities instead of overriding
		m_zoomVelocity += sign * 1.0f; // Tune factor
		m_panVelocity += OP * sign * 0.05f;
		
	}
	resizeGL(width(), height());
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
	
	m_viewMatrix = m_camera->getViewMatrix();
	m_projectionMatrix = m_camera->getProjectionMatrix();
	update();
}


void ModelViewerWidget::setViewTop() {
	m_camera->setView(GLCamera::ViewProjection::TOP_VIEW);
	update();
}

void ModelViewerWidget::setViewFront() {
	m_camera->setView(GLCamera::ViewProjection::FRONT_VIEW);
	update();
}

void ModelViewerWidget::setViewLeft() {
	m_camera->setView(GLCamera::ViewProjection::LEFT_VIEW);
	update();
}

void ModelViewerWidget::setViewAxonometric() {
	m_camera->setView(GLCamera::ViewProjection::SE_ISOMETRIC_VIEW);
	update();
}

void ModelViewerWidget::fitToView() {
	// Optional: adjust _cameraDistance or zoom to fit model bounds
	updateCamera();
	update();
}

void ModelViewerWidget::onInertiaTimeout() 
{
	if(m_mode == InteractionMode::Select)
		return; // Don't apply inertia while selecting
	if (!m_inertiaActive)
		return;

	bool stillActive = false;

	// Apply rotation inertia
	if (!m_rotationVelocity.isNull()) {
		m_camera->rotateX(-m_rotationVelocity.y());
		m_camera->rotateY(-m_rotationVelocity.x());
		m_rotationVelocity *= 0.90f;
		if (m_rotationVelocity.length() > 0.01f)
			stillActive = true;
		else
			m_rotationVelocity = QVector2D();
	}

	// Apply pan inertia
	if (!m_panVelocity.isNull()) {
		m_camera->move(m_panVelocity.x(), m_panVelocity.y(), m_panVelocity.z());
		m_panVelocity *= 0.90f;
		if (m_panVelocity.length() > 0.0001f)
			stillActive = true;
		else
			m_panVelocity = QVector3D();
	}

	// Apply zoom inertia
	if (std::abs(m_zoomVelocity) > 0.001f) {
		float zoomFactor = 1.005f;

		if (m_zoomVelocity > 0)
			m_viewRadius /= zoomFactor;
		else
			m_viewRadius *= zoomFactor;

		// Zoom-centric pan
		QPoint cen = rect().center();
		QVector3D OP = get3dTranslationVectorFromMousePoints(cen, cen);
		OP *= -m_zoomVelocity * 0.05f;
		m_camera->move(OP.x(), OP.y(), OP.z());

		m_zoomVelocity *= 0.75f;
		if (std::abs(m_zoomVelocity) > 0.001f)
			stillActive = true;
		else
			m_zoomVelocity = 0.0f;

		resizeGL(width(), height());
	}

	if (!stillActive) {
		m_inertiaActive = false;
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


QVector3D ModelViewerWidget::get3dTranslationVectorFromMousePoints(const QPoint& start, const QPoint& end)
{		
	QVector3D Z(0, 0, 0); // instead of 0 for x and y we need worldPosition.x() and worldPosition.y() ....
	Z = Z.project(m_viewMatrix * m_modelMatrix, m_projectionMatrix, QRect(0, 0, width(), height()));
	QVector3D p1(start.x(), height() - start.y(), Z.z());
	QVector3D O = p1.unproject(m_viewMatrix * m_modelMatrix, m_projectionMatrix, QRect(0, 0, width(), height()));
	QVector3D p2(end.x(), height() - end.y(), Z.z());
	QVector3D P = p2.unproject(m_viewMatrix * m_modelMatrix, m_projectionMatrix, QRect(0, 0, width(), height()));
	QVector3D OP = P - O;
	return OP;
}