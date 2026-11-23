#version 440
// Кадр видео «Просмотра» (PreviewVideo, кадры NV12 декодера): яркость — текстура R8, цвет (U, V) — RG8 вдвое
// меньше; RGB считает матрица colorMatrix (YUV → RGB по colorimetry кадра: BT.709 / 601 / 2020, диапазон).
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    mat4 colorMatrix;
};
layout(binding = 1) uniform sampler2D luma;
layout(binding = 2) uniform sampler2D chroma;

void main() {
    vec4 yuv = vec4(texture(luma, qt_TexCoord0).r, texture(chroma, qt_TexCoord0).rg, 1.0);
    fragColor = vec4(clamp((colorMatrix * yuv).rgb, 0.0, 1.0), 1.0) * qt_Opacity;
}
