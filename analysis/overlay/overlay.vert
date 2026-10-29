#version 450
layout(push_constant) uniform PC { vec4 rect; vec4 uv; vec4 color; } pc;
layout(location = 0) out vec2 vUV;
layout(location = 1) out vec4 vColor;
layout(location = 2) out vec2 vLocal;
void main() {
    const vec2 c[6] = vec2[](vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(0.0, 1.0), vec2(0.0, 1.0), vec2(1.0, 0.0), vec2(1.0, 1.0));
    vec2 t = c[gl_VertexIndex];
    gl_Position = vec4(mix(pc.rect.xy, pc.rect.zw, t), 0.0, 1.0);
    vUV = mix(pc.uv.xy, pc.uv.zw, t);
    vColor = pc.color;
    vLocal = t;
}
