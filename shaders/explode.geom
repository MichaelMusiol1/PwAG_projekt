#version 330 core
// Rozpad sprite'a na cząsteczki (śmierć potwora). Na podstawie shadera geometrii z lab6
// (subdiv.geom): każdy trójkąt jest parametryzowany (u, v) i zamieniany na siatkę punktów,
// które odlatują od środka w czasie. Kolor punktu = piksel tekstury w jego miejscu.

layout(triangles) in;
layout(points, max_vertices = 66) out;   // LEVEL 10 -> 11*12/2 = 66 punktów (limit 1024 komponentów)

in VertexData {
    vec3 worldPos;
    vec3 normal;
    vec2 uv;
} gin[];

out VertexData {
    vec3 worldPos;
    vec3 normal;
    vec2 uv;
} gout;

uniform mat4 uView;
uniform mat4 uProj;
uniform float uExplodeT;        // 0 = początek, 1 = koniec
uniform vec3 uExplodeOrigin;    // środek sprite'a

const int LEVEL = 10;

void main()
{
    vec3 center = (gin[0].worldPos + gin[1].worldPos + gin[2].worldPos) / 3.0;
    float t = uExplodeT;
    float step = 1.0 / float(LEVEL);

    for (int i = 0; i <= LEVEL; ++i) {
        for (int j = 0; j <= LEVEL - i; ++j) {
            float u = float(i) * step;
            float v = float(j) * step;
            float w = 1.0 - u - v;
            vec3 P = u * gin[0].worldPos + v * gin[1].worldPos + w * gin[2].worldPos;

            // prędkość: od środka sprite'a + od środka trójkąta (jak w lab6), do tego grawitacja
            vec3 velocity = normalize(P - uExplodeOrigin + vec3(0.0, 0.001, 0.0)) * 2.0
                          + normalize(P - center + vec3(0.001)) * 0.6;
            vec3 pos = P + velocity * t + vec3(0.0, 1.5 * t - 3.0 * t * t, 0.0);

            gout.worldPos = pos;
            gout.normal = gin[0].normal;
            gout.uv = u * gin[0].uv + v * gin[1].uv + w * gin[2].uv;
            gl_Position = uProj * uView * vec4(pos, 1.0);
            gl_PointSize = max(1.0, 40.0 * (1.0 - t) / gl_Position.w);
            EmitVertex();
        }
    }
    EndPrimitive();
}
