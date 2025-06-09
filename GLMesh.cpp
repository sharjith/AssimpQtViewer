#include "GLMesh.h"
#include <QOpenGLShaderProgram>
#include <assimp/mesh.h>
#include <QDebug>

GLMesh::GLMesh(aiMesh* mesh, QOpenGLShaderProgram* program) : m_mesh(mesh), m_program(program) {
    initializeOpenGLFunctions();    
}

GLMesh::~GLMesh() {}


void GLMesh::setMaterial(const Material& material) {
    m_material = material;
}

void GLMesh::setupMesh() {
    QVector<Vertex> vertices;
    QVector<unsigned int> indices;

    for (unsigned int i = 0; i < m_mesh->mNumVertices; ++i) {
        Vertex v;
        v.position = QVector3D(m_mesh->mVertices[i].x, m_mesh->mVertices[i].y, m_mesh->mVertices[i].z);
        v.normal = QVector3D(m_mesh->mNormals[i].x, m_mesh->mNormals[i].y, m_mesh->mNormals[i].z);

        if (m_mesh->HasTextureCoords(0)) {
            v.texCoord = QVector2D(m_mesh->mTextureCoords[0][i].x, m_mesh->mTextureCoords[0][i].y);
        }
        else {
            v.texCoord = QVector2D(0.0f, 0.0f);
        }
        if (m_mesh->HasVertexColors(0)) {
            aiColor4D c = m_mesh->mColors[0][i];
            v.color = QVector4D(c.r, c.g, c.b, c.a);
        }
        else {			
            v.color = m_material.diffuse;
        }
        vertices.append(v);
    }

    for (unsigned int i = 0; i < m_mesh->mNumFaces; ++i) {
        const aiFace& face = m_mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; ++j)
            indices.append(face.mIndices[j]);
    }
    m_indexCount = indices.size();

    m_vbo.create();
    m_vbo.bind();
    m_vbo.allocate(vertices.constData(), vertices.size() * sizeof(Vertex));

    m_ebo.create();
    m_ebo.bind();
    m_ebo.allocate(indices.constData(), indices.size() * sizeof(unsigned int));
}

void GLMesh::draw() {
    m_program->enableAttributeArray("vertexPosition");
    m_program->enableAttributeArray("vertexNormal");
    m_program->enableAttributeArray("vertexTexCoord");
    m_program->enableAttributeArray("vertexColor");

    m_vbo.bind();
    m_program->setAttributeBuffer("vertexPosition", GL_FLOAT, offsetof(Vertex, position), 3, sizeof(Vertex));
    m_program->setAttributeBuffer("vertexNormal", GL_FLOAT, offsetof(Vertex, normal), 3, sizeof(Vertex));
    m_program->setAttributeBuffer("vertexTexCoord", GL_FLOAT, offsetof(Vertex, texCoord), 2, sizeof(Vertex));
    m_program->setAttributeBuffer("vertexColor", GL_FLOAT, offsetof(Vertex, color), 4, sizeof(Vertex));

    m_ebo.bind();
    glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, nullptr);
}

void GLMesh::setModelMatrix(const QMatrix4x4& mat) {
    m_modelMatrix = mat;
}

const QMatrix4x4& GLMesh::modelMatrix() const {
    return m_modelMatrix;
}