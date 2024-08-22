#version 330 core

// https://github.com/Nadrin/PBR/blob/master/data/shaders/glsl/pbr_fs.glsl

#define PI 3.1415926538

out vec4 FragColor;

in vec3 frag_normal;
in vec3 fragPos;
in vec4 frag_pos_light_space;

uniform sampler2D shadow_map;
uniform vec3 viewPos;

uniform float perceptualRoughness; //  [0, 1]
uniform float metallic; // [0, 1]
uniform float reflectance; // [0, 1]
uniform vec3  baseColor;

uniform vec3 lightDir;
uniform vec3 lightColor;

float shadow_factor(vec4 frag_pos_light_space);

float D_GGX(float NoH, float a) {
    float a2 = a * a;
    float f = (NoH * a2 - NoH) * NoH + 1.0;
    return a2 / (PI * f * f);
}

vec3 F_Schlick(float u, vec3 f0) {
    return f0 + (vec3(1.0) - f0) * pow(1.0 - u, 5.0);
}

float V_SmithGGXCorrelated(float NoV, float NoL, float a) {
    float a2 = a * a;
    float GGXL = NoV * sqrt((-NoL * a2 + NoL) * NoL + a2);
    float GGXV = NoL * sqrt((-NoV * a2 + NoV) * NoV + a2);
    return 0.5 / (GGXV + GGXL);
}

float Fd_Lambert()
{
    return 1.0 / PI;
}

void main()
{
    vec3 l = normalize(lightDir);
    vec3 v = normalize(viewPos - fragPos);
    vec3 n = frag_normal;
    vec3 h = normalize(v + l);

    float light_intensity = 1;

    vec3 albedo = baseColor;
    vec3 diffuseColor = (1.0 - metallic) * baseColor;

    vec3 f0 = 0.16 * reflectance * reflectance * (1.0 - metallic)
            + baseColor * metallic;

    float NoV = abs(dot(n, v)) + 1e-5;
    float NoL = clamp(dot(n, l), 0.0, 1.0);
    float NoH = clamp(dot(n, h), 0.0, 1.0);
    float LoH = clamp(dot(l, h), 0.0, 1.0);

    float roughness = perceptualRoughness * perceptualRoughness;

    float D = D_GGX(NoH, roughness);
    vec3  F = F_Schlick(LoH, f0);
    float V = V_SmithGGXCorrelated(NoV, NoL, roughness);

    // specular BRDF
    float denominator = 4.0 * max(dot(n, v), 0.0) * max(dot(n, l), 0.0) + 0.0001; // + 0.0001 to prevent divide by zero
    vec3 Fr = (D * V) * F / denominator;

    vec3 radiance = lightColor;
    vec3 light = ((vec3(1.0) - F) * (1.0 - metallic) * albedo / PI + Fr) * radiance * NoL;

    float shadow = shadow_factor(frag_pos_light_space);

    vec3 ambient = vec3(0.03) * albedo;
    vec3 color = (ambient + light) * (1 - shadow);

/*
    // HDR tonemapping
    color = color / (color + vec3(1.0));
    // gamma correct
    color = pow(color, vec3(1.0/2.2)); 
*/
    FragColor = vec4(color, 1.0);
}

float shadow_factor(vec4 frag_pos_light_space)
{
    vec3 proj_coords = frag_pos_light_space.xyz / frag_pos_light_space.w;
    proj_coords = proj_coords * 0.5 + 0.5;

    float closest_depth = texture(shadow_map, proj_coords.xy).r;
    float current_depth = proj_coords.z;

    float bias = 0.005;
    float shadow = 0;
    vec2 delta = 1.0 / textureSize(shadow_map, 0);
    for (int i = -1; i < 2; i++)
        for (int j = -1; j < 2; j++) {
            float closest_depth = texture(shadow_map, proj_coords.xy + vec2(i, j) * delta).r;
            shadow += (current_depth - bias > closest_depth) ? 1.0 : 0.0;
        }
    shadow /= 9;
    return shadow;
}