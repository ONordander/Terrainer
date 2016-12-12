#version 410

layout(points) in;
layout(points, max_vertices = 80) out;

void main()
{
	gl_Position = gl_in[0].gl_Position;
	EmitVertex();
    EndPrimitive();
}
