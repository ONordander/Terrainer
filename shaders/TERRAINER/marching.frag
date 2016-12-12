#version 410

out vec4 frag_color;

uniform sampler1D cube_tex;
uniform isampler2D edge_conn;

void main()
{
    vec4 col = texelFetch(cube_tex, 1, 0);
    if (col.r > 0) {
        frag_color = vec4(0.3f);
        return;
    }
    frag_color = vec4(1.0, 1.0, 1.0, 1.0f);
}