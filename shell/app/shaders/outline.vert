#version 440
// Сглаженный край поверхности, обрезанной по контуру окна (outline_cut.cpp): кайма в пиксель, у которой внутренние
// вершины непрозрачны, внешние — прозрачны; между ними видимость растёт ровно.
layout(location = 0) in vec4 position;
layout(location = 1) in vec2 texCoord;
layout(location = 2) in float alpha;
layout(location = 0) out vec2 vTexCoord;
layout(location = 1) out float vAlpha;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
};

void main() {
    vTexCoord = texCoord;
    vAlpha = alpha;
    gl_Position = qt_Matrix * position;
}
