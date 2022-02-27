#version 400
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (location = 0) out vec3 outColor;

void main() {
    int vertIdx = gl_VertexIndex;
    switch(vertIdx)
    {
        case 0:
            gl_Position = vec4(0.5, 0.0, 0.5, 0.0);
            outColor = vec3(1.0, 0.0, 0.0);
            break;
        case 1:
            gl_Position = vec4(1.0, 1.0, 0.5, 0.0);
            outColor = vec3(0.0, 1.0, 0.0);
            break;
        case 2:
            gl_Position = vec4(0.0, 1.0, 0.5, 0.0);
            outColor = vec3(0.0, 0.0, 1.0);
            break;
        default:
            return;
    }
}