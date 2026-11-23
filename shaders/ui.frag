#version 330 core
// Shader fragmentów interfejsu 2D: tekstura (ikona, litera czcionki lub biały piksel) * kolor.

in vec2 vUV;
out vec4 fragColor;

uniform sampler2D uTex;
uniform vec4 uColor;

void main()
{
    fragColor = texture(uTex, vUV) * uColor;
    if (fragColor.a < 0.01)
        discard;
}
