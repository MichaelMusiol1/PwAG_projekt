#version 330 core
// Shader wierzchołków sceny 3D (modele, teren, sprite'y, niebo, woda).

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;
uniform vec4 uUVRect;   // przesunięcie (xy) i skala (zw) UV - wybór klatki sprite'a

out VertexData {
    vec3 worldPos;
    vec3 normal;
    vec2 uv;
} vout;

void main()
{
    vec4 world = uModel * vec4(aPos, 1.0);
    vout.worldPos = world.xyz;
    vout.normal = mat3(transpose(inverse(uModel))) * aNormal;
    vout.uv = uUVRect.xy + aUV * uUVRect.zw;
    gl_Position = uProj * uView * world;
}
