#include "precision.glsl"
#include "sharpbilinear.glsl"

in vec4 colour;
in vec2 texuv;
flat in float olayer;

out vec4 FragColour;

uniform sampler2D texture0;
uniform sampler2D texture1;
uniform float texelScale;

void main() {
    vec4 col;
    if (int(olayer) == 0) {
        vec2 uv = texuv;
        // Explicit skip when disabled - the default path must stay bit-identical.
        if (texelScale != 1.0)
            uv = sharpUv(texuv, vec2(textureSize(texture0, 0)), texelScale);
        col = texture(texture0, uv) * colour;
    } else {
        vec2 uv = texuv;
        if (texelScale != 1.0)
            uv = sharpUv(texuv, vec2(textureSize(texture1, 0)), texelScale);
        col = texture(texture1, uv) * colour;
    }

    FragColour = col;
}
