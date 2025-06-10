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
//#include <GL/gl.h>
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
	m_shader.load(":/shaders/shaders/basic.vert",
		":/shaders/shaders/basic.frag");

	m_backgroundShader.load(":/shaders/shaders/gradientbg.vert",
		":/shaders/shaders/gradientbg.frag");

	m_trihedronShader.load(":/shaders/shaders/trihedron.vert",
		":/shaders/shaders/trihedron.frag");

	generateCylinderGeometry();
	generateConeGeometry();
	generateSphereGeometry();
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
		m_shader.setUniform("lightColor", lightColor);
		m_shader.setUniform("lightPos", lightPos);
		m_shader.setUniform("ambientColor", ambient);
				
		m_shader.setUniform("shininess", shininess);

		m_shader.setUniform("mvp", m_projectionMatrix * m_viewMatrix);
		m_shader.setUniform("view", m_viewMatrix);

		for (const auto& mesh : m_glMeshes) {
			QVector4D diffuse(0.8f, 0.8f, 0.8f, 1.0f);
			m_shader.setUniform("color", diffuse);
			m_shader.setUniform("specularColor", mesh->m_material.specular);			
			m_shader.setUniform("model", mesh->modelMatrix());
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

void ModelViewerWidget::generateCylinderGeometry() {
	std::vector<float> vertexData;
	const int segments = 12;
	const float radius = 0.05f;
	const float height = 1.0f;

	for (int i = 0; i <= segments; ++i) {
		float angle = 2.0f * M_PI * i / segments;
		float x = radius * cos(angle);
		float y = radius * sin(angle);

		// Normal for side surface (same for top and bottom at given x,y)
		float nx = cos(angle);
		float ny = sin(angle);
		float nz = 0.0f;

		// Bottom vertex
		vertexData.push_back(x);      // position
		vertexData.push_back(y);
		vertexData.push_back(0.0f);
		vertexData.push_back(nx);     // normal
		vertexData.push_back(ny);
		vertexData.push_back(nz);

		// Top vertex
		vertexData.push_back(x);
		vertexData.push_back(y);
		vertexData.push_back(height);
		vertexData.push_back(nx);
		vertexData.push_back(ny);
		vertexData.push_back(nz);
	}

	// ADD: Base cap vertices with blended normals
	float blendFactor = 0.6f; // Adjust this (0.0 = pure axial, 1.0 = pure radial)

	// Bottom cap center
	vertexData.push_back(0.0f);
	vertexData.push_back(0.0f);
	vertexData.push_back(0.0f);
	vertexData.push_back(0.0f);     // blended normal
	vertexData.push_back(0.0f);
	vertexData.push_back(-1.0f + blendFactor * 0.5f); // slightly less downward

	// Bottom cap rim vertices
	for (int i = 0; i <= segments; ++i) {
		float angle = 2.0f * M_PI * i / segments;
		float x = radius * cos(angle);
		float y = radius * sin(angle);

		// Blended normal: mix radial and axial components
		float nx = blendFactor * cos(angle);
		float ny = blendFactor * sin(angle);
		float nz = -(1.0f - blendFactor); // negative for bottom face

		// Normalize the blended normal
		float length = sqrt(nx * nx + ny * ny + nz * nz);
		nx /= length;
		ny /= length;
		nz /= length;

		vertexData.push_back(x);
		vertexData.push_back(y);
		vertexData.push_back(0.0f);
		vertexData.push_back(nx);
		vertexData.push_back(ny);
		vertexData.push_back(nz);
	}

	// Top cap center
	vertexData.push_back(0.0f);
	vertexData.push_back(0.0f);
	vertexData.push_back(height);
	vertexData.push_back(0.0f);
	vertexData.push_back(0.0f);
	vertexData.push_back(1.0f - blendFactor * 0.5f); // slightly less upward

	// Top cap rim vertices
	for (int i = 0; i <= segments; ++i) {
		float angle = 2.0f * M_PI * i / segments;
		float x = radius * cos(angle);
		float y = radius * sin(angle);

		// Blended normal: mix radial and axial components
		float nx = blendFactor * cos(angle);
		float ny = blendFactor * sin(angle);
		float nz = (1.0f - blendFactor); // positive for top face

		// Normalize the blended normal
		float length = sqrt(nx * nx + ny * ny + nz * nz);
		nx /= length;
		ny /= length;
		nz /= length;

		vertexData.push_back(x);
		vertexData.push_back(y);
		vertexData.push_back(height);
		vertexData.push_back(nx);
		vertexData.push_back(ny);
		vertexData.push_back(nz);
	}

	m_cylinderVertexCount = (int)vertexData.size() / 6; // 3 pos + 3 normal

	// Generate VAO/VBO
	GLuint VBO;
	glGenVertexArrays(1, &m_cylinderVAO);
	glGenBuffers(1, &VBO);

	glBindVertexArray(m_cylinderVAO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, vertexData.size() * sizeof(float), vertexData.data(), GL_STATIC_DRAW);

	// Position attribute
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);

	// Normal attribute
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);

	glBindVertexArray(0);
}

