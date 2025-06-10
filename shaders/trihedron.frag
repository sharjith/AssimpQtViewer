#version 330 core

in vec3 vColor; // Interpolated color from the vertex shader
in vec3 vNormal; // Interpolated normal from vertex shader (you need to pass this in)
in vec3 vPosition; // Position in world space or view space
in vec3 vFragPos; // Position in view space

out vec4 FragColor; // Output color

uniform vec3 uColor; // Base color of the object
uniform vec3 viewDir; // Light coming from this direction

// Static light parameters
vec3 ambientColor = uColor * vec3(0.8); // Ambient light contribution
const vec3 lightColor = vec3(1.0); // Diffuse light color

void main() {
    
    // Normalize the normal
    vec3 normal = normalize(vNormal);

    vec3 viewDir = normalize(-vFragPos);  // From fragment to camera

    // Light comes from the camera direction
    vec3 lightDir = viewDir;

    // --- Ambient ---
    float ambientStrength = 0.2;
    vec3 ambient = ambientStrength * lightColor;

    // Compute diffuse lighting
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    // --- Specular ---
    float specularStrength = 0.5;
    vec3 reflectDir = reflect(lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    vec3 specular = specularStrength * spec * lightColor;

    // --- Combine lighting ---
    vec3 lighting = (ambient + diffuse + specular) * uColor;

    // Final color
    vec3 finalColor = uColor * lighting;

    FragColor = vec4(lighting, 1.0);
}
