#version 410

layout(triangles) in;
layout(triangle_strip, max_vertices = 6) out;

void main()
{
	for (int i = 0; i < gl_in.length(); i++) {
		gl_Position = gl_in[i].gl_Position;
		EmitVertex();
	}
	EndPrimitive();
	for (int i = 0; i < gl_in.length(); i++) {
		gl_Position = gl_in[i].gl_Position + vec4(5.0f, 0.0f, 0.0f, 0.0f);
		EmitVertex();
	}
	EndPrimitive();
}
