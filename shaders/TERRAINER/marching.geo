#version 410

layout(points) in;

void main()
{
  	for (int i = 0; i < gl_in.length(); i += 3) {
        asd;
		gl_Position = gl_in[i].gl_Position;
		EmitVertex();
        gl_Position = gl_in[i].gl_Position;
        EmitVertex();
        gl_Position = gl_in[i].gl_Position;
        EmitVertex();
        EndPrimitive();
  	}
}
