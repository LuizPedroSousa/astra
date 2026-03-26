#version 450 core
layout (location = 0) out vec3 g_position;
layout (location = 1) out vec3 g_normal;
layout (location = 2) out vec4 g_albedo;


in OBJECT_COORDINATES {
    vec2 texture;
    vec3 fragment;
    vec3 fragment_view_space;
    mat4 model;
    vec4 fragment_light_space;
    vec4 tangent_fragment_light_space;
    vec3 tangent_fragment;
    vec3 tangent_light_position;
    vec3 tangent_view_position;
    vec3 normal;
    vec3 normal_view_space;
}
obj_coordinates;

struct Material {
    sampler2D diffuse;
    sampler2D specular;

    float shininess;
};

#define NR_MATERIALS 1

uniform Material materials[NR_MATERIALS];
uniform sampler2D normal_map;

void main()
{
    g_position = obj_coordinates.fragment;
    g_normal = normalize(obj_coordinates.normal);
    g_albedo.rgb = texture(materials[0].diffuse, obj_coordinates.texture).rgb;
    g_albedo.a = texture(materials[0].specular, obj_coordinates.texture).r;
}
