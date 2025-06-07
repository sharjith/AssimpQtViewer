#include "ShaderProgram.h"
#include <QFile>
#include <QTextStream>
#include <QDebug>

ShaderProgram::ShaderProgram() {}

ShaderProgram::~ShaderProgram() {}

bool ShaderProgram::load(const QString& vertexPath, const QString& fragmentPath) {
    if (!m_program.addShaderFromSourceFile(QOpenGLShader::Vertex, vertexPath)) {
        qDebug() << "Vertex shader error:" << m_program.log();
        return false;
    }
    if (!m_program.addShaderFromSourceFile(QOpenGLShader::Fragment, fragmentPath)) {
        qDebug() << "Fragment shader error:" << m_program.log();
        return false;
    }
    if (!m_program.link()) {
        qDebug() << "Shader link error:" << m_program.log();
        return false;
    }
    return true;
}

void ShaderProgram::use() {
    m_program.bind();
}

void ShaderProgram::setUniform(const QString& name, const QMatrix4x4& value) {
    m_program.setUniformValue(name.toUtf8().data(), value);
}

void ShaderProgram::setUniform(const QString& name, const QVector3D& value) {
    m_program.setUniformValue(name.toUtf8().data(), value);
}

void ShaderProgram::setUniform(const QString& name, float value) {
    m_program.setUniformValue(name.toUtf8().data(), value);
}

void ShaderProgram::setUniform(const QString& name, int value) {
    m_program.setUniformValue(name.toUtf8().data(), value);
}

QOpenGLShaderProgram* ShaderProgram::program() {
    return &m_program;
}
