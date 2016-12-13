#version 410

out vec4 frag_color;
in vec3 normal;
in vec3 vertex;

uniform mat4 normal_model_to_world;
uniform mat4 vertex_model_to_world;
uniform vec3 camera_pos;
uniform vec3 light_position;

void main()
{
    vec3 N = vec4(normal_model_to_world * vec4(normal, 0.0)).xyz;
    vec3 vertex_new = vec4(vertex_model_to_world * vec4(vertex, 0.0)).xyz;

    vec3 V = normalize(camera_pos - vertex_new);
    vec3 L = normalize(light_position - vertex_new);
    vec3 R = normalize(reflect(-L, N));

    vec4 light_diffuse = vec4(vec3(0.8, 0.8, 0.8) * max(dot(N, L), 0.0f), 0.0);
    vec4 light_specular = vec4(vec3(0.8, 0.8, 0.8) * pow(max(dot(V, R), 0.0), 100.0f), 0.0);
    frag_color = vec4(light_diffuse.xyz + light_specular.xyz, 1.0);
}
