#version 410

layout(points) in;
layout(triangle_strip, max_vertices = 15) out;

out vec3 normal;
out vec3 vertex;

uniform isampler1D edge_tex;
uniform sampler2D noise_tex;
uniform sampler3D noise_t;
uniform float cube_step;
uniform mat4 vertex_world_to_clip;
uniform mat4 vertex_model_to_world;

const int edge_table[256] = int[256](0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,2,1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,3,1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,3,2,3,3,2,3,4,4,3,3,4,4,3,4,5,5,2,1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,3,2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,4,2,3,3,4,3,4,2,3,3,4,4,5,4,5,3,2,3,4,4,3,4,5,3,2,4,5,5,4,5,2,4,1,1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,3,2,3,3,4,3,4,4,5,3,2,4,3,4,3,5,2,2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,4,3,4,4,3,4,5,5,4,4,3,5,2,5,4,2,1,2,3,3,4,3,4,4,5,3,4,4,5,2,3,3,2,3,4,4,5,4,5,5,2,4,3,5,4,3,2,4,1,3,4,4,5,4,5,3,4,4,5,5,2,3,4,2,1,2,3,3,2,3,4,2,1,3,2,4,1,2,1,1,0);



float smooth_noise(vec4 world_pos)
{
	float x = world_pos.x;
	float y = world_pos.y;
	float z = world_pos.z;

	float fractX = x - int(x);
	float fractY = y - int(y);
	float fractZ = z - int(z);

	int x1 = int(mod(int(x) + 32, 32));
	int y1 = int(mod(int(y) + 32, 32));
	int z1 = int(mod(int(z) + 32, 32));

	int x2 = int(mod(x1 + 32 - 1, 32));
	int y2 = int(mod(y1 + 32 - 1, 32));
	int z2 = int(mod(z1 + 32 - 1, 32));


	float value = 0.0;
	value += fractX * fractY * fractZ * texelFetch(noise_t, ivec3(z1, y1, x1), 0).r;
	value += fractX * (1 - fractY) * fractZ * texelFetch(noise_t, ivec3(z1, y2, x1), 0).r;
	value += (1 - fractX) * fractY * fractZ * texelFetch(noise_t, ivec3(z1, y1, x2), 0).r;
	value += (1 - fractX) * (1 - fractY) * fractZ * texelFetch(noise_t, ivec3(z1, y2, x2), 0).r;

	value += fractX * fractY * (1 - fractZ) * texelFetch(noise_t, ivec3(z2, y1, x1), 0).r;
	value += fractX * (1 - fractY) * (1 - fractZ) * texelFetch(noise_t, ivec3(z2, y2, x1), 0).r;
	value += (1 - fractX) * fractY * (1 - fractZ) * texelFetch(noise_t, ivec3(z2, y1, x2), 0).r;
	value += (1 - fractX) * (1 - fractY) * (1 - fractZ) * texelFetch(noise_t, ivec3(z2, y2, x2), 0).r;

  	return value;
}

float turbulence(vec4 world_pos, float initial_size)
{
	float value = 0.0f;
	float size = initial_size;
	while (size >= 1) {
		value += smooth_noise(world_pos / size) * size;
		size /= 2.0f;
	}
	return (128.0 * value / initial_size);
}

float density(vec4 world_pos)
{
	//return (world_pos, 32.0f);
	return smooth_noise(world_pos);
	//return (texture(noise_t, ((world_pos.xyz + 1) / 2)).r * 2) - 1;
	//return (texture(noise_tex, world_pos.xy).r * 2) - 1;
	//return -world_pos.y;
}


