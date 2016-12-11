#version 410

layout(points) in;
layout(triangle_stip, max_vertices = 3) out;

void main()
{
  	for (int i = 0; i < gl_in.length(); i++) {
		if ( i > 0 && i % 3 == 0) {
			EndPrimitive();
		} else {
			gl_Position = gl_in[i].gl_Position;
			EmitVertex();
		}
  	}
}
