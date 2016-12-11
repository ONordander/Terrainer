#version 410

layout(points) in;

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