vec4 interp(int index, float densities[8])
{
	float x = gl_in[0].gl_Position.x;
	float y = gl_in[0].gl_Position.y;
	float z = gl_in[0].gl_Position.z;

	if (index == 0) {
		float i_factor = mix(densities[0], densities[1], abs(densities[0] / (densities[1] - densities[0])));
		return vec4(x, y + i_factor * cube_step, z, 1.0f);
	}
	if (index == 1) {
		float i_factor = mix(densities[1], densities[2], abs(densities[1] / (densities[2] - densities[1])));
		return vec4(x + i_factor * cube_step, y + cube_step, z, 1.0);
	}
	if (index == 2) {
		float i_factor = mix(densities[2], densities[3], abs(densities[2] / (densities[3] - densities[2])));
		return vec4(x + cube_step, y + i_factor * cube_step, z, 1.0);
	}
	if (index == 3) {
		float i_factor = mix(densities[0], densities[3], abs(densities[0] / (densities[3] - densities[0])));
		return vec4(x + i_factor * cube_step, y, z, 1.0);
	}
	if (index == 4) {
		float i_factor = mix(densities[4], densities[5], abs(densities[4] / (densities[5] - densities[4])));
		return vec4(x, y + i_factor * cube_step, z + cube_step, 1.0);
	}
	if (index == 5) {
		float i_factor = mix(densities[5], densities[6], abs(densities[5] / (densities[6] - densities[5])));
		return vec4(x + i_factor * cube_step, y + cube_step, z + cube_step, 1.0);
	}
	if (index == 6) {
		float i_factor = mix(densities[6], densities[7], abs(densities[6] / (densities[7] - densities[6])));
		return vec4(x + cube_step, y + i_factor * cube_step, z + cube_step, 1.0);
	}
	if (index == 7) {
		float i_factor = mix(densities[4], densities[7], abs(densities[4] / (densities[7] - densities[4])));
		return vec4(x + i_factor * cube_step, y, z + cube_step, 1.0);
	}
	if (index == 8) {
		float i_factor = mix(densities[0], densities[4], abs(densities[0] / (densities[4] - densities[0])));
		return vec4(x, y, z + i_factor * cube_step, 1.0);
	}
	if (index == 9) {
		float i_factor = mix(densities[1], densities[5], abs(densities[1] / (densities[5] - densities[1])));
		return vec4(x, y + cube_step, z + i_factor * cube_step, 1.0);
	}
	if (index == 10) {
		float i_factor = mix(densities[2], densities[6], abs(densities[2] / (densities[6] - densities[2])));
		return vec4(x + cube_step, y + cube_step, z + i_factor * cube_step, 1.0);
	}
	if (index == 11) {
		float i_factor = mix(densities[3], densities[7], abs(densities[3] / (densities[3] - densities[7])));
		return vec4(x + cube_step, y, z + i_factor * cube_step, 1.0);
	}
	return vec4(1.0f);
}

void main()
{
	float densities[8];
	//sample at cube corners
	//corner 0
	densities[0] = density(vertex_model_to_world * gl_in[0].gl_Position);
	//corner 1
	densities[1] = density(vertex_model_to_world * vec4(gl_in[0].gl_Position.x, gl_in[0].gl_Position.y + cube_step, gl_in[0].gl_Position.z, 1.0f));
	//corner 2
	densities[2] = density(vertex_model_to_world * vec4(gl_in[0].gl_Position.x + cube_step, gl_in[0].gl_Position.y + cube_step, gl_in[0].gl_Position.z, 1.0f));
	//corner 3
	densities[3] = density(vertex_model_to_world * vec4(gl_in[0].gl_Position.x + cube_step, gl_in[0].gl_Position.y, gl_in[0].gl_Position.z, 1.0f));	
	//corner 4
	densities[4] = density(vertex_model_to_world * vec4(gl_in[0].gl_Position.x, gl_in[0].gl_Position.y, gl_in[0].gl_Position.z + cube_step, 1.0f));
	//corner 5
	densities[5] = density(vertex_model_to_world * vec4(gl_in[0].gl_Position.x, gl_in[0].gl_Position.y + cube_step, gl_in[0].gl_Position.z + cube_step, 1.0f));
	//corner 6
	densities[6] = density(vertex_model_to_world * vec4(gl_in[0].gl_Position.x + cube_step, gl_in[0].gl_Position.y + cube_step, gl_in[0].gl_Position.z + cube_step, 1.0f));
	//corner 7
	densities[7] = density(vertex_model_to_world * vec4(gl_in[0].gl_Position.x + cube_step, gl_in[0].gl_Position.y, gl_in[0].gl_Position.z + cube_step, 1.0f));
	
	int lookup_idx = 0;
	for (int i = 0; i < 8; i++) {
		if (densities[i] > 0) {
			lookup_idx += int(pow(2, i));
		}
	}

	if (lookup_idx == 0 || lookup_idx == 255)
		return;

	int i = 0;
	while (true) {
		int val = (20 * lookup_idx) + (i * 4);
		if (texelFetch(edge_tex, val, 0).r == -1 || i > 5) {
			break;
		}
		int edge_1 = texelFetch(edge_tex, val, 0).r;
		int edge_2 = texelFetch(edge_tex, val + 1, 0).r;
		int edge_3 = texelFetch(edge_tex, val + 2, 0).r;
		vec4 v_1 = interp(edge_1, densities);
		vec4 v_2 = interp(edge_2, densities);
		vec4 v_3 = interp(edge_3, densities);
		normal = normalize(cross(v_2.xyz - v_1.xyz, v_3.xyz - v_1.xyz));
		vertex = v_1.xyz;
		gl_Position = vertex_world_to_clip * vertex_model_to_world * v_1;
		EmitVertex();
		normal = normalize(cross(v_1.xyz - v_2.xyz, v_3.xyz - v_2.xyz));
		vertex = v_2.xyz;
		gl_Position = vertex_world_to_clip * vertex_model_to_world * v_2;
		EmitVertex();
		normal = normalize(cross(v_1.xyz - v_3.xyz, v_2.xyz - v_3.xyz));
		vertex = v_3.xyz;
		gl_Position = vertex_world_to_clip * vertex_model_to_world * v_3;
		EmitVertex();
		EndPrimitive();
		i++;
	}
}
