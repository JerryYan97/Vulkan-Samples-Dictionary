#version 450

// We'll use gl_fragCoord after we can load the hdri.
layout(binding = 0) uniform sampler2D i_hdriSampler;
// layout(binding = 1) uniform vec3 i_viewDir;

layout(location = 0) in vec2 i_uv;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 hdriVals = texture(i_hdriSampler, i_uv);
    outColor = vec4(hdriVals.x, hdriVals.y, hdriVals.z, 1.0);
    // outColor = vec4(i_uv, 0.0, 1.0);
}