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
	fmt.setProfile(QSurfaceFormat::CoreProfile);
	fmt.setOption(QSurfaceFormat::DebugContext);
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
	layout->addWidget(createViewButton(":/icons/res/bottom.png", "Bottom View", [this]() { setViewBottom(); }, _viewToolbar));
	layout->addWidget(createViewButton(":/icons/res/back.png", "Rear View", [this]() { setViewRear(); }, _viewToolbar));
	layout->addWidget(createViewButton(":/icons/res/right.png", "Right View", [this]() { setViewRight(); }, _viewToolbar));
	layout->addWidget(createViewButton(":/icons/res/isometric.png", "Isometric View", [this]() { setViewAxonometric(); }, _viewToolbar));
	layout->addWidget(createViewButton(":/icons/res/fit-all.png", "Fit All", [this]() { fitToView(); }, _viewToolbar));

	QToolButton* projToggleButton = new QToolButton(_viewToolbar);
	projToggleButton->setCheckable(true);
	projToggleButton->setIcon(QIcon(":/icons/res/Perspective.png"));  // default icon
	projToggleButton->setIconSize(QSize(64, 64));
	projToggleButton->setToolTip("Toggle Projection");

	connect(projToggleButton, &QToolButton::toggled, this, [this, projToggleButton](bool checked) {
		if (checked) {
			m_camera->setProjectionType(GLCamera::ProjectionType::ORTHOGRAPHIC);
			projToggleButton->setIcon(QIcon(":/icons/res/Ortho.png"));
			projToggleButton->setIconSize(QSize(64, 64));
			projToggleButton->setToolTip("Switch to Perspective");
		}
		else {
			m_camera->setProjectionType(GLCamera::ProjectionType::PERSPECTIVE);
			projToggleButton->setIcon(QIcon(":/icons/res/Perspective.png"));
			projToggleButton->setIconSize(QSize(64, 64));
			projToggleButton->setToolTip("Switch to Orthographic");
		}
		update();
		});

	layout->addWidget(projToggleButton);

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

	m_camera = new GLCamera(height(), width(), m_viewRadius, 45);
	m_camera->setProjectionType(GLCamera::ProjectionType::PERSPECTIVE);
	m_camera->setView(GLCamera::ViewProjection::SE_ISOMETRIC_VIEW);

	if (!m_scene) {
		// Set up a default camera view for the test triangle
		m_camera->setPosition(QVector3D(0, 0, 2));
		m_camera->setPosition(QVector3D(0, 0, 0));
		//m_camera->setUpVector(QVector3D(0, 1, 0));
		m_camera->setViewRange(2.0f);
		m_camera->setProjectionType(GLCamera::ProjectionType::PERSPECTIVE);
	}


	// Load shader sources (for demo, you can hardcode or load from file)
	m_shader.load(":/shaders/shaders/mesh.vert",
		":/shaders/shaders/mesh.frag");

	m_backgroundShader.load(":/shaders/shaders/gradientbg.vert",
		":/shaders/shaders/gradientbg.frag");

	m_trihedronShader.load(":/shaders/shaders/trihedron.vert",
		":/shaders/shaders/trihedron.frag");

	m_trihedron = std::make_unique<Trihedron>(&m_trihedronShader);
}

void ModelViewerWidget::resizeGL(int w, int h) {
	glViewport(0, 0, w, h);

	if (h == 0) h = 1; // Prevent division by zero

	if (m_camera) {
		m_camera->setScreenSize(w, h);
		m_camera->setViewRange(m_viewRadius * 2.1f);
		m_viewMatrix = m_camera->getViewMatrix();
		m_projectionMatrix = m_camera->getProjectionMatrix();
	}
	else {
		// Fallback for when no model/camera is set up
		m_viewMatrix.setToIdentity();
		m_viewMatrix.lookAt(QVector3D(0, 0, 2), QVector3D(0, 0, 0), QVector3D(0, 1, 0));
		m_projectionMatrix.setToIdentity();
		m_projectionMatrix.perspective(45.0f, float(w) / float(h), 0.1f, 10.0f);
	}

}

