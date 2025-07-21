#include "GLMesh.h"
#include <QOpenGLShaderProgram>
#include <assimp/mesh.h>
#include <QDebug>

GLMesh::GLMesh(aiMesh *mesh, QOpenGLShaderProgram *program) : m_mesh(mesh), m_program(program), m_textureId(0)
{
	m_name = QString::fromStdString(mesh->mName.C_Str());
	m_name = m_name.isEmpty() ? QString("Mesh") : m_name;
    initializeOpenGLFunctions();
}

GLMesh::~GLMesh()
{
}


void GLMesh::setMaterial(const Material &material)
{
    m_material = material;
}

void GLMesh::setTexture(GLuint textureId)
{
    if (m_hasTexture)
    {
        // delete previous texture if it exists
        if (m_textureId != 0)
        {
            glDeleteTextures(1, &m_textureId);
        }
    } else
    {
        m_hasTexture = false;
    }
    m_textureId = textureId;
    m_hasTexture = (textureId > 0);
}

void GLMesh::setTextures(const MaterialTextures &textures)
{
    m_textures = textures;
    m_hasAnyTexture = (textures.diffuse > 0 || textures.specular > 0 ||
                       textures.emissive > 0 || textures.height > 0 ||
                       textures.displacement > 0 || textures.opacity > 0 ||
                       textures.metallic > 0 || textures.roughness > 0 ||
                       textures.normal > 0);
}

