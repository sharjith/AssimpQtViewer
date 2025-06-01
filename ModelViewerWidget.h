#pragma once

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <QOpenGLFunctions>
#include <QOpenGLWidget>
#include <QString>
#include <QToolButton>
#include <QMatrix4x4>

class GLCamera;

enum class ViewProjection {
    Top,
    Front,
    Left,
	Bottom,
	Rear,
	Right,
    Axonometric,
    Custom
};

class ModelViewerWidget : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT
public:
    explicit ModelViewerWidget(QWidget *parent = nullptr);
    void loadModel(const QString &filePath);    
    const aiScene* getScene() const { return m_scene; }
    void highlightNode(aiNode *node);
	Assimp::Importer* getImporter() { return &m_importer; }
protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

	void mousePressEvent(QMouseEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;
	void mouseReleaseEvent(QMouseEvent* event) override;
	void wheelEvent(QWheelEvent* event) override;
	void keyPressEvent(QKeyEvent* event) override;
	/*void keyReleaseEvent(QKeyEvent* event) override;
	void focusInEvent(QFocusEvent* event) override;
	void focusOutEvent(QFocusEvent* event) override;*/

    void resizeEvent(QResizeEvent* event) override;


signals:
    void nodePicked(aiNode* node);

private slots:
    void onInertiaTimeout();

private:   
    void computeBoundingBox(const aiScene* scene, const aiNode* node,
        aiVector3D& minimum, aiVector3D& maximum, const aiMatrix4x4& transform);
    void drawGradientBackground();
    void drawTrihedron(float axisLength = 1.0f, float axisRadius = 0.05f, float coneHeight = 0.2f, float coneRadius = 0.1f, float sphereRadius = 0.1f);
    void drawTrihedronOverlay();
    void drawNode(aiNode* node);
    GLuint loadTextureIfNeeded(const aiMaterial* material, unsigned int materialIndex);
	void updateCamera();
    void resetView();
    void setViewProjection(ViewProjection view);
    void setViewTop();
    void setViewFront();
    void setViewLeft();
    void setViewBottom();
    void setViewRear();
    void setViewRight();
    void setViewAxonometric();    
    void fitToView();
    void pickAtScreenPosition(const QPoint& pos);
    void pickRay(const aiVector3D& origin, const aiVector3D& dir);
    bool rayIntersectsTriangle(
        const aiVector3D& orig, const aiVector3D& dir,
        const aiVector3D& v0, const aiVector3D& v1, const aiVector3D& v2,
        float& outT
    );
    aiNode* findNodeForMesh(aiNode* node, int meshIndex);

    QVector3D get3dTranslationVectorFromMousePoints(const QPoint& start, const QPoint& end);
    
    QToolButton* createViewButton(const QString& iconPath, const QString& tooltip, const std::function<void()>& callback, QWidget* parent = nullptr);

private:
    const aiScene* m_scene = nullptr;
    aiNode* m_highlightedNode = nullptr;
    Assimp::Importer m_importer;    

	GLCamera* m_camera = nullptr; // Custom camera class for handling camera operations

    aiVector3D m_cameraPos;   // Current camera position
    aiVector3D m_viewCenter;  // Model center point
    aiVector3D m_upVector;    // Up direction for the camera (usually (0,1,0))
    
    float m_viewRadius;
    float m_cameraDistance;
    bool m_sceneUpdated;

    // Interaction modes
    enum class InteractionMode { Select, Zoom, Rotate, Pan };

    QPoint m_lastMousePos;
	QPoint m_totalMouseDelta;
    InteractionMode m_mode = InteractionMode::Select;

	ViewProjection m_viewProjection = ViewProjection::Custom;

    QWidget* _viewToolbar;

    bool m_isDragging = false;
	aiNode* m_lastPickedNode = nullptr;
    
    QTimer* m_inertiaTimer = nullptr;
    QVector2D m_rotationVelocity;
    QVector3D m_panVelocity;
    float m_zoomVelocity = 0.0f;
	float m_inertiaFactor = 0.95f; // Factor to reduce velocity each frame
    bool m_inertiaActive = false;
        
    std::unordered_map<unsigned int, GLuint> m_materialTextureCache;
	QString m_lastModelPath;

	QMatrix4x4 m_viewMatrix;
    QMatrix4x4 m_modelMatrix; // Model matrix for transformations
    QMatrix4x4 m_projectionMatrix;       
};
