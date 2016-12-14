#version 410

layout(points) in;
layout(points, max_vertices = 15) out;

void main() {
	gl_Position = gl_in[0].gl_Position;
}
