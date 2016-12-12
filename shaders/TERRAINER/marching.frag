#version 410

out vec4 frag_color;

uniform sampler2D cube_tex;
uniform isampler2D edge_conn;

void main()
{
    vec4 col = texture(cube_tex, vec2(0, 0));
    vec4 edge_c = texelFetch(edge_conn, ivec2(6, 0), 0);
    if (edge_c.a > 1) {
        frag_color = vec4(0.3f);
        return;
    }
    frag_color = vec4(1.0/abs(edge_c.a), 1.0/abs(edge_c.g), 1.0/abs(edge_c.b), 1.0);
}