void ModelViewerWidget::paintGL() {

	glViewport(0, 0, width(), height());
	glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	drawGradientBackground();

	glEnable(GL_DEPTH_TEST);
	m_viewMatrix = m_camera->getViewMatrix();
	m_projectionMatrix = m_camera->getProjectionMatrix();


	if (!m_glMeshes.empty()) {
		m_shader.use();

		QVector3D lightColor(1.0f, 1.0f, 1.0f);
		QVector3D lightPos(0.0f, 0.0f, 1.0f);
		QVector3D ambient(0.3f, 0.3f, 0.3f);		
		float shininess = 100.0f;

		QVector3D lightDirWorld(0.577f, 0.577f, 0.577f); // Normalized (1,1,1)		
		QVector3D cameraTarget = m_viewCenter + lightDirWorld * m_viewRadius; // Target point in world space
		QVector3D cameraPos = m_camera->getPosition(); // or eye position in world space

		QVector3D viewDir = cameraTarget - cameraPos;
		viewDir.normalize();
		m_shader.setUniform("lightDir", lightDirWorld);
		m_shader.setUniform("viewPos", cameraPos);		
		m_shader.setUniform("lightPos", lightPos);
		m_shader.setUniform("ambientColor", ambient);				
		m_shader.setUniform("shininess", shininess);

		m_shader.setUniform("mvp", m_projectionMatrix * m_viewMatrix);
		m_shader.setUniform("view", m_viewMatrix);

		for (const auto& meshptr : m_glMeshes) {
			GLMesh* mesh = meshptr.get(); // Use smart pointer to access raw pointer
			bool shouldHighlight = false;
			m_shader.setUniform("specularColor", mesh->material().specular);
			m_shader.setUniform("model", mesh->modelMatrix());
			
			// Check if this mesh should be highlighted
			if (m_highlightedMeshIndex >= 0) {
				// Direct lookup
				auto it = m_meshIndexToGLMesh.find(m_highlightedMeshIndex);
				shouldHighlight = (it != m_meshIndexToGLMesh.end() && it->second == mesh);
			}
			else if (m_highlightedNode) {
				// Highlighting node - check if this mesh belongs to the highlighted node
				auto it = m_nodeMeshes.find(m_highlightedNode);
				if (it != m_nodeMeshes.end()) {
					const auto& nodeMeshes = it->second;
					shouldHighlight = std::find(nodeMeshes.begin(), nodeMeshes.end(), mesh) != nodeMeshes.end();
				}
			}

			// Apply highlighting
			if (shouldHighlight) {
				glEnable(GL_BLEND);
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				m_shader.setUniform("isSelected", true);
			}
			else {
				glDisable(GL_BLEND);
				m_shader.setUniform("isSelected", false);
			}
			mesh->draw();
		}
	}

	drawTrihedronOverlay(); // Draw trihedron in mini viewport

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

void ModelViewerWidget::computeBoundingBox(const aiScene* scene, const aiNode* node,
	aiVector3D& minimum, aiVector3D& maximum, const aiMatrix4x4& transform)
{
	aiMatrix4x4 currentTransform = transform * node->mTransformation;

	for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
		const aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];

		computeBounds(mesh, currentTransform, minimum, maximum);
	}

	for (unsigned int i = 0; i < node->mNumChildren; ++i) {
		computeBoundingBox(scene, node->mChildren[i], minimum, maximum, currentTransform);
	}
}

void ModelViewerWidget::computeBounds(const aiMesh* mesh, const aiMatrix4x4& currentTransform, aiVector3D& minimum, aiVector3D& maximum)
{
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

void ModelViewerWidget::computeBoundingSphere(const aiMesh* iMesh, const aiMatrix4x4& transform, aiVector3D& oCenter, float& oRadius)
{
	aiVector3D minimum(FLT_MAX, FLT_MAX, FLT_MAX);
	aiVector3D maximum(-FLT_MAX, -FLT_MAX, -FLT_MAX);
	computeBounds(iMesh, transform, minimum, maximum);
	oCenter = (aiVector3D(maximum.x, maximum.y, maximum.z) + aiVector3D(minimum.x, minimum.y, minimum.z)) * 0.5f;
	float maxExtent = std::max({ maximum.x - minimum.x, maximum.y - minimum.y, maximum.z - minimum.z });
	oRadius = maxExtent * 0.5f;
}

void ModelViewerWidget::drawGradientBackground() {

	glViewport(0, 0, width(), height());

	glDisable(GL_DEPTH_TEST);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	// Vertex data: positions and colors
	float vertices[] = {
		// Position      // Color
		-1.0f,  1.0f,   0.3f, 0.3f, 0.3f,  // Top left
		 1.0f,  1.0f,   0.55f, 0.55f, 0.55f, // Top right
		 1.0f, -1.0f,   0.95f, 0.95f, 0.95f, // Bottom right
		-1.0f, -1.0f,   0.7f, 0.7f, 0.7f    // Bottom left
	};

	unsigned int indices[] = {
		0, 1, 2,  // First triangle
		2, 3, 0   // Second triangle
	};

	// Generate and bind a VAO and VBO for the vertices
	unsigned int VAO, VBO, EBO;
	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
	glGenBuffers(1, &EBO);

	glBindVertexArray(VAO);

	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

	// Position attribute
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);

	// Color attribute
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(2 * sizeof(float)));
	glEnableVertexAttribArray(1);

	// Use your custom shader program
	m_backgroundShader.use(); // Assumes you have a Shader class that handles compiling/linking shaders
	glBindVertexArray(VAO);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

	// Cleanup
	glBindVertexArray(0);
	glDeleteBuffers(1, &VBO);
	glDeleteBuffers(1, &EBO);
	glDeleteVertexArrays(1, &VAO);
}

