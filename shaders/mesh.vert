#version 450 core

layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec3 vertexNormal;
layout(location = 2) in vec2 vertexTexCoord;
layout(location = 3) in vec4 vertexColor;
layout (location = 4) in vec3 tangent;     
layout (location = 5) in vec3 bitangent;   




out vec4 vColor;
out vec2 vTexCoord;  
out vec3 fragNormal;
out vec3 fragPos;
out vec3 fragTangent;
out vec3 fragBitangent;

uniform mat4 mvp;
uniform mat4 model;
uniform mat4 view;

void main() {
    vec4 viewPos4 = view * model * vec4(vertexPosition, 1.0);
    fragPos = viewPos4.xyz;
    mat3 normalMatrix = transpose(inverse(mat3(view * model)));
    fragNormal = normalize(normalMatrix * vertexNormal);
      // Transform tangent space vectors to world space
    fragTangent = normalize(mat3(model) * tangent);
    fragBitangent = normalize(mat3(model) * bitangent);
    vColor = vertexColor;
    vTexCoord = vertexTexCoord;  // ADD: Pass texture coordinates
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}