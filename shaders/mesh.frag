#version 330 core
in vec3 fragNormal;
in vec3 fragPos;
in vec4 vColor;
in vec2 vTexCoord;  // ADD: Receive texture coordinates

uniform vec3 ambientColor;
uniform vec3 specularColor;
uniform float shininess;
uniform vec3 viewPos;
uniform vec3 lightDir;

uniform bool isSelected = false; // Selection state

// ADD: Texture uniforms
uniform sampler2D u_texture;
uniform bool u_hasTexture = false;

out vec4 fragColor;

void main() {
    // ADD: Determine base color (texture * vertex color or just vertex color)
    vec4 baseColor;
    if (u_hasTexture) {
        vec4 textureColor = texture(u_texture, vTexCoord);
        baseColor = textureColor * vColor;  // Multiply texture with vertex color
    } else {
        baseColor = vColor;  // Use vertex color only
    }

    vec3 norm = normalize(gl_FrontFacing ? fragNormal : -fragNormal);
    vec3 L = normalize(lightDir);
    vec3 V = normalize(viewPos - fragPos);
    
    // --- Enhanced ambient lighting ---
    // Base ambient (MODIFIED: use baseColor instead of vColor)
    vec3 ambient = ambientColor * baseColor.rgb;
    
    // Add subtle fill lighting from multiple directions for complex geometry
    vec3 fillLight1 = normalize(vec3(0.5, 1.0, 0.3));   // Soft top-right fill
    vec3 fillLight2 = normalize(vec3(-0.3, 0.2, 0.8));  // Back-side fill
    vec3 fillLight3 = normalize(vec3(0.2, -0.5, -0.4)); // Bottom fill
    
    float fill1 = max(dot(norm, fillLight1), 0.0) * 0.010;
    float fill2 = max(dot(norm, fillLight2), 0.0) * 0.008;
    float fill3 = max(dot(norm, fillLight3), 0.0) * 0.006;
    
    vec3 fillLighting = (fill1 + fill2 + fill3) * baseColor.rgb;  // MODIFIED: use baseColor
    
    // --- Primary diffuse lighting ---
    float diff = max(dot(norm, L), 0.0);
    
    // Add minimum diffuse to prevent complete darkness in crevices
    float minDiffuse = 0.08;
    diff = max(diff, minDiffuse);
    
    // Make diffuse contribution darker for more contrast (MODIFIED: use baseColor)
    vec3 diffuse = (diff * 0.75) * baseColor.rgb; // Reduced diffuse intensity
    
    // --- Fresnel rim lighting for shape definition ---
    float ndotv = max(dot(norm, V), 0.0);
    float fresnel = pow(1.0 - ndotv, 2.0);
    vec3 rimLight = fresnel * 0.05 * baseColor.rgb; // MODIFIED: use baseColor
    
    // --- Enhanced specular ---
    float spec = 0.0;
    if (diff > minDiffuse && ndotv > 0.05) { // Lowered threshold for better coverage
        vec3 H = normalize(L + V);
        float nh = max(dot(norm, H), 0.0);
        if (nh > 0.0) {
            spec = pow(nh, shininess) * 1.3; // Boost primary specular by 30%
            
            // Add stronger secondary specular highlight
            float secondarySpec = pow(nh, shininess * 0.3) * 0.5; // Increased from 0.3 to 0.5
            spec += secondarySpec;
        }
    }
    vec3 specular = specularColor * spec;
    
    // --- Combine all lighting with minimum brightness ---
    vec3 result = ambient + fillLighting + diffuse + rimLight + specular;
    
    // Ensure minimum brightness for complex geometry visibility (MODIFIED: use baseColor)
    result = max(result, baseColor.rgb * 0.15); // Keep minimum brightness
    
    // Gentler contrast enhancement without hue shifts
    float avgLuminance = dot(result, vec3(0.299, 0.587, 0.114));
    float contrastFactor = 1.15; // Moderate contrast boost
    result = (result - avgLuminance) * contrastFactor + avgLuminance;
    
    // Preserve original colors with subtle saturation
    vec3 desaturated = vec3(avgLuminance);
    result = mix(desaturated, result, 1.08); // Gentle 8% saturation boost
    
    // Simple edge contrast without color distortion
    result = result * (1.0 + fresnel * 0.06);
    
    fragColor = vec4(clamp(result, 0.0, 1.0), baseColor.a);  // MODIFIED: use baseColor alpha

    // --- Selection Highlight ---
    if (isSelected) {
        // Compute lighting
        vec3 norm = normalize(gl_FrontFacing ? fragNormal : -fragNormal);
        //vec3 lightDir = normalize(viewPos - fragPos);
        float diff = max(dot(norm, lightDir), 0.0);

        vec3 viewDir = normalize(viewPos - fragPos);
        vec3 reflectDir = reflect(-lightDir, norm);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);

        // Base color from fragColor
        vec3 baseColorSel = fragColor.rgb;

        // Lighten the color with lighting + subtle spec
        vec3 lightened = baseColorSel + vec3(0.3) * diff + vec3(0.2) * spec + vec3(0.1);
        lightened = clamp(lightened, 0.0, 1.0);

        // Apply a subtle transparency
        float alpha = fragColor.a * 0.99;

        // Add glow effect
        vec3 glowColor = lightened * 1.2; // Make the glow a bit brighter
        glowColor = clamp(glowColor, 0.0, 1.0);

        // Mix base color with the glow
        vec3 finalColor = mix(lightened, glowColor, 0.5); // blend base and glow color

        fragColor = vec4(finalColor, alpha);    
    }
}