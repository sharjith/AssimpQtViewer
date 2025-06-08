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

    // Extract vertices
    for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
        Vertex v;
        v.position = QVector3D(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
        v.normal = QVector3D(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);

        if (mesh->HasTextureCoords(0)) {
            v.texCoord = QVector2D(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
        }
        else {
            v.texCoord = QVector2D(0.0f, 0.0f);
        }
        vertices.append(v);
    }

    // Extract indices
    for (unsigned int i = 0; i < mesh->mNumFaces; ++i) {
        const aiFace& face = mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; ++j) {
            indices.append(face.mIndices[j]);
        }
    }
    m_indexCount = indices.size();

    // Create and bind VAO
    if (!m_vao.create()) {
        qWarning() << "Failed to create VAO";
        return;
    }
    m_vao.bind();

    // Create and fill VBO
    if (!m_vbo.create()) {
        qWarning() << "Failed to create VBO";
        m_vao.release();
        return;
    }
    m_vbo.bind();
    m_vbo.allocate(vertices.constData(), vertices.size() * sizeof(Vertex));

    // Create and fill EBO
    if (!m_ebo.create()) {
        qWarning() << "Failed to create EBO";
        m_vao.release();
        return;
    }
    m_ebo.bind();
    m_ebo.allocate(indices.constData(), indices.size() * sizeof(unsigned int));

    // Set vertex attribute pointers
    glEnableVertexAttribArray(0); // position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, position)));

    glEnableVertexAttribArray(1); // normal
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, normal)));

    glEnableVertexAttribArray(2); // texCoord
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, texCoord)));

    // Unbind VAO (this also unbinds EBO from current context)
    m_vao.release();

    // Optional: unbind VBO and EBO here, just for cleanliness
    m_vbo.release();
    m_ebo.release();

    qDebug() << "SetupMesh: vertices=" << vertices.size() << " indices=" << indices.size();
}


void GLMesh::draw() {
    if (!m_vao.isCreated()) {
        qWarning() << "Attempt to draw an uninitialized mesh!";
        return;
    }
    if (m_indexCount == 0) {
        qWarning() << "No indices to draw!";
        return;
    }

    m_vao.bind();

    GLenum err = glGetError();
    if (err != GL_NO_ERROR)
        qWarning() << "Pre-glDrawElements OpenGL Error:" << err;

    // Do NOT bind m_ebo here! VAO remembers the EBO binding
    glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, nullptr);

    err = glGetError();
    if (err != GL_NO_ERROR)
        qWarning() << "Post-glDrawElements OpenGL Error:" << err;

    m_vao.release();
}

