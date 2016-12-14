#version 410

out vec4 frag_color;
in vec3 normal;

uniform sampler3D noise_tex;

void main()
{
	//vec4 col = texture(noise_tex, gl_Position);
    //frag_color = vec4(col.xyz, 1.0f);
    vec3 coord = vec3(0.3, 0.3, 0.3);
    frag_color = vec4(coord, 1.0);
}
