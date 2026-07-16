#version 450

layout(location = 0) in vec3 worldNormal;
layout(location = 1) in vec3 baseColor;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 normal = normalize(worldNormal);
    vec3 light = normalize(vec3(0.35, 0.85, 0.55));
    float diffuse = max(dot(normal, light), 0.20);
    outColor = vec4(baseColor * (0.28 + diffuse), 1.0);
}
