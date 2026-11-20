#version 430 core
// The shadow layers' copy (shadow.cpp): one triangle that covers the viewport.
void main()
{
    vec2 pos = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
    gl_Position = vec4(pos * 2.0 - 1.0, 0.0, 1.0);
}
