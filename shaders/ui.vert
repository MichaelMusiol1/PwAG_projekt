#version 330 core
// Shader wierzchołków interfejsu 2D - współrzędne w pikselach okna, (0,0) = lewy górny róg.

layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;

uniform vec2 uScreen;   // rozmiar okna w pikselach

out vec2 vUV;

void main()
{
    vUV = aUV;
    vec2 ndc = aPos / uScreen * 2.0 - 1.0;
    gl_Position = vec4(ndc.x, -ndc.y, 0.0, 1.0);
}
