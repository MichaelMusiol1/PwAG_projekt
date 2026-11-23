#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;
uniform vec4 uUVRect;   // przesunięcie (xy) i skala (zw) UV - tak wybierana jest klatka sprite'a

out VertexData {
    vec3 worldPos;
    vec3 normal;
    vec2 uv;
} vout;

void main()
{
    vec4 world = uModel * vec4(aPos, 1.0);
    vout.worldPos = world.xyz;
    // inverse() dla każdego wierzchołka. Powinno przychodzić gotowe z CPU, ale karta jakoś to znosi.
    vout.normal = mat3(transpose(inverse(uModel))) * aNormal;
    vout.uv = uUVRect.xy + aUV * uUVRect.zw;
    gl_Position = uProj * uView * world;
}
