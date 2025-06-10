#version 330 core

in vec3 vColor; // Interpolated color from the vertex shader
out vec4 FragColor; // Output color

void main() {
    FragColor = vec4(vColor, 1.0); // Set the fragment color
}