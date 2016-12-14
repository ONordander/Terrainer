#version 410

uniform mat4 vertex_model_to_world;
uniform sampler3D noise_t;

layout (location = 0) in vec3 vertex;
layout (location = 0) out float density;

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

void main()
{
    vec4 world_pos = vertex_model_to_world * vec4(vertex, 1.0);
    density = -world_pos.y;
    density += smooth_noise(world_pos);
}