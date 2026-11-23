#version 440
// Кадр без своей прозрачности (outline_cut.cpp, solid): старший байт пикселя не читается — у кадров wl_shm XRGB в нём
// что угодно (у Wine — ноль)
layout(location = 0) in vec2 vTexCoord;
layout(location = 1) in float vAlpha;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
};
layout(binding = 1) uniform sampler2D source;

void main() {
    fragColor = vec4(texture(source, vTexCoord).rgb, 1.0) * (vAlpha * qt_Opacity);
}
