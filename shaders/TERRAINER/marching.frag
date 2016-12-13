#version 410

out vec4 frag_color;
in vec3 normal;

void main()
{
    vec3 coord = vec3(0.3, 0.3, 0.3);
    frag_color = vec4(coord, 1.0);
}
