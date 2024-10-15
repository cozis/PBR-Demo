#version 330 core

layout (location=0) in vec3 aPos;
layout (location=1) in vec3 aNormal;
layout (location=2) in vec2 aTexCoords;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 norm;
uniform mat4 light_space_matrix;

uniform vec3 viewPos;

out vec3 frag_normal;
out vec3 fragPos;
out vec4 frag_pos_light_space;

void main()
{
	gl_Position = projection * view * model * vec4(aPos, 1.0);
	fragPos = vec3(model * vec4(aPos, 1.0));
	frag_normal = normalize(mat3(norm) * aNormal);
	frag_pos_light_space = light_space_matrix * model * vec4(aPos, 1);
}
