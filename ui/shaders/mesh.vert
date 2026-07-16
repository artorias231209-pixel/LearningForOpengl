#version 450

layout(binding = 0) uniform GlobalUniform {
    mat4 viewProjection;
} globalData;

layout(push_constant) uniform DrawConstants {
    mat4 model;
    vec4 color;
} drawData;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;

layout(location = 0) out vec3 worldNormal;
layout(location = 1) out vec3 baseColor;

void main() {
    worldNormal = mat3(drawData.model) * inNormal;
    baseColor = drawData.color.rgb;
    gl_Position = globalData.viewProjection * drawData.model * vec4(inPosition, 1.0);
}
