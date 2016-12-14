#version 410

out vec4 frag_color;
in vec3 normal;
in vec3 vertex;

uniform mat4 normal_model_to_world;
uniform mat4 vertex_model_to_world;
uniform vec3 camera_pos;
uniform vec3 light_position;
uniform vec4 light_ambient;
uniform sampler2D marble_tex;
uniform sampler3D noise_tex;

void main()
{
    vec3 vertex_new = vec4(vertex_model_to_world * vec4(vertex, 1.0)).xyz;
    vec3 N = normalize(vec4(normal_model_to_world * vec4(normal, 0.0)).xyz);
    vec3 blend_w = abs(N);
    blend_w = normalize(max(blend_w, 0.0));
    float b = (blend_w.x + blend_w.y + blend_w.z);
    blend_w /= vec3(b, b, b);

    vec4 blended_col;
    vec2 coord_1 = (vertex_new.yz + 1.0f) / 2.0f;
    vec2 coord_2 = (vertex_new.zx + 1.0f) / 2.0f;
    vec2 coord_3 = (vertex_new.xy + 1.0f) / 2.0f;

    vec4 color_1 = texture(marble_tex, coord_1);
    vec4 color_2 = texture(marble_tex, coord_2);
    vec4 color_3 = texture(marble_tex, coord_3);

    blended_col = color_1.xyzw * blend_w.xxxx + 
                  color_2.xyzw * blend_w.yyyy +
                  color_3.xyzw * blend_w.zzzz;


    vec3 V = normalize(camera_pos - vertex_new);
    vec3 L = normalize(light_position - vertex_new);
    vec3 R = normalize(reflect(-L, N));

    vec4 light_diffuse = blended_col * max(dot(N, L), 0.0);
    vec3 light_specular = vec3(0.03, 0.03, 0.03) * pow(max(dot(V, R), 0.0), 100.0);
    frag_color = vec4(light_diffuse.xyz + light_specular.xyz + light_ambient.xyz, 1.0);
}
