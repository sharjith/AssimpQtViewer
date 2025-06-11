#version 330 core

layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec3 vertexNormal;
layout(location = 2) in vec2 vertexTexCoord;
layout(location = 3) in vec4 vertexColor;

out vec4 vColor;

uniform mat4 mvp;
uniform mat4 model;
uniform mat4 view;

out vec3 fragNormal;
out vec3 fragPos;

void main() {
    vec4 viewPos4 = view * model * vec4(vertexPosition, 1.0);
    fragPos = viewPos4.xyz;
    mat3 normalMatrix = transpose(inverse(mat3(view * model)));
    fragNormal = normalize(normalMatrix * vertexNormal);
    vColor = vertexColor;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
};