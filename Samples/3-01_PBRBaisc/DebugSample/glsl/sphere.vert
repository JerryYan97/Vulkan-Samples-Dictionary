#version 450

layout(binding = 0) uniform Matices {
    mat4 vpMat;
} i_matrices;

layout(location = 0) in vec3 i_pos;
layout(location = 1) in vec3 i_normal;

layout(location = 0) out vec3 o_worldPos;
layout(location = 1) out vec3 o_worldNormal;

vec3 g_sphereWorldPos[4] = {
    vec3(3.0, 0.0, 0.0),
    vec3(8.0, 0.0, 0.0),
    vec3(10.0, 0.0, 0.0),
    vec3(12.0, 0.0, 0.0)
};

mat4 PosToModelMat(vec3 pos)
{
    // NOTE: GLSL's matrices are column major but we fill it as row major. Thus, we need to transpose it at the end.
    mat4 mat = { vec4(1.0, 0.0, 0.0, pos.x),
                 vec4(0.0, 1.0, 0.0, pos.y),
                 vec4(0.0, 0.0, 1.0, pos.z),
                 vec4(0.0, 0.0, 0.0, 1.0) };
    mat4 transMat = transpose(mat);
    return transMat;
}

void main()
{
    mat4 modelMat = PosToModelMat(g_sphereWorldPos[gl_InstanceIndex]);

    vec4 worldPos = modelMat * vec4(i_pos, 1.0);
    vec4 worldNormal = modelMat * vec4(i_normal, 0.0);

    o_worldPos = worldPos.xyz;
    o_worldNormal = normalize(worldNormal.xyz);

    gl_Position = i_matrices.vpMat * modelMat * vec4(i_pos, 1.0);
}