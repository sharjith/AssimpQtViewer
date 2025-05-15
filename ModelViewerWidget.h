#pragma once

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QString>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>

class ModelViewerWidget : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT
public:
    explicit ModelViewerWidget(QWidget *parent = nullptr);
    void loadModel(const QString &filePath);
    const aiScene* getScene() const { return scene; }
    void highlightNode(aiNode *node);
protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

	void mousePressEvent(QMouseEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;
	void mouseReleaseEvent(QMouseEvent* event) override;
	void wheelEvent(QWheelEvent* event) override;
	/*void keyPressEvent(QKeyEvent* event) override;
	void keyReleaseEvent(QKeyEvent* event) override;
	void focusInEvent(QFocusEvent* event) override;
	void focusOutEvent(QFocusEvent* event) override;*/

private:
	void updateCamera();
private:
    const aiScene *scene = nullptr;
    aiNode *highlightedNode = nullptr;
    Assimp::Importer importer;
    void drawNode(aiNode *node);

    aiVector3D _viewCenter;
    float _viewRadius;
    float _cameraDistance;

    // Interaction modes
    enum class InteractionMode { None, Rotate, Pan };

    QPoint m_lastMousePos;
    float m_rotationX = 0.0f, m_rotationY = 0.0f;
    float m_panX = 0.0f, m_panY = 0.0f;
    float m_zoom = 1.0f;
    InteractionMode m_mode = InteractionMode::None;
};
