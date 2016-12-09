#version 410

layout(triangles) in;
layout(triangle_strip, max_vertices = 8) out;
/*
in vertex_data {
    vec2 tex_coord;
    vec3 normal;
} gl_in[];
*/ 
/*
out vertex_data {
    vec2 tex_coord;
    vec3 normal;
};
*/
 
void main()
{
  for(int i = 0; i < gl_in.length(); i++)
  {
    gl_Position = vec4(1.0f);
    EmitVertex();
  }
  EndPrimitive();
 
  for(int i = 0; i < gl_in.length(); i++)
  {
    EmitVertex();
  }
  EndPrimitive();
}