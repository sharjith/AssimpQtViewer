#include "GLMesh.h"

struct Vertex {
    QVector3D position;
    QVector3D normal;
    QVector2D texCoord;
};

GLMesh::GLMesh(aiMesh* mesh) {
    initializeOpenGLFunctions();
    setupMesh(mesh);
}

GLMesh::~GLMesh() {
    m_vao.destroy();
    m_vbo.destroy();
    m_ebo.destroy();
}

void GLMesh::setModelMatrix(const QMatrix4x4& mat) {
    m_modelMatrix = mat;
}

const QMatrix4x4& GLMesh::modelMatrix() const {
    return m_modelMatrix;
}

void GLMesh::setupMesh(aiMesh* mesh) {
    QVector<Vertex> vertices;
    QVector<unsigned int> indices;

    for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
        Vertex v;
        v.position = QVector3D(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
        v.normal = QVector3D(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
        if (mesh->HasTextureCoords(0)) {
            v.texCoord = QVector2D(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
        } else {
            v.texCoord = QVector2D(0.0f, 0.0f);
        }
        vertices.append(v);
    }

    for (unsigned int i = 0; i < mesh->mNumFaces; ++i) {
        const aiFace& face = mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; ++j) {
            indices.append(face.mIndices[j]);
        }
    }
    m_indexCount = indices.size();

    m_vao.create();
    m_vao.bind();

    m_vbo.create();
    m_vbo.bind();
    m_vbo.allocate(vertices.constData(), vertices.size() * sizeof(Vertex));

    m_ebo.create();
    m_ebo.bind();
    m_ebo.allocate(indices.constData(), indices.size() * sizeof(unsigned int));

    glEnableVertexAttribArray(0); // position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, position)));

    glEnableVertexAttribArray(1); // normal
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, normal)));

    glEnableVertexAttribArray(2); // texCoord
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, texCoord)));

    m_vao.release();
}

void GLMesh::draw() {
    m_vao.bind();
    glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, nullptr);
    m_vao.release();
}

