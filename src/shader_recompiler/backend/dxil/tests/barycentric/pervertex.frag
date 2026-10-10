#version 450
#extension GL_EXT_fragment_shader_barycentric : require
layout(location=0) pervertexEXT in vec4 value[];
layout(location=0) out vec4 color;
void main(){color=value[2];}
