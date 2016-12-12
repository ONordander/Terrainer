#version 410

out vec4 frag_color;

uniform sampler1D cube_tex;
uniform isampler2D edge_conn;

void main()
{
    frag_color = vec4(1.0, 1.0, 1.0, 1.0f);
}
