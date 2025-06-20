#include "Trihedron.h"

Trihedron::Trihedron(ShaderProgram *shader)
    : m_shader(shader), m_cylinderVAO(0), m_coneVAO(0), m_sphereVAO(0),
      m_cylinderVertexCount(0), m_coneVertexCount(0), m_sphereIndexCount(0)
{
    initializeOpenGLFunctions();

    generateSphereGeometry();
    generateCylinderGeometry();
    generateConeGeometry();
}

void Trihedron::generateCylinderGeometry()
{
    std::vector<float> vertexData;
    const int segments = 12;
    const float radius = 0.05f;
    const float height = 1.0f;

    for (int i = 0; i <= segments; ++i)
    {
        float angle = 2.0f * M_PI * i / segments;
        float x = radius * cos(angle);
        float y = radius * sin(angle);

        // Normal for side surface (same for top and bottom at given x,y)
        float nx = cos(angle);
        float ny = sin(angle);
        float nz = 0.0f;

        // Bottom vertex
        vertexData.push_back(x); // position
        vertexData.push_back(y);
        vertexData.push_back(0.0f);
        vertexData.push_back(nx); // normal
        vertexData.push_back(ny);
        vertexData.push_back(nz);

        // Top vertex
        vertexData.push_back(x);
        vertexData.push_back(y);
        vertexData.push_back(height);
        vertexData.push_back(nx);
        vertexData.push_back(ny);
        vertexData.push_back(nz);
    }

    // ADD: Base cap vertices with blended normals
    float blendFactor = 0.6f; // Adjust this (0.0 = pure axial, 1.0 = pure radial)

    // Bottom cap center
    vertexData.push_back(0.0f);
    vertexData.push_back(0.0f);
    vertexData.push_back(0.0f);
    vertexData.push_back(0.0f); // blended normal
    vertexData.push_back(0.0f);
    vertexData.push_back(-1.0f + blendFactor * 0.5f); // slightly less downward

    // Bottom cap rim vertices
    for (int i = 0; i <= segments; ++i)
    {
        float angle = 2.0f * M_PI * i / segments;
        float x = radius * cos(angle);
        float y = radius * sin(angle);

        // Blended normal: mix radial and axial components
        float nx = blendFactor * cos(angle);
        float ny = blendFactor * sin(angle);
        float nz = -(1.0f - blendFactor); // negative for bottom face

        // Normalize the blended normal
        float length = sqrt(nx * nx + ny * ny + nz * nz);
        nx /= length;
        ny /= length;
        nz /= length;

        vertexData.push_back(x);
        vertexData.push_back(y);
        vertexData.push_back(0.0f);
        vertexData.push_back(nx);
        vertexData.push_back(ny);
        vertexData.push_back(nz);
    }

    // Top cap center
    vertexData.push_back(0.0f);
    vertexData.push_back(0.0f);
    vertexData.push_back(height);
    vertexData.push_back(0.0f);
    vertexData.push_back(0.0f);
    vertexData.push_back(1.0f - blendFactor * 0.5f); // slightly less upward

    // Top cap rim vertices
    for (int i = 0; i <= segments; ++i)
    {
        float angle = 2.0f * M_PI * i / segments;
        float x = radius * cos(angle);
        float y = radius * sin(angle);

        // Blended normal: mix radial and axial components
        float nx = blendFactor * cos(angle);
        float ny = blendFactor * sin(angle);
        float nz = (1.0f - blendFactor); // positive for top face

        // Normalize the blended normal
        float length = sqrt(nx * nx + ny * ny + nz * nz);
        nx /= length;
        ny /= length;
        nz /= length;

        vertexData.push_back(x);
        vertexData.push_back(y);
        vertexData.push_back(height);
        vertexData.push_back(nx);
        vertexData.push_back(ny);
        vertexData.push_back(nz);
    }

    m_cylinderVertexCount = (int) vertexData.size() / 6; // 3 pos + 3 normal

    // Generate VAO/VBO
    GLuint VBO;
    glGenVertexArrays(1, &m_cylinderVAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(m_cylinderVAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertexData.size() * sizeof(float), vertexData.data(), GL_STATIC_DRAW);

    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *) 0);
    glEnableVertexAttribArray(0);

    // Normal attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *) (3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void Trihedron::generateConeGeometry()
{
    std::vector<float> vertexData;
    const int segments = 12;
    const float radius = 0.1f;
    const float height = 0.2f;

    // ===== Base Circle (flat face) =====
    for (int i = 0; i <= segments; ++i)
    {
        float angle = 2.0f * M_PI * i / segments;
        float x = radius * cos(angle);
        float y = radius * sin(angle);

        // Center of base (normal pointing -Z)
        vertexData.push_back(0.0f);
        vertexData.push_back(0.0f);
        vertexData.push_back(0.0f);
        vertexData.push_back(0.0f);
        vertexData.push_back(0.0f);
        vertexData.push_back(-1.0f);

        // Perimeter point of base
        vertexData.push_back(x);
        vertexData.push_back(y);
        vertexData.push_back(0.0f);
        vertexData.push_back(0.0f);
        vertexData.push_back(0.0f);
        vertexData.push_back(-1.0f);
    }

    // ===== Side Surface =====
    for (int i = 0; i <= segments; ++i)
    {
        float angle = 2.0f * M_PI * i / segments;
        float x = radius * cos(angle);
        float y = radius * sin(angle);

        // Vector from cone tip to perimeter point
        float len = std::sqrt(x * x + y * y + height * height);
        float nx = x / len;
        float ny = y / len;
        float nz = radius / len; // From side normal of cone

        // Tip of the cone
        vertexData.push_back(0.0f);
        vertexData.push_back(0.0f);
        vertexData.push_back(height);
        vertexData.push_back(nx);
        vertexData.push_back(ny);
        vertexData.push_back(nz);

        // Perimeter point of base
        vertexData.push_back(x);
        vertexData.push_back(y);
        vertexData.push_back(0.0f);
        vertexData.push_back(nx);
        vertexData.push_back(ny);
        vertexData.push_back(nz);
    }

    m_coneVertexCount = static_cast<int>(vertexData.size()) / 6; // 3 position + 3 normal

    // Generate VAO/VBO
    GLuint VBO;
    glGenVertexArrays(1, &m_coneVAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(m_coneVAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertexData.size() * sizeof(float), vertexData.data(), GL_STATIC_DRAW);

    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *) 0);
    glEnableVertexAttribArray(0);

    // Normal attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *) (3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void Trihedron::generateSphereGeometry()
{
    std::vector<float> vertexData;
    std::vector<unsigned int> indices;
    float radius = 0.1f;
    int latitudeSegments = 12;
    int longitudeSegments = 12;

    vertexData.clear();
    indices.clear();

    // Generate vertices
    for (int lat = 0; lat <= latitudeSegments; ++lat)
    {
        float theta = M_PI * lat / latitudeSegments; // 0 to PI (top to bottom)
        float sinTheta = sin(theta);
        float cosTheta = cos(theta);

        for (int lon = 0; lon <= longitudeSegments; ++lon)
        {
            float phi = 2.0f * M_PI * lon / longitudeSegments; // 0 to 2PI (around)
            float sinPhi = sin(phi);
            float cosPhi = cos(phi);

            // Calculate position
            float x = radius * sinTheta * cosPhi;
            float y = radius * cosTheta;
            float z = radius * sinTheta * sinPhi;

            // Normal is the same as normalized position for a sphere centered at origin
            float nx = sinTheta * cosPhi;
            float ny = cosTheta;
            float nz = sinTheta * sinPhi;

            // Add vertex data (position + normal)
            vertexData.push_back(x); // position
            vertexData.push_back(y);
            vertexData.push_back(z);
            vertexData.push_back(nx); // normal
            vertexData.push_back(ny);
            vertexData.push_back(nz);
        }
    }

    // Generate indices for triangles
    for (int lat = 0; lat < latitudeSegments; ++lat)
    {
        for (int lon = 0; lon < longitudeSegments; ++lon)
        {
            int current = lat * (longitudeSegments + 1) + lon;
            int next = current + longitudeSegments + 1;

            // First triangle
            indices.push_back(current);
            indices.push_back(next);
            indices.push_back(current + 1);

            // Second triangle
            indices.push_back(current + 1);
            indices.push_back(next);
            indices.push_back(next + 1);
        }
    }

    m_sphereIndexCount = indices.size();

    // Create and bind VAO
    glGenVertexArrays(1, &m_sphereVAO);
    glBindVertexArray(m_sphereVAO);

    // Create and bind VBO
    glGenBuffers(1, &m_sphereVBO);
    glBindBuffer(GL_ARRAY_BUFFER, m_sphereVBO);
    glBufferData(GL_ARRAY_BUFFER, vertexData.size() * sizeof(float),
                 vertexData.data(), GL_STATIC_DRAW);

    // Create and bind EBO
    glGenBuffers(1, &m_sphereEBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_sphereEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, vertexData.size() * sizeof(unsigned int),
                 indices.data(), GL_STATIC_DRAW);

    // Set up vertex attributes (assuming same layout as cylinder)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *) 0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *) (3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void Trihedron::drawCylinder(const QMatrix4x4 &modelMatrix)
{
    m_shader->setUniformValue("uModel", modelMatrix);
    glBindVertexArray(m_cylinderVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, m_cylinderVertexCount);
    glBindVertexArray(0);
}

void Trihedron::drawCone(const QMatrix4x4 &modelMatrix)
{
    m_shader->setUniformValue("uModel", modelMatrix);
    glBindVertexArray(m_coneVAO);
    glDrawArrays(GL_TRIANGLE_FAN, 0, m_coneVertexCount);
    glBindVertexArray(0);
}

void Trihedron::drawSphere(const QMatrix4x4 &modelMatrix)
{
    m_shader->setUniformValue("uModel", modelMatrix);
    glBindVertexArray(m_sphereVAO);
    glDrawElements(GL_TRIANGLES, m_sphereIndexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void Trihedron::draw(const QMatrix4x4 &viewMatrix, const QMatrix4x4 &projectionMatrix, const float& scale)
{
    m_shader->bind();

    // Set view and projection matrices
    m_shader->setUniformValue("uView", viewMatrix);
    m_shader->setUniformValue("uProjection", projectionMatrix);

    QMatrix4x4 model;

    // Draw center sphere
    model.setToIdentity();
	model.scale(scale);
    m_shader->setUniformValue("uColor", QVector3D(1.0f, 1.0f, 1.0f)); // White color
    drawSphere(model);

    // Draw X-axis (Red)
    model.setToIdentity();
    model.rotate(90, 0, 1, 0); // Rotate to align with X-axis
    model.scale(scale);
    m_shader->setUniformValue("uColor", QVector3D(1.0f, 0.0f, 0.0f)); // Red color
    drawCylinder(model);

    model.setToIdentity();
    model.translate(1.0f * scale, 0.0f, 0.0f); // Move to cylinder tip
    model.rotate(90, 0, 1, 0); // Align cone along +X-axis
    model.scale(scale);
    drawCone(model);

    // Draw Y-axis (Green)
    model.setToIdentity();
    model.rotate(-90, 1, 0, 0); // Rotate to align with Y-axis
    model.scale(scale);
    m_shader->setUniformValue("uColor", QVector3D(0.0f, 0.75f, 0.0f)); // Green color
    drawCylinder(model);

    model.setToIdentity();
    model.translate(0.0f, 1.0f * scale, 0.0f); // Move to cylinder tip
    model.rotate(-90, 1, 0, 0); // Align cone along +Y-axis
    model.scale(scale);
    drawCone(model);

    // Draw Z-axis (Blue)
    model.setToIdentity();
    m_shader->setUniformValue("uColor", QVector3D(0.0f, 0.0f, 1.0f)); // Blue color
    model.scale(scale);
    drawCylinder(model);

    model.setToIdentity();
    model.translate(0.0f, 0.0f, 1.0f * scale); // Move to cylinder tip
    model.scale(scale);
    drawCone(model);
}
