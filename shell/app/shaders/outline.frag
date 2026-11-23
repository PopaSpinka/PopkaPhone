#version 440
layout(location = 0) in vec2 vTexCoord;
layout(location = 1) in float vAlpha;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
};
layout(binding = 1) uniform sampler2D source;

void main() {
    fragColor = texture(source, vTexCoord) * (vAlpha * qt_Opacity);
}
