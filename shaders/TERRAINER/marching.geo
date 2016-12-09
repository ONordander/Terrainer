#version 410

layout(triangles) in;
layout(triangle_strip, max_vertices = 8) out;
 
void main()
{
  for(int i = 0; i < gl_in.length(); i++)
  {
    gl_Position = gl_in[i].gl_Position + i;
    EmitVertex();
  }
  EndPrimitive();
}