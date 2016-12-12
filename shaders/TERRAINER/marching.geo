#version 410

layout(points) in;
layout(triangle_strip, max_vertices = 3) out;

uniform sampler2D cube_tex;
uniform float cube_step;

float density(vec3 world_pos)
{
	return -world_pos.y;
}

void main()
{
	gl_Position = gl_in[0].gl_Position;
	EmitVertex();

    gl_Position = gl_in[0].gl_Position + vec4(2.0f, 0.0f, 0.0f, 0.0f);
    EmitVertex();

    gl_Position = gl_in[0].gl_Position + vec4(0.0f, 5.0f, 0.0f, 0.0f);
    EmitVertex();
    EndPrimitive();
    /*
	
	//sample at cube corners
	//corner 0
	float d0 = density(gl_Position.xyz);
	//corner 1
	float d1 = density(vec3(gl_Position.x, gl_Position.y + cube_step, gl_Position.z));
	//corner 2
	float d2 = density(vec3(gl_Position.x + cube_step, gl_Position.y + cube_step, gl_Position.z));
	//corner 3
	float d3 = density(vec3(gl_Position.x + cube_step, gl_Position.y, gl_Position.z));	
	//corner 4
	float d4 = density(vec3(gl_Position.x, gl_Position.y, gl_Position.z + cube_step));
	//corner 5
	float d5 = density(vec3(gl_Position.x, gl_Position.y + cube_step, gl_Position.z + cube_step));
	//corner 6
	float d6 = density(vec3(gl_Position.x + cube_step, gl_Position.y + cube_step, gl_Position.z + cube_step));
	//corner 7
	float d7 = density(vec3(gl_Position.x + cube_step, gl_Position.y, gl_Position.z + cube_step));
	
	int lookup_idx;

	if (d0 > 0)
		lookup_idx |= (1 << 0);
	if (d1 > 0)
		lookup_idx |= (1 << 1);
	if (d2 > 0)
		lookup_idx |= (1 << 2);
	if (d3 > 0)
		lookup_idx |= (1 << 3);
	if (d4 > 0)
		lookup_idx |= (1 << 4);
	if (d5 > 0)
		lookup_idx |= (1 << 5);
	if (d6 > 0)
		lookup_idx |= (1 << 6);
	if (d7 > 0)
		lookup_idx |= (1 << 7);
	*/
}

