#version 410

layout(points) in;
layout(triangle_strip, max_vertices = 1) out;

void main()
{
  	for (int i = 0; i < gl_in.length(); i += 3) {
		gl_Position = gl_in[i].gl_Position + i / 3;
		EmitVertex();
        gl_Position = gl_in[i].gl_Position + i / 2;
        EmitVertex();
        gl_Position = gl_in[i].gl_Position + i;
        EmitVertex();
        EndPrimitive();
  	}
}