void ModelViewerWidget::drawTrihedronOverlay() {
	const int overlaySize = 110; // Size of mini viewport
	const int margin = 10;      // Margin from the bottom-left corner

	// Set up the mini viewport
	glViewport(margin, margin, overlaySize, overlaySize);
	glClear(GL_DEPTH_BUFFER_BIT); // Clear depth buffer for the overlay
	glEnable(GL_DEPTH_TEST); // Enable depth testing for the overlay

	// Create projection matrix
	QMatrix4x4 projection;
	if (m_camera->getProjectionType() == GLCamera::ProjectionType::PERSPECTIVE) {
		// Narrow field of view for better appearance in small viewport
		projection.perspective(30.0, 1.0, 0.1, 10.0);
	}
	else {
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
	if (m_trihedron) {
		m_trihedron->draw(view, projection);
	}

	// Restore the viewport to the main scene
	glViewport(0, 0, width(), height());
}


void ModelViewerWidget::loadModel(const QString& filePath) {
	
	makeCurrent(); // Ensure OpenGL context is current

	if(m_scene) {
		m_importer.FreeScene(); // Free previous scene if exists
		m_scene = nullptr;
		m_glMeshes.clear();
	}

	m_scene = m_importer.ReadFile(filePath.toStdString(),
		aiProcess_Triangulate | aiProcess_ValidateDataStructure |
		aiProcess_CalcTangentSpace | aiProcess_GenSmoothNormals |
		aiProcess_FixInfacingNormals | aiProcess_JoinIdenticalVertices |
		aiProcess_OptimizeMeshes | aiProcess_GenUVCoords | aiProcess_SortByPType);

	if (!m_scene || m_scene->mFlags == AI_SCENE_FLAGS_INCOMPLETE || !m_scene->mRootNode) {
		qDebug() << "ERROR::ASSIMP:: " << m_importer.GetErrorString();
		m_scene = nullptr;
		return;
	}

	if (m_scene->mRootNode) {
		loadNodeMeshes(m_scene->mRootNode);
	}
		
			
	updateCamera();
	update();
}

void ModelViewerWidget::loadNodeMeshes(aiNode* node) {

	// Load meshes for this node
	for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
		unsigned int meshIndex = node->mMeshes[i];
		aiMesh* mesh = m_scene->mMeshes[meshIndex];

		// compute bounding sphere for the mesh
		aiVector3D oCenter; float oRadius;
		computeBoundingSphere(mesh, node->mTransformation, oCenter, oRadius);

		//GLMesh* glMesh = new GLMesh(mesh, m_shader.program());
		auto glMesh = std::make_unique<GLMesh>(mesh, m_shader.program());

		glMesh->setBoundingSphere(QVector3D(oCenter.x, oCenter.y, oCenter.z), oRadius);

		// Set up material (your existing code)
		Material mat;
		if (mesh->mMaterialIndex >= 0) {
			aiMaterial* material = m_scene->mMaterials[mesh->mMaterialIndex];
			aiColor4D c;
			if (AI_SUCCESS == material->Get(AI_MATKEY_COLOR_AMBIENT, c))
				mat.ambient = QVector4D(c.r, c.g, c.b, c.a);
			if (AI_SUCCESS == material->Get(AI_MATKEY_COLOR_DIFFUSE, c))
				mat.diffuse = QVector4D(c.r, c.g, c.b, c.a);
			if (AI_SUCCESS == material->Get(AI_MATKEY_COLOR_SPECULAR, c))
				mat.specular = QVector4D(c.r, c.g, c.b, c.a);
			float shininess = 0.0f;
			if (AI_SUCCESS == material->Get(AI_MATKEY_SHININESS, shininess))
				mat.shininess = shininess;
		}

		glMesh->setMaterial(mat);
		glMesh->setupMesh();

		// Store associations - use raw pointer
		GLMesh* rawPtr = glMesh.get();
		m_nodeMeshes[node].push_back(rawPtr);
		m_meshIndexToGLMesh[meshIndex] = rawPtr; // Store raw pointer

		m_glMeshes.emplace_back(std::move(glMesh));
	}

	// Recursively process child nodes
	for (unsigned int i = 0; i < node->mNumChildren; ++i) {
		loadNodeMeshes(node->mChildren[i]);
	}
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
	m_highlightedMeshIndex = -1; // Clear mesh selection
	update();
}

