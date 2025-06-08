#pragma once

#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QMatrix4x4>
#include <assimp/scene.h>

class GLMesh : protected QOpenGLFunctions_3_3_Core {
public:
    GLMesh(aiMesh* mesh);
    ~GLMesh();

    void draw();
    void setModelMatrix(const QMatrix4x4& mat);
    const QMatrix4x4& modelMatrix() const;

private:
    void setupMesh(aiMesh* mesh);

    QOpenGLVertexArrayObject m_vao;
    QOpenGLBuffer m_vbo{QOpenGLBuffer::VertexBuffer};
    QOpenGLBuffer m_ebo{QOpenGLBuffer::IndexBuffer};

    int m_indexCount = 0;
    QMatrix4x4 m_modelMatrix;
};
