#version 450

layout(location = 0) out vec4 fragColor;

layout(set=0, binding=0) uniform sampler2D renderTarget;

void main() {
    vec2 uv = gl_FragCoord.xy / vec2(1280.0, 768.0);

    fragColor = pow(texture(renderTarget, uv), vec4(1.0 / 2.2));
}