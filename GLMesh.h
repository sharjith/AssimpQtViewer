#pragma once

#include <QOpenGLShaderProgram>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QMatrix4x4>
#include <QVector>
#include <QVector3D>
#include <QVector2D>

#include <assimp/scene.h>

struct Vertex {
    QVector3D position;
    QVector3D normal;
    QVector2D texCoord;
	QVector4D color; // Optional color attribute
};

struct Material {
    QVector4D ambient;
    QVector4D diffuse;
    QVector4D specular;
    float shininess = 32.0f;
};


class QOpenGLShaderProgram;

class GLMesh : protected QOpenGLFunctions_3_3_Core {
public:
    GLMesh(aiMesh* mesh, QOpenGLShaderProgram* program);
    ~GLMesh();
    void setMaterial(const Material& material);
    void setupMesh();
    void draw();
    void setModelMatrix(const QMatrix4x4& mat);
    const QMatrix4x4& modelMatrix() const;

    Material m_material;
private:

	aiMesh* m_mesh = nullptr;
    
    QOpenGLBuffer m_vbo{ QOpenGLBuffer::VertexBuffer };
    QOpenGLBuffer m_ebo{ QOpenGLBuffer::IndexBuffer };
    int m_indexCount = 0;
    
    QMatrix4x4 m_modelMatrix;
    QOpenGLShaderProgram* m_program;
};