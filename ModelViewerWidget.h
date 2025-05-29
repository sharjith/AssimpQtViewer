#pragma once

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <QOpenGLFunctions>
#include <QOpenGLWidget>
#include <QString>
#include <QToolButton>

enum class ViewProjection {
    Top,
    Front,
    Left,
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
    void drawGradientBackground();
    void drawNode(aiNode* node);
    GLuint loadTextureIfNeeded(const aiMaterial* material, unsigned int materialIndex);
	void updateCamera();
    void resetView();
    void setViewProjection(ViewProjection view);
    void setViewTop();
    void setViewFront();
    void setViewLeft();
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
    
    QToolButton* createViewButton(const QString& iconPath, const QString& tooltip, const std::function<void()>& callback, QWidget* parent = nullptr);

private:
    const aiScene* m_scene = nullptr;
    aiNode* m_highlightedNode = nullptr;
    Assimp::Importer m_importer;    

    aiVector3D m_cameraPos;   // Current camera position
    aiVector3D m_viewCenter;  // Model center point
    aiVector3D m_upVector;    // Up direction for the camera (usually (0,1,0))
    
    float m_viewRadius;
    float m_cameraDistance;
    bool m_sceneUpdated;

    // Interaction modes
    enum class InteractionMode { Select, Zoom, Rotate, Pan };

    QPoint m_lastMousePos;
    float m_rotationX = 0.0f, m_rotationY = 0.0f;
    float m_panX = 0.0f, m_panY = 0.0f;
    float m_zoom = 1.0f;
    float m_azimuth = 45.0f;     // Horizontal angle in degrees
    float m_elevation = 35.0f;  // Vertical angle in degrees
    InteractionMode m_mode = InteractionMode::Select;

	ViewProjection m_viewProjection = ViewProjection::Custom;

    QWidget* _viewToolbar;

    float m_azimuthSpeed = 0.f;
    float m_elevationSpeed = 0.f;
    bool m_isDragging = false;
    QPointF m_panSpeed = { 0, 0 };
    float m_zoomSpeed = 0.f;
    QTimer* m_inertiaTimer = nullptr;
	aiNode* m_lastPickedNode = nullptr;
        
    std::unordered_map<unsigned int, GLuint> m_materialTextureCache;
	QString m_lastModelPath;
};
