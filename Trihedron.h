#ifndef TRIHEDRON_H
#define TRIHEDRON_H

#include <QOpenGLFunctions_3_3_Core>
#include <QMatrix4x4>
#include "ShaderProgram.h"

class TextRenderer;

class Trihedron : protected QOpenGLFunctions_3_3_Core
{
public:
    explicit Trihedron(ShaderProgram *shader);

    void generateCylinderGeometry();

    void generateConeGeometry();

    void generateSphereGeometry();

    void drawCylinder(const QMatrix4x4 &modelMatrix);

    void drawCone(const QMatrix4x4 &modelMatrix);

    void drawSphere(const QMatrix4x4 &modelMatrix);

    void draw(const QMatrix4x4 &viewMatrix, const QMatrix4x4 &projectionMatrix, const float& scale = 1.0f);

private:
    ShaderProgram* m_shader;
    GLuint m_cylinderVAO, m_coneVAO, m_sphereVAO;
    GLuint m_sphereVBO;
    GLuint m_sphereEBO;
    int m_cylinderVertexCount, m_coneVertexCount, m_sphereIndexCount;

    std::unique_ptr<ShaderProgram> m_trihedronTextShader; // Shader for trihedron text labels
    TextRenderer* m_axisTextRenderer;
};

#endif // TRIHEDRON_H
