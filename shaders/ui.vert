#version 330 core

layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;

uniform vec2 uScreen;

out vec2 vUV;

void main()
{
    vUV = aUV;
    vec2 ndc = aPos / uScreen * 2.0 - 1.0;
    gl_Position = vec4(ndc.x, -ndc.y, 0.0, 1.0);   // w oknie y rośnie w dół, w OpenGL w górę, bo przecież oczywiście że tak
}
