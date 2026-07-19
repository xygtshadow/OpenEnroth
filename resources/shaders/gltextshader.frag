#include "precision.glsl"

in vec4 colour;
in vec2 texuv;
flat in float olayer;

out vec4 FragColour;

uniform sampler2D texture0;
uniform sampler2D texture1;
uniform float u_texelScale;

// Sharp-bilinear: remaps UVs so that texel interiors sample at a single point (crisp under
// magnification) while texel-boundary transitions keep a one-device-pixel bilinear ramp.
// Requires a LINEAR sampler; texelScale is device pixels per texel (1.0 = disabled).
vec2 sharpUv(vec2 uv, vec2 texSize, float texelScale) {
    vec2 px = uv * texSize;
    vec2 fl = floor(px - 0.5) + 0.5;
    vec2 fr = clamp((px - fl) * texelScale - (texelScale - 1.0) * 0.5, 0.0, 1.0);
    return (fl + fr) / texSize;
}

void main() {
    vec4 col;
    if (int(olayer) == 0) {
        vec2 uv = texuv;
        // Explicit skip when disabled - the default path must stay bit-identical.
        if (u_texelScale != 1.0)
            uv = sharpUv(texuv, vec2(textureSize(texture0, 0)), u_texelScale);
        col = texture(texture0, uv) * colour;
    } else {
        vec2 uv = texuv;
        if (u_texelScale != 1.0)
            uv = sharpUv(texuv, vec2(textureSize(texture1, 0)), u_texelScale);
        col = texture(texture1, uv) * colour;
    }

    FragColour = col;
}