void GLMesh::setupMesh()
{
    QVector<Vertex> vertices;
    QVector<unsigned int> indices;

    for (unsigned int i = 0; i < m_mesh->mNumVertices; ++i)
    {
        Vertex v;
        v.position = QVector3D(m_mesh->mVertices[i].x, m_mesh->mVertices[i].y, m_mesh->mVertices[i].z) * m_modelMatrix;
        v.normal = QVector3D(m_mesh->mNormals[i].x, m_mesh->mNormals[i].y, m_mesh->mNormals[i].z) * m_modelMatrix;

        if (m_mesh->HasTextureCoords(0))
        {
            v.texCoord = QVector2D(m_mesh->mTextureCoords[0][i].x, m_mesh->mTextureCoords[0][i].y);
        } else
        {
            v.texCoord = QVector2D(0.0f, 0.0f);
        }
        if (m_mesh->HasVertexColors(0))
        {
            aiColor4D c = m_mesh->mColors[0][i];
            v.color = QVector4D(c.r, c.g, c.b, c.a);
        } else
        {
            v.color = m_material.diffuse;
        }
        vertices.append(v);
    }

    for (unsigned int i = 0; i < m_mesh->mNumFaces; ++i)
    {
        const aiFace &face = m_mesh->mFaces[i];
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

void GLMesh::draw()
{
    // Bind textures to different texture units
    if (m_hasAnyTexture)
    {
        // Diffuse texture - Texture unit 0
        if (m_textures.diffuse > 0)
        {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, m_textures.diffuse);
            m_program->setUniformValue("u_diffuseTexture", 0);
            m_program->setUniformValue("u_hasDiffuse", true);
        } else
        {
            m_program->setUniformValue("u_hasDiffuse", false);
        }

        // Specular texture - Texture unit 1
        if (m_textures.specular > 0)
        {
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, m_textures.specular);
            m_program->setUniformValue("u_specularTexture", 1);
            m_program->setUniformValue("u_hasSpecular", true);
        } else
        {
            m_program->setUniformValue("u_hasSpecular", false);
        }

        // Emissive texture - Texture unit 2
        if (m_textures.emissive > 0)
        {
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, m_textures.emissive);
            m_program->setUniformValue("u_emissiveTexture", 2);
            m_program->setUniformValue("u_hasEmissive", true);
        } else
        {
            m_program->setUniformValue("u_hasEmissive", false);
        }

        // Height/Normal texture - Texture unit 3
        if (m_textures.height > 0)
        {
            glActiveTexture(GL_TEXTURE3);
            glBindTexture(GL_TEXTURE_2D, m_textures.height);
            m_program->setUniformValue("u_heightTexture", 3);
            m_program->setUniformValue("u_hasHeight", true);
        } else
        {
            m_program->setUniformValue("u_hasHeight", false);
        }

        // Displacement texture - Texture unit 4
        if (m_textures.displacement > 0)
        {
            glActiveTexture(GL_TEXTURE4);
            glBindTexture(GL_TEXTURE_2D, m_textures.displacement);
            m_program->setUniformValue("u_displacementTexture", 4);
            m_program->setUniformValue("u_hasDisplacement", true);
        } else
        {
            m_program->setUniformValue("u_hasDisplacement", false);
        }

        // Opacity texture - Texture unit 5
        if (m_textures.opacity > 0)
        {
            glActiveTexture(GL_TEXTURE5);
            glBindTexture(GL_TEXTURE_2D, m_textures.opacity);
            m_program->setUniformValue("u_opacityTexture", 5);
            m_program->setUniformValue("u_hasOpacity", true);
        } else
        {
            m_program->setUniformValue("u_hasOpacity", false);
        }

        // Metallic texture - Texture unit 6
        if (m_textures.metallic > 0)
        {
            glActiveTexture(GL_TEXTURE6);
            glBindTexture(GL_TEXTURE_2D, m_textures.metallic);
            m_program->setUniformValue("u_metallicTexture", 6);
            m_program->setUniformValue("u_hasMetallic", true);
        } else
        {
            m_program->setUniformValue("u_hasMetallic", false);
        }

        // Roughness texture - Texture unit 7
        if (m_textures.roughness > 0)
        {
            glActiveTexture(GL_TEXTURE7);
            glBindTexture(GL_TEXTURE_2D, m_textures.roughness);
            m_program->setUniformValue("u_roughnessTexture", 7);
            m_program->setUniformValue("u_hasRoughness", true);
        } else
        {
            m_program->setUniformValue("u_hasRoughness", false);
        }

        // Normal texture - Texture unit 8
        if (m_textures.normal > 0)
        {
            glActiveTexture(GL_TEXTURE8);
            glBindTexture(GL_TEXTURE_2D, m_textures.normal);
            m_program->setUniformValue("u_normalTexture", 8);
            m_program->setUniformValue("u_hasNormal", true);
        } else
        {
            m_program->setUniformValue("u_hasNormal", false);
        }
    } else
    {
        // No textures available
        m_program->setUniformValue("u_hasDiffuse", false);
        m_program->setUniformValue("u_hasSpecular", false);
        m_program->setUniformValue("u_hasEmissive", false);
        m_program->setUniformValue("u_hasHeight", false);
        m_program->setUniformValue("u_hasDisplacement", false);
        m_program->setUniformValue("u_hasOpacity", false);
        m_program->setUniformValue("u_hasMetallic", false);
        m_program->setUniformValue("u_hasRoughness", false);
        m_program->setUniformValue("u_hasNormal", false);
    }


    m_program->setUniformValue("opacity", m_material.opacity);

    m_program->enableAttributeArray("vertexPosition");
    m_program->enableAttributeArray("vertexNormal");
    m_program->enableAttributeArray("vertexTexCoord");
    m_program->enableAttributeArray("vertexColor");
    m_program->enableAttributeArray("tangent");
    m_program->enableAttributeArray("bitangent");

    m_vbo.bind();
    m_program->setAttributeBuffer("vertexPosition", GL_FLOAT, offsetof(Vertex, position), 3, sizeof(Vertex));
    m_program->setAttributeBuffer("vertexNormal", GL_FLOAT, offsetof(Vertex, normal), 3, sizeof(Vertex));
    m_program->setAttributeBuffer("vertexTexCoord", GL_FLOAT, offsetof(Vertex, texCoord), 2, sizeof(Vertex));
    m_program->setAttributeBuffer("vertexColor", GL_FLOAT, offsetof(Vertex, color), 4, sizeof(Vertex));
    m_program->setAttributeBuffer("tangent", GL_FLOAT, offsetof(Vertex, normal), 3, sizeof(Vertex));
    m_program->setAttributeBuffer("bitangent", GL_FLOAT, offsetof(Vertex, normal), 3, sizeof(Vertex));

    m_ebo.bind();
    glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, nullptr);

    // Clean up - disable attributes
    m_program->disableAttributeArray("vertexPosition");
    m_program->disableAttributeArray("vertexNormal");
    m_program->disableAttributeArray("vertexTexCoord");
    m_program->disableAttributeArray("vertexColor");
    m_program->disableAttributeArray("tangent");
    m_program->disableAttributeArray("bitangent");

    // Unbind texture
    if (m_hasTexture)
    {
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    m_vbo.release();
    m_ebo.release();
}

void GLMesh::setModelMatrix(const QMatrix4x4 &mat)
{
    m_modelMatrix = mat;
	qDebug() << "Mesh: " << m_name << " - Model matrix set:" << m_modelMatrix;
}

const QMatrix4x4 &GLMesh::modelMatrix() const
{
    return m_modelMatrix;
}
