#version 330 core

layout(location = 0) in vec2 aPos;  // Input vertex position
layout(location = 1) in vec3 aColor; // Input vertex color

out vec3 vColor; // Pass color to the fragment shader

void main() {
    vColor = aColor; // Pass input color to the fragment shader
    gl_Position = vec4(aPos, 0.0, 1.0); // Transform to clip space
}
