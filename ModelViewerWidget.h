#pragma once

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <QOpenGLFunctions>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLExtraFunctions>
#include <QOpenGLWidget>
#include <QString>
#include <QToolButton>
#include <QMatrix4x4>
#include "GLMesh.h"
#include "Trihedron.h"
#include "ShaderProgram.h"

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

class ModelViewerWidget : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core {
    Q_OBJECT
public:
    explicit ModelViewerWidget(QWidget *parent = nullptr);
    void loadModel(const QString &filePath);    
    const aiScene* getScene() const { return m_scene; }
    void highlightNode(aiNode *node);
    void highlightMesh(int meshIndex);
    void clearHighlight();

	Assimp::Importer* getImporter() { return &m_importer; }

    aiNode* findNodeForMesh(aiNode* node, int meshIndex);

	void setMeshVisibility(int meshIndex, bool visible);
	
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
    void meshPicked(int meshIndex);

private slots:
    void onInertiaTimeout();

private:   
    void loadNodeMeshes(aiNode* node);
    void computeBoundingBox(const aiScene* scene, const aiNode* node,
        aiVector3D& minimum, aiVector3D& maximum, const aiMatrix4x4& transform);
    void computeBoundingSphere(const aiMesh* iMesh, const aiMatrix4x4& transform, aiVector3D& oCenter, float& oRadius);
    void computeBounds(const aiMesh* mesh, const aiMatrix4x4& currentTransform, aiVector3D& minimum, aiVector3D& maximum);
    void drawGradientBackground();   
    void drawTrihedronOverlay();
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
    bool rayIntersectsSphere(const aiVector3D& origin, const aiVector3D& dir, const aiVector3D& center, float radius);
    bool rayIntersectsTriangle(
        const aiVector3D& orig, const aiVector3D& dir,
        const aiVector3D& v0, const aiVector3D& v1, const aiVector3D& v2,
        float& outT
    );
    
    QVector3D get3dTranslationVectorFromMousePoints(const QPoint& start, const QPoint& end);
    
    QToolButton* createViewButton(const QString& iconPath, const QString& tooltip, const std::function<void()>& callback, QWidget* parent = nullptr);

private:
    const aiScene* m_scene = nullptr;
    Assimp::Importer m_importer;   

	GLCamera* m_camera = nullptr; // Custom camera class for handling camera operations

    QVector3D m_cameraPos;   // Current camera position
    QVector3D m_viewCenter;  // Model center point
    QVector3D m_upVector;    // Up direction for the camera (usually (0,1,0))
    
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
    QVector3D m_zoomPanVelocity;
    float m_zoomVelocity = 0.0f;
	float m_inertiaFactor = 0.75f; // Factor to reduce velocity each frame
    bool m_inertiaActive = false;
        
    std::unordered_map<unsigned int, GLuint> m_materialTextureCache;
	QString m_lastModelPath;

	QMatrix4x4 m_viewMatrix;
    QMatrix4x4 m_modelMatrix; // Model matrix for transformations
    QMatrix4x4 m_projectionMatrix;     

    int m_lastPickedMeshIndex = -1;
    aiNode* m_highlightedNode = nullptr;
    int m_highlightedMeshIndex = -1;
    std::vector<std::unique_ptr<GLMesh>> m_glMeshes;
    std::map<aiNode*, std::vector<GLMesh*>> m_nodeMeshes; // Store raw pointers
    std::map<int, GLMesh*> m_meshIndexToGLMesh; // Store raw pointers

    ShaderProgram m_shader;
	ShaderProgram m_backgroundShader; // Shader for background gradient
	
    std::unique_ptr<Trihedron> m_trihedron; // Trihedron for orientation reference    
    ShaderProgram m_trihedronShader; // Shader for overlay elements like trihedron

};
