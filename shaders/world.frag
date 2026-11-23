#version 330 core
// Wzory z lab4 (perVertexLightingSimple.vs) przeniesione z wierzchołków do fragmentów -
// podłoga krypty to kwadraty 3 x 3 m, na których światło pochodni z wierzchołków byłoby kanciaste.

in VertexData {
    vec3 worldPos;
    vec3 normal;
    vec2 uv;
} fin;

out vec4 fragColor;

// MODE_* i MAX_LIGHTS muszą się zgadzać z render.cpp i render.h
const int MODE_MODEL   = 0;
const int MODE_TERRAIN = 1;
const int MODE_SPRITE  = 2;   // oświetlenie "owinięte" - sprite nie robi się czarny, gdy słońce jest za nim
const int MODE_UNLIT   = 3;
const int MODE_SKY     = 4;
const int MODE_WATER   = 5;   // falują tylko normalne, geometria stoi (YOINK pomysł z lab4)
const int MAX_LIGHTS   = 16;

uniform int uMode;
uniform sampler2D uTex0;   // teren: splatmapa
uniform sampler2D uTex1;   // teren: warstwy
uniform sampler2D uTex2;
uniform sampler2D uTex3;
uniform sampler2D uTex4;
uniform float uTiling;
uniform vec3 uTint;
uniform float uSpecular;
uniform float uEmissive;

uniform vec3 uCamPos;
uniform vec3 uSunDir;
uniform vec3 uSunColor;
uniform vec3 uAmbient;
uniform int uLightCount;
uniform vec3 uLightPos[MAX_LIGHTS];
uniform vec3 uLightColor[MAX_LIGHTS];
uniform float uLightRange[MAX_LIGHTS];
uniform vec3 uFogColor;
uniform float uFogDensity;
uniform float uTime;

vec3 lighting(vec3 albedo, vec3 N, float specStrength, bool wrap)
{
    vec3 V = normalize(uCamPos - fin.worldPos);
    vec3 color = uAmbient * albedo;

    vec3 L = normalize(-uSunDir);
    float ndl = wrap ? dot(N, L) * 0.5 + 0.5 : max(dot(N, L), 0.0);
    vec3 H = normalize(L + V);
    float spec = ndl > 0.0 ? pow(max(dot(N, H), 0.0), 32.0) * specStrength : 0.0;
    color += uSunColor * (albedo * ndl + vec3(spec));

    for (int i = 0; i < uLightCount; ++i) {
        vec3 d = uLightPos[i] - fin.worldPos;
        float dist = length(d);
        L = d / max(dist, 0.0001);
        float x = dist / uLightRange[i];
        // łagodnie do zera na granicy zasięgu, bez ostrej krawędzi plamy światła
        float att = clamp(1.0 - x * x, 0.0, 1.0);
        att *= att;
        ndl = wrap ? dot(N, L) * 0.5 + 0.5 : max(dot(N, L), 0.0);
        H = normalize(L + V);
        spec = ndl > 0.0 ? pow(max(dot(N, H), 0.0), 32.0) * specStrength : 0.0;
        color += uLightColor[i] * att * (albedo * ndl + vec3(spec));
    }
    return color;
}

vec3 applyFog(vec3 color)
{
    float dist = length(uCamPos - fin.worldPos);
    float fog = clamp(1.0 - exp(-dist * uFogDensity), 0.0, 1.0);
    return mix(color, uFogColor, fog);
}

void main()
{
    vec3 N = normalize(fin.normal);

    if (uMode == MODE_TERRAIN) {
        // R, G, B = udział warstw 2..4, reszta to warstwa 1
        vec3 w = texture(uTex0, fin.uv).rgb;
        vec2 tuv = fin.uv * uTiling;
        float base = clamp(1.0 - (w.r + w.g + w.b), 0.0, 1.0);
        vec3 albedo = texture(uTex1, tuv).rgb * base
                    + texture(uTex2, tuv).rgb * w.r
                    + texture(uTex3, tuv).rgb * w.g
                    + texture(uTex4, tuv).rgb * w.b;
        fragColor = vec4(applyFog(lighting(albedo, N, 0.05, false)), 1.0);
        return;
    }

    if (uMode == MODE_SKY) {
        vec3 sky = texture(uTex0, fin.uv).rgb * uTint;
        float up = normalize(fin.worldPos - uCamPos).y;
        fragColor = vec4(mix(uFogColor, sky, smoothstep(-0.02, 0.35, up)), 1.0);
        return;
    }

    if (uMode == MODE_WATER) {
        vec2 p = fin.worldPos.xz;
        vec3 wn = normalize(vec3(0.10 * cos(p.x * 1.3 + uTime * 1.7) + 0.06 * cos((p.x + p.y) * 2.1 + uTime * 2.3),
                                 1.0,
                                 0.10 * sin(p.y * 1.1 + uTime * 1.3) + 0.06 * sin((p.x - p.y) * 1.7 + uTime * 1.9)));
        vec3 V = normalize(uCamPos - fin.worldPos);
        float fresnel = pow(1.0 - max(dot(wn, V), 0.0), 3.0);
        vec3 color = lighting(vec3(0.04, 0.15, 0.21), wn, 0.6, false);
        color = mix(color, uFogColor * 0.8, fresnel * 0.5);   // odbicie nieba pod małym kątem
        fragColor = vec4(applyFog(color), mix(0.75, 0.92, fresnel));
        return;
    }

    vec4 texel = texture(uTex0, fin.uv);
    if (texel.a < 0.5)
        discard;   // magenta z BMP; discard jak w lab5, bo bez sortowania blending by się sypał
    vec3 albedo = texel.rgb * uTint;

    if (uMode == MODE_UNLIT) {
        fragColor = vec4(albedo, 1.0);
        return;
    }
    vec3 color = lighting(albedo, N, uSpecular, uMode == MODE_SPRITE) + albedo * uEmissive;
    fragColor = vec4(applyFog(color), 1.0);
}