void ModelViewerWidget::highlightMesh(int meshIndex) {
	m_highlightedMeshIndex = meshIndex;
	m_highlightedNode = nullptr; // Clear node selection
	update();
}

void ModelViewerWidget::clearHighlight() {
	m_highlightedNode = nullptr;
	m_highlightedMeshIndex = -1;
	update();
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

		setCursor(QCursor(QPixmap(":/icons/res/rotatecursor.png")));

		QPoint rotate = m_lastMousePos - downPoint;

		m_camera->rotateX(rotate.y() / 2.0);
		m_camera->rotateY(rotate.x() / 2.0);

		// Store rotation velocity
		m_rotationVelocity = QVector2D(delta.x(), delta.y()) / 2.0f;
		m_inertiaActive = false; // Stop inertia while dragging
	}
	else if (m_mode == InteractionMode::Pan) {

		setCursor(QCursor(QPixmap(":/icons/res/pancursor.png")));

		// Pan speed scaled by distance
		float panSpeed = m_cameraDistance * 0.001f;

		// Apply panning to the view center
		QVector3D OP = get3dTranslationVectorFromMousePoints(downPoint, m_lastMousePos);
		m_camera->move(OP.x(), OP.y(), OP.z());

		m_panVelocity = OP;
		m_inertiaActive = false;

	}
	else if (m_mode == InteractionMode::Zoom) {

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

	m_lastMousePos = downPoint;

	update();
}

