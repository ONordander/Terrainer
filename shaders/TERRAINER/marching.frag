#version 410

out vec4 frag_color;

uniform sampler3D noise_tex;

void main()
{
	vec4 col = texture(noise_tex, gl_Position);
    frag_color = vec4(col.xyz, 1.0f);
}
