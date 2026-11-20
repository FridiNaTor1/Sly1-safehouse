#version 430 core

layout (location = 0) in vec2 a_position;
layout (location = 1) in vec2 a_texcoord;
layout (location = 2) in vec4 a_color;

uniform mat4 u_model;
uniform mat4 u_projection;
uniform vec4 u_uvRect;
uniform int  u_useVertexColor;
// Points' size (tv.cpp's flecks): what glPointSize sets for OpenGL, which ignores this while GL_PROGRAM_POINT_SIZE is
// off; a renderer without glPointSize (Vulkan) reads it here.
uniform float u_pointSize;

layout(location = 0) out vec2 v_texcoord;
layout(location = 1) out vec4 v_color;

void main()
{
    v_texcoord = mix(u_uvRect.xy, u_uvRect.zw, a_texcoord);

    if (u_useVertexColor != 0)
        v_color = a_color;
    else
        v_color = vec4(1.0);

    gl_Position = u_projection * u_model * vec4(a_position, 0.0, 1.0);
    gl_PointSize = u_pointSize;
}
