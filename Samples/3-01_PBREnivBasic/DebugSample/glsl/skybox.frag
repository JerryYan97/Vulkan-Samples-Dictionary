#version 450

layout(binding = 0) uniform samplerCube i_cubeMapTexture;

// Be careful to Vulkan's UBO's struct && vec3 alignment.
layout(binding = 1) uniform CameraVecsUBO {
    vec3 view;
    vec3 right;
    vec3 up;
    vec3 heightWidthNear; // The height, width and distance of the camera's near plane -- |z| = 0.1.
} i_cameraVecs;

layout(location = 0) in vec2 i_screenUv;

layout(location = 0) out vec4 outColor;

void main() {
    float halfHeight = i_cameraVecs.heightWidthNear[0];
    float halfWidth  = i_cameraVecs.heightWidthNear[1];

    vec3 viewHoriVec = i_cameraVecs.right * i_screenUv[0];
    vec3 viewVertVec = i_cameraVecs.up    * i_screenUv[1];

    vec3 viewNearPlaneVec = viewHoriVec + viewVertVec;
    vec3 viewVec = viewNearPlaneVec + (i_cameraVecs.view * i_cameraVecs.heightWidthNear[2]);
    vec3 viewDir = normalize(viewVec);

    outColor = texture(i_cubeMapTexture, viewDir);
}