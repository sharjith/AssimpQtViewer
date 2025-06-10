#version 330 core

layout(location = 0) in vec3 aPos; // Vertex position
layout(location = 1) in vec3 aNormal; // Vertex position

uniform mat4 uModel;      // Model matrix
uniform mat4 uView;       // View matrix
uniform mat4 uProjection; // Projection matrix

out vec3 vNormal;
out vec3 vFragPos; // Fragment position in world space
out vec3 vPosition;

void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    vPosition = worldPos.xyz;
    vFragPos = vec3(uModel * vec4(aPos, 1.0));
    vNormal = mat3(transpose(inverse(uModel))) * aNormal; // Normal transformation
    gl_Position = uProjection * uView * worldPos;
}