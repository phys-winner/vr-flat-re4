#version 450
layout(set = 0, binding = 0) uniform sampler2D atlas;
layout(location = 0) in vec2 vUV;
layout(location = 1) in vec4 vColor;
layout(location = 2) in vec2 vLocal;
layout(location = 0) out vec4 outColor;
void main() {
    if (vColor.a > 1.5) {
        outColor = vec4(texture(atlas, vUV).rgb, 1.0);
        return;
    }
    if (vColor.a >= 0.0) {
        outColor = texture(atlas, vUV) * vColor;
        return;
    }
    if (vColor.a < -1.5) {
        // Progress ring (hold-to-skip, crank): red = progress 0..1, clockwise from the top; no atlas sample.
        vec2 q = vLocal * 2.0 - 1.0;
        float r = length(q);
        float e = max(fwidth(r), 0.001);
        float ring = smoothstep(0.76 - e, 0.76 + e, r) * (1.0 - smoothstep(0.96 - e, 0.96 + e, r));
        const float tau = 6.28318530718;
        float turn = mod(atan(q.x, -q.y) + tau, tau) / tau;
        float te = max(fwidth(turn), 0.0001);
        float done = 1.0 - smoothstep(vColor.r - te, vColor.r + te, turn);
        outColor = vec4(mix(vec3(0.20, 0.22, 0.22), vec3(1.0), done), ring * mix(0.40, 0.95, done));
        return;
    }
    // Health carries current/max in red and the original integer-third status (0/1/2) in green.
    vec2 p = vLocal * 2.0 - 1.0;
    float radius = length(p);
    float edge = max(fwidth(radius), 0.001);
    float disc = 1.0 - smoothstep(0.86 - edge, 0.86 + edge, radius);
    float band = smoothstep(0.70 - edge, 0.70 + edge, radius)
        * (1.0 - smoothstep(0.82 - edge, 0.82 + edge, radius));
    const float pi = 3.14159265359;
    float theta = mod(atan(p.y, p.x + 0.000001) + pi / 3.0 + 2.0 * pi, 2.0 * pi);
    float progress = theta / (5.0 * pi / 3.0);
    float angularEdge = max(fwidth(progress), 0.0001);
    float arc = 1.0 - smoothstep(1.0 - angularEdge, 1.0 + angularEdge, progress);
    float segment = fract(progress * 20.0);
    float segmentEdge = min(angularEdge * 20.0, 0.035);
    float segments = smoothstep(0.05 - segmentEdge, 0.05 + segmentEdge, segment)
        * (1.0 - smoothstep(0.95 - segmentEdge, 0.95 + segmentEdge, segment));
    float hp = clamp(vColor.r, 0.0, 1.0);
    float filled = hp <= 0.0 ? 0.0 : hp >= 1.0 ? 1.0
        : 1.0 - smoothstep(hp - angularEdge, hp + angularEdge, progress);
    vec3 health = vColor.g < 0.5 ? vec3(0.92, 0.16, 0.12)
        : vColor.g < 1.5 ? vec3(0.94, 0.76, 0.12) : vec3(0.32, 0.88, 0.23);
    vec3 color = mix(vec3(0.035, 0.075, 0.082), mix(vec3(0.18, 0.22, 0.22), health, filled), band * arc * segments);
    vec4 frame = texture(atlas, vUV);
    outColor = vec4(mix(color, frame.rgb, frame.a), max(disc * 0.92, frame.a));
}
