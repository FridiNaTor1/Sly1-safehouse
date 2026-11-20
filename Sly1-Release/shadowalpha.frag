#version 430 core
// The shadow layers' copy (shadow.cpp): texelFetch reads each staging texel unfiltered, so every layer texel is the
// bit-exact 8-bit alpha. sourceLayer reads texture unit 0.
uniform sampler2DArray sourceLayer;
layout(location = 0) out vec4 FragColor;
void main()
{
    FragColor = vec4(texelFetch(sourceLayer, ivec3(gl_FragCoord.xy, 0), 0).a);
}
