// Sharp-bilinear UV remapping, shared by the 2D quad and text shaders.

// Remaps UVs so that texel interiors sample at a single point (crisp under magnification) while
// texel-boundary transitions keep a one-device-pixel bilinear ramp. Requires a LINEAR sampler;
// scale is device pixels per texel (1.0 = disabled).
vec2 sharpUv(vec2 uv, vec2 texSize, float scale) {
    vec2 px = uv * texSize;
    vec2 fl = floor(px - 0.5) + 0.5;
    vec2 fr = clamp((px - fl) * scale - (scale - 1.0) * 0.5, 0.0, 1.0);
    return (fl + fr) / texSize;
}
