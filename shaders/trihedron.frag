#version 330 core
in vec3 vColor; // Interpolated color from the vertex shader
in vec3 vNormal; // Interpolated normal from vertex shader
in vec3 vPosition; // Position in world space
in vec3 vFragPos; // Position in view space
out vec4 FragColor; // Output color

uniform vec3 uColor; // Base color of the object

// Static light parameters
const vec3 lightColor = vec3(1.0); // Light color

void main() {
    // Normalize the normal
    vec3 normal = normalize(vNormal);
    vec3 viewDir = normalize(-vFragPos);  // From fragment to camera
    
    // Primary light from camera direction
    vec3 lightDir = viewDir;
    
    // --- Enhanced Ambient for intersections ---
    // Calculate distance from origin to boost ambient near intersection
    float distFromOrigin = length(vPosition);
    float intersectionBoost = smoothstep(2.0, 0.5, distFromOrigin); // Adjust range as needed
    
    float ambientStrength = 0.4 + intersectionBoost * 0.3; // Boost ambient near origin
    vec3 ambient = ambientStrength * lightColor;
    
    // --- Multi-directional lighting ---
    // Primary diffuse from camera
    float diff1 = max(dot(normal, lightDir), 0.0);
    
    // Add implicit lights from major axes to illuminate intersections
    vec3 lightDirX = normalize(vec3(1.0, 0.0, 0.0));
    vec3 lightDirY = normalize(vec3(0.0, 1.0, 0.0));
    vec3 lightDirZ = normalize(vec3(0.0, 0.0, 1.0));
    
    float diffX = max(dot(normal, lightDirX), 0.0) * 0.3;
    float diffY = max(dot(normal, lightDirY), 0.0) * 0.3;
    float diffZ = max(dot(normal, lightDirZ), 0.0) * 0.3;
    
    // Combine diffuse contributions with minimum threshold
    float totalDiff = max(diff1 + diffX + diffY + diffZ, 0.2); // Never go below 20%
    vec3 diffuse = totalDiff * lightColor;
    
    // --- Fresnel-like rim lighting ---
    float fresnel = pow(1.0 - max(dot(normal, viewDir), 0.0), 1.5);
    vec3 rimLight = fresnel * 0.2 * lightColor;
    
    // --- Specular (fixed reflection direction) ---
    float specularStrength = 0.5;
    vec3 reflectDir = reflect(-lightDir, normal); // Fixed: negate lightDir
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    vec3 specular = specularStrength * spec * lightColor;
    
    // --- Combine all lighting ---
    vec3 lighting = ambient + diffuse + rimLight + specular;
    
    // Apply color and ensure minimum brightness
    vec3 finalColor = uColor * max(lighting, vec3(0.3)); // Never darker than 30%
    
    FragColor = vec4(finalColor, 1.0);
}