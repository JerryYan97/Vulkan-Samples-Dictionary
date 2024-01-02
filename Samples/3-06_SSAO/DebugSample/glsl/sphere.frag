#version 450

// We'll use gl_fragCoord after we can load the hdri.
layout(binding = 0) uniform sampler2D i_hdriSampler;
// layout(binding = 1) uniform vec3 i_viewDir;

layout(location = 0) in vec2 i_uv;
layout(location = 1) in vec2 i_viewXy;

layout(location = 0) out vec4 outColor;

float M_PI = 3.1415926;

void main() {
    vec3 view_pos = vec3(i_viewXy, 0.1);
    view_pos = normalize(view_pos);
    
    vec3 d = -view_pos;

    float u = 0.5 + (atan(d.z, d.x) / (2.0 * M_PI));
    float v = 0.5 + (asin(d.y) / M_PI);

    vec2 uv = vec2(u, v);
    // vec4 hdriVals = texture(i_hdriSampler, uv);
    // outColor = vec4(hdriVals.x, hdriVals.y, hdriVals.z, 1.0);

    // outColor = vec4(view_pos, 1.0);
    vec4 hdriVals = texture(i_hdriSampler, i_uv);
    outColor = vec4(hdriVals.x, hdriVals.y, hdriVals.z, 1.0);
    // outColor = vec4(i_uv, 0.0, 1.0);
}