void ModelViewerWidget::generateConeGeometry() {
	std::vector<float> vertexData;
	const int segments = 12;
	const float radius = 0.1f;
	const float height = 0.2f;

	// ===== Base Circle (flat face) =====
	for (int i = 0; i <= segments; ++i) {
		float angle = 2.0f * M_PI * i / segments;
		float x = radius * cos(angle);
		float y = radius * sin(angle);

		// Center of base (normal pointing -Z)
		vertexData.push_back(0.0f);
		vertexData.push_back(0.0f);
		vertexData.push_back(0.0f);
		vertexData.push_back(0.0f);
		vertexData.push_back(0.0f);
		vertexData.push_back(-1.0f);

		// Perimeter point of base
		vertexData.push_back(x);
		vertexData.push_back(y);
		vertexData.push_back(0.0f);
		vertexData.push_back(0.0f);
		vertexData.push_back(0.0f);
		vertexData.push_back(-1.0f);
	}

	// ===== Side Surface =====
	for (int i = 0; i <= segments; ++i) {
		float angle = 2.0f * M_PI * i / segments;
		float x = radius * cos(angle);
		float y = radius * sin(angle);

		// Vector from cone tip to perimeter point
		float len = std::sqrt(x * x + y * y + height * height);
		float nx = x / len;
		float ny = y / len;
		float nz = radius / len; // From side normal of cone

		// Tip of the cone
		vertexData.push_back(0.0f);
		vertexData.push_back(0.0f);
		vertexData.push_back(height);
		vertexData.push_back(nx);
		vertexData.push_back(ny);
		vertexData.push_back(nz);

		// Perimeter point of base
		vertexData.push_back(x);
		vertexData.push_back(y);
		vertexData.push_back(0.0f);
		vertexData.push_back(nx);
		vertexData.push_back(ny);
		vertexData.push_back(nz);
	}

	m_coneVertexCount = static_cast<int>(vertexData.size()) / 6; // 3 position + 3 normal

	// Generate VAO/VBO
	GLuint VBO;
	glGenVertexArrays(1, &m_coneVAO);
	glGenBuffers(1, &VBO);

	glBindVertexArray(m_coneVAO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, vertexData.size() * sizeof(float), vertexData.data(), GL_STATIC_DRAW);

	// Position attribute
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);

	// Normal attribute
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);

	glBindVertexArray(0);
}

void ModelViewerWidget::generateSphereGeometry() {
	std::vector<float> vertexData;
	std::vector<unsigned int> indices;
	float radius = 0.1f;
	int latitudeSegments = 12;
	int longitudeSegments = 12;

	vertexData.clear();
	indices.clear();

	// Generate vertices
	for (int lat = 0; lat <= latitudeSegments; ++lat) {
		float theta = M_PI * lat / latitudeSegments; // 0 to PI (top to bottom)
		float sinTheta = sin(theta);
		float cosTheta = cos(theta);

		for (int lon = 0; lon <= longitudeSegments; ++lon) {
			float phi = 2.0f * M_PI * lon / longitudeSegments; // 0 to 2PI (around)
			float sinPhi = sin(phi);
			float cosPhi = cos(phi);

			// Calculate position
			float x = radius * sinTheta * cosPhi;
			float y = radius * cosTheta;
			float z = radius * sinTheta * sinPhi;

			// Normal is the same as normalized position for a sphere centered at origin
			float nx = sinTheta * cosPhi;
			float ny = cosTheta;
			float nz = sinTheta * sinPhi;

			// Add vertex data (position + normal)
			vertexData.push_back(x);   // position
			vertexData.push_back(y);
			vertexData.push_back(z);
			vertexData.push_back(nx);  // normal
			vertexData.push_back(ny);
			vertexData.push_back(nz);
		}
	}

	// Generate indices for triangles
	for (int lat = 0; lat < latitudeSegments; ++lat) {
		for (int lon = 0; lon < longitudeSegments; ++lon) {
			int current = lat * (longitudeSegments + 1) + lon;
			int next = current + longitudeSegments + 1;

			// First triangle
			indices.push_back(current);
			indices.push_back(next);
			indices.push_back(current + 1);

			// Second triangle
			indices.push_back(current + 1);
			indices.push_back(next);
			indices.push_back(next + 1);
		}
	}

	m_sphereIndexCount = indices.size();

	// Create and bind VAO
	glGenVertexArrays(1, &m_sphereVAO);
	glBindVertexArray(m_sphereVAO);

	// Create and bind VBO
	glGenBuffers(1, &m_sphereVBO);
	glBindBuffer(GL_ARRAY_BUFFER, m_sphereVBO);
	glBufferData(GL_ARRAY_BUFFER, vertexData.size() * sizeof(float),
		vertexData.data(), GL_STATIC_DRAW);

	// Create and bind EBO
	glGenBuffers(1, &m_sphereEBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_sphereEBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, vertexData.size() * sizeof(unsigned int),
		indices.data(), GL_STATIC_DRAW);

	// Set up vertex attributes (assuming same layout as cylinder)
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);

	glBindVertexArray(0);
}


