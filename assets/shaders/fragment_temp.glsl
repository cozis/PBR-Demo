#version 330 core

struct Light {
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

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

uniform Light light;

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

float Fd_Lambert() {
    return 1.0 / PI;
}

void main()
{
    // NOTE: frag_normal is normalized by the vertex shader
/*
    vec3 l = normalize(light.direction);
    vec3 v = viewDir;
    vec3 n = frag_normal;
    vec3 h = normalize(v + l);
    vec3 diffuseColor = (1.0 - metallic) * baseColor.rgb;
    vec3 f0 = 0.16 * reflectance * reflectance * (1.0 - metallic) + baseColor * metallic;

    float NoV = abs(dot(n, v)) + 1e-5;
    float NoL = clamp(dot(n, l), 0.0, 1.0);
    float NoH = clamp(dot(n, h), 0.0, 1.0);
    float LoH = clamp(dot(l, h), 0.0, 1.0);

    float roughness = perceptualRoughness * perceptualRoughness;

    float D = D_GGX(NoH, roughness);
    vec3  F = F_Schlick(LoH, f0);
    float V = V_SmithGGXCorrelated(NoV, NoL, roughness);

    // specular BRDF
    vec3 Fr = (D * V) * F;

    //vec3 energyCompensation = 1.0 + f0 * (1.0 / dfg.y - 1.0);
    //// Scale the specular lobe to account for multiscattering
    //Fr *= pixel.energyCompensation;

    // diffuse BRDF
    vec3 Fd = diffuseColor * Fd_Lambert();

    // apply lighting...

    float shadow = shadow_factor(frag_pos_light_space);
*/
    vec3 result = vec3(1, 0, 0); // Fd + Fr * (1 - shadow);
    FragColor = vec4(result, 1.0);
}
