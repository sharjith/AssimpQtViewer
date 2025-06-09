#version 330 core

in vec3 fragNormal;
in vec3 fragPos;

in vec4 vColor;

uniform vec3 ambientColor;
uniform vec3 specularColor;

uniform float shininess;
uniform vec3 viewPos;
uniform vec3 lightDir;
out vec4 fragColor;

void main() {
    vec3 norm = normalize(fragNormal);
    vec3 L = normalize(lightDir);
    vec3 V = normalize(viewPos -fragPos);
    float ndotv = max(dot(norm, V), 0.0);
    float diff = max(dot(norm, L), 0.0);
    vec3 ambient = ambientColor * vColor.rgb;
    vec3 diffuse = diff * vColor.rgb;
    float spec = 0.0;
    if (diff > 0.0 && ndotv > 0.1) {
        vec3 H = normalize(L + V);
        float nh = max(dot(norm, H), 0.0);
        if (nh > 0.0)
            spec = pow(nh, shininess);
    }
    vec3 specular = specularColor * spec;
    vec3 result = ambient + diffuse + specular;
    fragColor = vec4(clamp(result, 0.0, 1.0), vColor.a);
};
