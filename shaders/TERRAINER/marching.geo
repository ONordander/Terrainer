#version 410

layout(points) in;
layout(triangle_strip, max_vertices = 3) out;

uniform sampler2D cube_tex;

void main()
{
	gl_Position = gl_in[0].gl_Position;
	EmitVertex();

    gl_Position = gl_in[0].gl_Position + vec4(2.0f, 0.0f, 0.0f, 0.0f);
    EmitVertex();

    gl_Position = gl_in[0].gl_Position + vec4(0.0f, 5.0f, 0.0f, 0.0f);
    EmitVertex();
    EndPrimitive();
}