void ModelViewerWidget::mouseReleaseEvent(QMouseEvent* event)
{
	setCursor(QCursor(Qt::ArrowCursor));

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

void ModelViewerWidget::wheelEvent(QWheelEvent* event)
{
	QPoint numDegrees = event->angleDelta() / 8;
	if (!numDegrees.isNull()) {

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
			(event->position().x() < cen.x() && event->position().y() > cen.y())) && (zoomStep > 0) ? 1.0f : -1.0f;
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

void ModelViewerWidget::keyPressEvent(QKeyEvent* event) {
	if (event->modifiers() & Qt::ControlModifier) {
		if (event->key() == Qt::Key_T) {
			setViewProjection(ViewProjection::Top);
		}
		else if (event->key() == Qt::Key_F) {
			setViewProjection(ViewProjection::Front);
		}
		else if (event->key() == Qt::Key_L) {
			setViewProjection(ViewProjection::Left);
		}
		if (event->key() == Qt::Key_B) {
			setViewProjection(ViewProjection::Bottom);
		}
		else if (event->key() == Qt::Key_R) {
			setViewProjection(ViewProjection::Rear);
		}
		else if (event->key() == Qt::Key_J) {
			setViewProjection(ViewProjection::Right);
		}
		else if (event->key() == Qt::Key_A) {
			setViewProjection(ViewProjection::Axonometric);
		}
	}
	else {
		if (event->key() == Qt::Key_Home) {
			updateCamera();
			update();
		}
	}
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
	m_viewCenter = QVector3D(0, 0, 0);
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

void ModelViewerWidget::setViewBottom() {
	m_camera->setView(GLCamera::ViewProjection::BOTTOM_VIEW);
	update();
}

void ModelViewerWidget::setViewRear() {
	m_camera->setView(GLCamera::ViewProjection::REAR_VIEW);
	update();
}

void ModelViewerWidget::setViewRight() {
	m_camera->setView(GLCamera::ViewProjection::RIGHT_VIEW);
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
	if (m_mode == InteractionMode::Select)
		return; // Don't apply inertia while selecting
	if (!m_inertiaActive)
		return;

	bool stillActive = false;

	// Apply rotation inertia
	if (!m_rotationVelocity.isNull()) {
		m_camera->rotateX(-m_rotationVelocity.y());
		m_camera->rotateY(-m_rotationVelocity.x());
		m_rotationVelocity *= m_inertiaFactor;
		if (m_rotationVelocity.length() > 0.01f)
			stillActive = true;
		else
			m_rotationVelocity = QVector2D();
	}

	// Apply pan inertia
	if (!m_panVelocity.isNull()) {
		m_camera->move(m_panVelocity.x(), m_panVelocity.y(), m_panVelocity.z());
		m_panVelocity *= m_inertiaFactor;
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
		OP *= -m_zoomPanVelocity * 0.05f;
		m_camera->move(OP.x(), OP.y(), OP.z());

		m_zoomVelocity *= m_inertiaFactor * 0.75f;
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

	// Get the viewport dimensions
	QRect viewportRect(0, 0, width(), height());

	float x = pos.x();
	float y = viewportRect.height() - pos.y(); // Flip Y for OpenGL
	float normalizedX = (x / viewportRect.width()) * 2.0f - 1.0f; // Normalize to [-1, 1]
	float normalizedY = (y / viewportRect.height()) * 2.0f - 1.0f;

	// Create normalized device coordinates (NDC) for near and far points
	QVector4D nearPointNDC(normalizedX, normalizedY, -1.0f, 1.0f); // NDC z = -1 for near
	QVector4D farPointNDC(normalizedX, normalizedY, 1.0f, 1.0f);   // NDC z = 1 for far

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

			// Bounding sphere intersection check
			QVector3D cen; float radius;
			m_meshIndexToGLMesh[meshIndex]->getBoundingSphere(cen, radius);
			aiVector3D center(cen.x(), cen.y(), cen.z()); // Convert to aiVector3D
			center *= transform; // Apply transformation to center
			if (!rayIntersectsSphere(origin, dir, center, radius)) {
				continue; // Skip this mesh entirely
			}

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
		// Check if we're clicking the same mesh again
		if (m_lastPickedMeshIndex == hitMeshIndex) {
			emit meshPicked(-1); // Signal to deselect  
			m_lastPickedMeshIndex = -1;
			highlightMesh(-1); // Clear highlight
		}
		else {
			emit meshPicked(hitMeshIndex);
			m_lastPickedMeshIndex = hitMeshIndex;
			highlightMesh(hitMeshIndex); // Highlight specific mesh
		}
	}
	else {
		// No mesh hit, clear selection
		m_lastPickedMeshIndex = -1;
		highlightMesh(-1);
	}	

}

bool ModelViewerWidget::rayIntersectsSphere(const aiVector3D& origin, const aiVector3D& dir, const aiVector3D& center, float radius) {
	// Vector from the ray origin to the sphere center
	aiVector3D oc = origin - center;

	// Quadratic equation coefficients
	float a = dir.SquareLength();  // Since dir should be normalized, a = 1
	float b = 2.0f * oc * dir;      // Dot product in Assimp
	float c = oc.SquareLength() - radius * radius;

	// Discriminant of the quadratic equation
	float discriminant = b * b - 4 * a * c;

	// Ray intersects the sphere if the discriminant is non-negative
	return discriminant >= 0;
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
	// Determine viewport and camera
	QRect viewport(0,0,width(), height());
	GLCamera* camera = m_camera;

	// Get view and projection matrices
	QMatrix4x4 view = camera->getViewMatrix();
	QMatrix4x4 projection = camera->getProjectionMatrix();
	QMatrix4x4 inv = (projection * view).inverted();

	// Choose reference Z in world space
	float referenceWorldZ = 0.0f;
	if (camera->getProjectionType() == GLCamera::ProjectionType::ORTHOGRAPHIC) {
		referenceWorldZ = m_viewCenter.z();
	}
	else {
		QVector3D focusPoint = camera->getPosition() + camera->getViewDir();
		referenceWorldZ = focusPoint.z();
	}

	// Project reference world point to get NDC Z
	QVector4D refWorld(0, 0, referenceWorldZ, 1.0f);
	QVector4D refClip = projection * view * refWorld;
	float ndcZ = refClip.w() != 0.0f ? refClip.z() / refClip.w() : 0.0f;

	// Helper to convert screen point to NDC
	auto toNDC = [&](const QPoint& pt) {
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