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

GLuint ModelViewerWidget::compileShader(GLenum type, const char* src) {
	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &src, nullptr);
	glCompileShader(shader);
	GLint status;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	if (!status) {
		char log[512];
		glGetShaderInfoLog(shader, 512, nullptr, log);
		qDebug() << "Shader compile error:" << log;
		glDeleteShader(shader);
		return 0;
	}
	return shader;
}

GLuint ModelViewerWidget::createProgram(const char* vsrc, const char* fsrc) {
	GLuint vs = compileShader(GL_VERTEX_SHADER, vsrc);
	GLuint fs = compileShader(GL_FRAGMENT_SHADER, fsrc);
	GLuint prog = glCreateProgram();
	glAttachShader(prog, vs);
	glAttachShader(prog, fs);
	glLinkProgram(prog);
	glDeleteShader(vs);
	glDeleteShader(fs);
	GLint status;
	glGetProgramiv(prog, GL_LINK_STATUS, &status);
	if (!status) {
		char log[512];
		glGetProgramInfoLog(prog, 512, nullptr, log);
		qDebug() << "Program link error:" << log;
		glDeleteProgram(prog);
		return 0;
	}
	return prog;
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

	const char* phongVertexShaderSrc =
		"#version 330 core\n"
		"layout(location = 0) in vec3 position;\n"
		"layout(location = 1) in vec3 normal;\n"
		"layout(location = 2) in vec2 texcoord;\n"
		"uniform mat4 mvp;\n"
		"uniform mat4 model;\n"
		"uniform mat4 view;\n"
		"out vec3 fragNormal;\n"
		"out vec3 fragPos;\n"
		"void main() {\n"
		"    vec4 viewPos4 = view * model * vec4(position, 1.0);\n"
		"    fragPos = viewPos4.xyz;\n"
		"    mat3 normalMatrix = transpose(inverse(mat3(view * model)));\n"
		"    fragNormal = normalize(normalMatrix * normal);\n"
		"    gl_Position = mvp * vec4(position, 1.0);\n"
		"}\n";

	const char* phongFragmentShaderSrc =
		"#version 330 core\n"
		"in vec3 fragNormal;\n"
		"in vec3 fragPos;\n"
		"uniform vec4 color;\n"
		"uniform vec3 ambientColor;\n"
		"uniform vec3 specularColor;\n"
		"uniform float shininess;\n"
		"uniform vec3 lightDir;\n"
		"out vec4 fragColor;\n"
		"void main() {\n"
		"    vec3 norm = normalize(fragNormal);\n"
		"    vec3 L = normalize(lightDir);\n"
		"    vec3 V = normalize(-fragPos);\n"
		"    float ndotv = max(dot(norm, V), 0.0);\n"
		"    float diff = max(dot(norm, L), 0.0);\n"
		"    vec3 ambient = ambientColor * color.rgb;\n"
		"    vec3 diffuse = diff * color.rgb;\n"
		"    float spec = 0.0;\n"
		"    if (diff > 0.0 && ndotv > 0.1) {\n"
		"        vec3 H = normalize(L + V);\n"
		"        float nh = max(dot(norm, H), 0.0);\n"
		"        if (nh > 0.0)\n"
		"            spec = pow(nh, shininess);\n"
		"    }\n"
		"    vec3 specular = specularColor * spec;\n"
		"    vec3 result = ambient + diffuse + specular;\n"
		"    fragColor = vec4(clamp(result, 0.0, 1.0), color.a);\n"
		"}\n";

	m_meshShaderProgram = createProgram(phongVertexShaderSrc, phongFragmentShaderSrc);
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

	drawGradientBackground();	

	glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	
	m_viewMatrix = m_camera->getViewMatrix();
	m_projectionMatrix = m_camera->getProjectionMatrix();

	if (!m_modernMeshes.empty()) {

		glUseProgram(m_meshShaderProgram);

		// Set default lighting values
		float ambient[] = { 0.2f, 0.2f, 0.2f };
		float specular[] = { 0.7f, 0.7f, 0.7f };
		float shininess = 64.0f;		
		QVector3D cpos = m_camera->getPosition();
		float viewPos[3] = { 0.0f, 0.0f, 0.0f };

		QVector3D lightDirView = QVector3D(0.0f, 0.0f, m_cameraDistance); // +Z in eye space
		float lightDir[3] = { lightDirView.x(), lightDirView.y(), lightDirView.z() };
		glUniform3fv(glGetUniformLocation(m_meshShaderProgram, "lightDir"), 1, lightDir);

		GLint viewLoc = glGetUniformLocation(m_meshShaderProgram, "view");
		glUniformMatrix4fv(viewLoc, 1, GL_FALSE, m_viewMatrix.constData());
		
		glUniform3fv(glGetUniformLocation(m_meshShaderProgram, "ambientColor"), 1, ambient);
		glUniform3fv(glGetUniformLocation(m_meshShaderProgram, "specularColor"), 1, specular);
		glUniform1f(glGetUniformLocation(m_meshShaderProgram, "shininess"), shininess);
		
		glUniform3fv(glGetUniformLocation(m_meshShaderProgram, "viewPos"), 1, viewPos);

		GLint mvpLoc = glGetUniformLocation(m_meshShaderProgram, "mvp");
		GLint modelLoc = glGetUniformLocation(m_meshShaderProgram, "model");


		for (ModernMesh& modernMesh : m_modernMeshes) {
			if (modernMesh.vao == 0) {
				// OpenGL upload
				glGenVertexArrays(1, &modernMesh.vao);
				glBindVertexArray(modernMesh.vao);

				glGenBuffers(1, &modernMesh.vbo);
				glBindBuffer(GL_ARRAY_BUFFER, modernMesh.vbo);
				glBufferData(GL_ARRAY_BUFFER, modernMesh.vertexData.size() * sizeof(float), modernMesh.vertexData.data(), GL_STATIC_DRAW);

				glGenBuffers(1, &modernMesh.ebo);
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, modernMesh.ebo);
				glBufferData(GL_ELEMENT_ARRAY_BUFFER, modernMesh.indices.size() * sizeof(unsigned int), modernMesh.indices.data(), GL_STATIC_DRAW);
				
				// Attribute layout: pos(3), normal(3), texcoord(2)
				int stride = 8 * sizeof(float);
				glEnableVertexAttribArray(0); // position
				glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
				glEnableVertexAttribArray(1); // normal
				glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
				glEnableVertexAttribArray(2); // texcoord
				glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));

				glBindVertexArray(0);				
			}
		}
		
		std::function<void(const ModernSceneNode&, QMatrix4x4)> drawNodeModern;
		drawNodeModern = [&](const ModernSceneNode& node, QMatrix4x4 parentTransform) {
			QMatrix4x4 globalTransform = parentTransform * node.transform;
			QMatrix4x4 mvp = m_projectionMatrix * m_viewMatrix * globalTransform;
						
			for (int meshIdx : node.meshIndices) {
				if (meshIdx < 0 || meshIdx >= int(m_modernMeshes.size())) continue;
				const ModernMesh& mesh = m_modernMeshes[meshIdx];
				if (mesh.vao == 0 || mesh.indexCount == 0) continue;

				GLint colorLoc = glGetUniformLocation(m_meshShaderProgram, "color");
				float color[4] = { mesh.color.x(), mesh.color.y(), mesh.color.z(), mesh.color.w() };
				glUniform4fv(colorLoc, 1, color);

				glBindVertexArray(mesh.vao);
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
								
				glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, mvp.constData());
				glUniformMatrix4fv(modelLoc, 1, GL_FALSE, globalTransform.constData());
								
				glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, 0);

				glBindVertexArray(0);
			}
			for (const ModernSceneNode& child : node.children)
				drawNodeModern(child, globalTransform);
			};

		drawNodeModern(m_modernRootNode, QMatrix4x4());
		glUseProgram(0);
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

	// Final camera position offset along Z axis (behind object)
	QVector3D center(m_viewCenter.x, m_viewCenter.y, m_viewCenter.z);
	QVector3D camPos = center + QVector3D(0, 0, m_cameraDistance);

	m_camera->setViewRange(m_viewRadius * 2.1f);

	QVector3D viewPos(m_viewCenter.x, m_viewCenter.y, m_viewCenter.z);
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
	glColor3f(0.8f, 0.8f, 0.0f);
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


	if (m_camera->getProjectionType() == GLCamera::ProjectionType::PERSPECTIVE) {
		// Narrow FOV for better appearance in small viewport
		gluPerspective(30.0, 1.0, 0.1, 10.0);
		glScalef(1.0f, 1.0f, 1.0f); // Scale to normal size
	}
	else {
		// Orthographic projection for overlay
		float orthoSize = 1.5f;
		glOrtho(-orthoSize, orthoSize, -orthoSize, orthoSize, 0.1, 10.0);
		glScalef(1.25f, 1.25f, 1.25f); // Scale to fit viewport
	}

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
	
	for (const ModernMesh& mesh : m_modernMeshes) {
		if (mesh.vao) glDeleteVertexArrays(1, &mesh.vao);
		if (mesh.vbo) glDeleteBuffers(1, &mesh.vbo);
		if (mesh.ebo) glDeleteBuffers(1, &mesh.ebo);
	}
	m_modernMeshes.clear();
	m_modernRootNode = ModernSceneNode();

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

	m_modernMeshes.reserve(m_scene->mNumMeshes);

	// --- Upload all meshes ---
	for (unsigned int i = 0; i < m_scene->mNumMeshes; ++i) {
		const aiMesh* mesh = m_scene->mMeshes[i];		
		ModernMesh modernMesh;
		modernMesh.name = mesh->mName.C_Str();
		modernMesh.materialIndex = mesh->mMaterialIndex;

		for (unsigned int v = 0; v < mesh->mNumVertices; ++v) {
			// Position
			modernMesh.vertexData.push_back(mesh->mVertices[v].x);
			modernMesh.vertexData.push_back(mesh->mVertices[v].y);
			modernMesh.vertexData.push_back(mesh->mVertices[v].z);
			// Normal
			if (mesh->HasNormals()) {
				modernMesh.vertexData.push_back(mesh->mNormals[v].x);
				modernMesh.vertexData.push_back(mesh->mNormals[v].y);
				modernMesh.vertexData.push_back(mesh->mNormals[v].z);
			}
			else {
				modernMesh.vertexData.push_back(0.0f);
				modernMesh.vertexData.push_back(0.0f);
				modernMesh.vertexData.push_back(0.0f);
			}
			// Texcoord (first channel)
			if (mesh->HasTextureCoords(0)) {
				modernMesh.vertexData.push_back(mesh->mTextureCoords[0][v].x);
				modernMesh.vertexData.push_back(mesh->mTextureCoords[0][v].y);
			}
			else {
				modernMesh.vertexData.push_back(0.0f);
				modernMesh.vertexData.push_back(0.0f);
			}
		}

		for (unsigned int f = 0; f < mesh->mNumFaces; ++f) {
			const aiFace& face = mesh->mFaces[f];
			for (unsigned int j = 0; j < face.mNumIndices; ++j) {
				if (face.mIndices[j] >= mesh->mNumVertices) {
					qDebug() << "Out-of-bounds index:" << face.mIndices[j] << "in mesh" << i;
				}
				modernMesh.indices.push_back(face.mIndices[j]);
			}
		}

		aiColor4D diffuse(0.8f, 0.8f, 0.8f, 1.0f);
		aiMaterial* material = m_scene->mMaterials[mesh->mMaterialIndex];
		if (AI_SUCCESS == aiGetMaterialColor(material, AI_MATKEY_COLOR_DIFFUSE, &diffuse)) {
			modernMesh.color = QVector4D(diffuse.r, diffuse.g, diffuse.b, diffuse.a);
		}
		else {
			modernMesh.color = QVector4D(0.8f, 0.8f, 0.8f, 1.0f);
		}
				
		modernMesh.indexCount = static_cast<int>(modernMesh.indices.size());
				
		for (size_t idx = 0; idx < modernMesh.indices.size(); ++idx) {
			if (modernMesh.indices[idx] >= mesh->mNumVertices) {
				qDebug() << "Out-of-bounds index:" << modernMesh.indices[idx] << "in mesh" << i;
				break; // Stop after first error
			}
		}
				
		m_modernMeshes.push_back(std::move(modernMesh));
	}

	// --- Build scene graph ---
	std::function<ModernSceneNode(const aiNode*)> buildNode = [&](const aiNode* node) -> ModernSceneNode {
		ModernSceneNode n;
		n.name = node->mName.C_Str();
		// Convert aiMatrix4x4 to QMatrix4x4
		const aiMatrix4x4& m = node->mTransformation;
		n.transform = QMatrix4x4(
			m.a1, m.b1, m.c1, m.d1,
			m.a2, m.b2, m.c2, m.d2,
			m.a3, m.b3, m.c3, m.d3,
			m.a4, m.b4, m.c4, m.d4
		);
		for (unsigned int i = 0; i < node->mNumMeshes; ++i)
			n.meshIndices.push_back(node->mMeshes[i]);
		for (unsigned int i = 0; i < node->mNumChildren; ++i)
			n.children.push_back(buildNode(node->mChildren[i]));
		return n;
		};
	m_modernRootNode = buildNode(m_scene->mRootNode);

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
	QVector3D Z(0, 0, 0); // instead of 0 for x and y we need worldPosition.x() and worldPosition.y() ....
	Z = Z.project(m_viewMatrix * m_modelMatrix, m_projectionMatrix, QRect(0, 0, width(), height()));
	QVector3D p1(start.x(), height() - start.y(), Z.z());
	QVector3D O = p1.unproject(m_viewMatrix * m_modelMatrix, m_projectionMatrix, QRect(0, 0, width(), height()));
	QVector3D p2(end.x(), height() - end.y(), Z.z());
	QVector3D P = p2.unproject(m_viewMatrix * m_modelMatrix, m_projectionMatrix, QRect(0, 0, width(), height()));
	QVector3D OP = P - O;
	return OP;
}