#include "precision.glsl"

in vec4 colour;
in vec2 texuv;
flat in int paletteid;

out vec4 FragColour;

uniform sampler2D texture0;
uniform sampler2D paltex2D;
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
    vec2 uv = texuv;
    // Explicit skip when disabled - the default path must stay bit-identical. Palette-indexed
    // textures sample NEAREST and their indices don't interpolate - leave them untouched.
    if (u_texelScale != 1.0 && paletteid == 0)
        uv = sharpUv(texuv, vec2(textureSize(texture0, 0)), u_texelScale);

    vec4 fragcol = texture(texture0, uv);
    int index = int(fragcol.r * 255.0);
    vec4 newcol = vec4(texelFetch(paltex2D, ivec2(index, paletteid), 0));

    if (paletteid > 0)
        if (index > 0)
            fragcol = vec4(newcol.r, newcol.g, newcol.b, 1.0);

    FragColour =  fragcol * colour;
}
