#pragma once

#include <QOpenGLShaderProgram>
#include <QString>

class ShaderProgram {
public:
    ShaderProgram();
    ~ShaderProgram();

    bool load(const QString& vertexPath, const QString& fragmentPath);
    void use();
    void release();
    void setUniform(const QString& name, const QMatrix4x4& value);
    void setUniform(const QString& name, const QVector3D& value);
    void setUniform(const QString& name, const QVector4D& value);
    void setUniform(const QString& name, float value);
    void setUniform(const QString& name, int value);
    QOpenGLShaderProgram* program();

private:
    QOpenGLShaderProgram m_program;
};