void ModelViewerWidget::drawCylinder(const QMatrix4x4& model) {
	
	m_trihedronShader.setUniform("uModel", model);

	glBindVertexArray(m_cylinderVAO);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, m_cylinderVertexCount);
	glBindVertexArray(0);	
}

void ModelViewerWidget::drawCone(const QMatrix4x4& model) {
	
	m_trihedronShader.setUniform("uModel", model);

	glBindVertexArray(m_coneVAO);
	glDrawArrays(GL_TRIANGLE_FAN, 0, m_coneVertexCount);
	glBindVertexArray(0);
}

void ModelViewerWidget::drawSphere(const QMatrix4x4& model) {
	m_trihedronShader.setUniform("uModel", model);
	glBindVertexArray(m_sphereVAO);
	glDrawElements(GL_TRIANGLES, m_sphereIndexCount, GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);
}

void ModelViewerWidget::drawTrihedron(const QMatrix4x4& view, const QMatrix4x4& projection) {
	m_trihedronShader.use();

	// Set view and projection matrices
	m_trihedronShader.setUniform("uView", view);
	m_trihedronShader.setUniform("uProjection", projection);
	m_trihedronShader.setUniform("viewPos", m_camera->getPosition());
	QMatrix4x4 model;

	// Draw center sphere
	model.setToIdentity();
	m_trihedronShader.setUniform("uColor", QVector3D(1.0f, 1.0f, 1.0f)); // White
	drawSphere(model);

	// Draw X-axis (Red)
	model.setToIdentity();
	model.rotate(90, 0, 1, 0); // Rotate to align with X-axis
	m_trihedronShader.setUniform("uColor", QVector3D(1.0f, 0.0f, 0.0f)); // Red
	drawCylinder(model);

	model.setToIdentity(); // Reset model matrix
	model.translate(1.0f, 0.0f, 0.0f); // Move to cylinder tip
	model.rotate(90, 0, 1, 0); // Align cone along +X-axis
	drawCone(model);

	// Draw Y-axis (Green)
	model.setToIdentity();
	model.rotate(-90, 1, 0, 0); // Rotate to align with Y-axis
	m_trihedronShader.setUniform("uColor", QVector3D(0.0f, 0.75f, 0.0f)); // Green
	drawCylinder(model);

	model.setToIdentity(); // Reset model matrix
	model.translate(0.0f, 1.0f, 0.0f); // Move to cylinder tip
	model.rotate(-90, 1, 0, 0); // Rotate cylinder along +Y-axis
	drawCone(model);

	// Draw Z-axis (Blue)
	model.setToIdentity();
	m_trihedronShader.setUniform("uColor", QVector3D(0.0f, 0.0f, 1.0f)); // Blue
	drawCylinder(model);

	model.translate(0.0f, 0.0f, 1.0f); // Move to cylinder tip
	drawCone(model);

	m_trihedronShader.release();
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
		float orthoSize = 1.5f;
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

	// Draw the trihedron (axes)
	drawTrihedron(view, projection);

	// Restore the viewport to the main scene
	glViewport(0, 0, width(), height());
}


void ModelViewerWidget::loadModel(const QString& filePath) {
	
	makeCurrent(); // Ensure OpenGL context is current

	if(m_scene) {
		m_importer.FreeScene(); // Free previous scene if exists
		m_scene = nullptr;
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

	m_glMeshes.clear();
	for (unsigned int i = 0; i < m_scene->mNumMeshes; ++i) {
		aiMesh* mesh = m_scene->mMeshes[i];
		GLMesh* glMesh = new GLMesh(mesh, m_shader.program());

		// Set up material
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

		glMesh->setMaterial(mat); // Store material in mesh
		glMesh->setupMesh(); // Initialize OpenGL buffers

		m_glMeshes.emplace_back(glMesh);
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