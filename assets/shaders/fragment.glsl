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

// IBL
uniform samplerCube irradianceMap;
uniform samplerCube prefilterMap;
uniform sampler2D   brdfLUT;  

float shadow_factor(vec4 frag_pos_light_space);

float distribGGX(float NoH, float a) {
	float a2 = a * a;
	float f = (NoH * a2 - NoH) * NoH + 1.0;
	return a2 / (PI * f * f);
}

vec3 fresnelSchlick(float u, vec3 f0) {
	return f0 + (vec3(1.0) - f0) * pow(1.0 - u, 5.0);
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
	return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float geometrySmith(float NoV, float NoL, float a) {
	float a2 = a * a;
	float GGXL = NoV * sqrt((-NoL * a2 + NoL) * NoL + a2);
	float GGXV = NoL * sqrt((-NoV * a2 + NoV) * NoV + a2);
	return 0.5 / (GGXV + GGXL);
}

void main()
{
	vec3 l = normalize(lightDir);
	vec3 v = normalize(viewPos - fragPos);
	vec3 n = frag_normal;
	vec3 h = normalize(v + l);

	float light_intensity = 1;

	float perceptualRoughness2 = max(perceptualRoughness, 0.089);
	float roughness = perceptualRoughness2 * perceptualRoughness2;

	vec3 f0 = 0.16 * reflectance * reflectance * (1.0 - metallic) + baseColor * metallic;

	float NoV = abs(dot(n, v)) + 1e-5;
	float NoL = clamp(dot(n, l), 0.0, 1.0);
	float NoH = clamp(dot(n, h), 0.0, 1.0);
	float LoH = clamp(dot(l, h), 0.0, 1.0);

	vec3 color = vec3(0);

	float shadow = shadow_factor(frag_pos_light_space);

	// Direct lighting
	{
		float D = distribGGX(NoH, roughness);
		vec3  F = fresnelSchlick(LoH, f0);
		float V = geometrySmith(NoV, NoL, roughness);

		vec3 specular = (F * D * V) / (4.0 * NoV * NoL + 0.0001);

		vec3 diffuse = (1 - F) * (1 - metallic) * baseColor / PI;

		color = (1 - shadow) * (diffuse + specular) * lightColor * light_intensity * NoL;
	}

	// Ambient lighting with IBL
	vec3 ambient;
	{
		vec3 F = fresnelSchlickRoughness(NoV, f0, roughness);

		vec3 irradiance = texture(irradianceMap, n).rgb;
		vec3 diffuse = irradiance * baseColor;

		float max_lod = 4;
		float lod = roughness * max_lod;

		vec3 r = reflect(-v, n); // -v?
		vec3 prefilterColor = textureLod(prefilterMap, r, lod).rgb;

		vec2 brdf = texture(brdfLUT, vec2(NoV, roughness)).rg;
		vec3 specular = prefilterColor * (F * brdf.x + brdf.y);

		float ao = 1;
		ambient = ((1 - F) * (1 - metallic) * diffuse + specular) * ao;
	}

	color = color + ambient;

	color = color / (color + vec3(1.0)); // HDR tonemapping
	color = pow(color, vec3(1.0/2.2)); // gamma correct

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