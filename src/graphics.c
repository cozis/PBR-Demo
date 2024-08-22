#include <stdio.h>
#include <stdlib.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "mesh.h"
#include "camera.h"
#include "vector.h"
#include "graphics.h"

typedef struct {
    unsigned int vao;
    unsigned int vbo;
    int num_vertices;
} GPUMeshBuffer;

#define SHADOW_WIDTH  1024
#define SHADOW_HEIGHT 1024

static unsigned int envCubemap;
static unsigned int captureFBO;
static unsigned int captureRBO;
static unsigned int depth_map_fbo;
static unsigned int depth_map;
static unsigned int hdrTexture;
static unsigned int irradianceMap;
static unsigned int prefilterMap;
static unsigned int brdfLUTTexture;

/*
 * Shader program handles
 */
static unsigned int background_program;
static unsigned int shader_program;
static unsigned int shadow_program;
static unsigned int skybox_program;

static GPUMeshBuffer sphere_buffer;
static GPUMeshBuffer cube_buffer;

static GLFWwindow *window_;

static char *load_file(const char *file, size_t *size)
{
    FILE *stream = fopen(file, "rb");
    if (stream == NULL) return NULL;

    fseek(stream, 0, SEEK_END);
    long size2 = ftell(stream);
    fseek(stream, 0, SEEK_SET);

    char *dst = (char*) malloc(size2+1);
    if (dst == NULL) {
        fclose(stream);
        return NULL;
    }

    fread(dst, 1, size2, stream);
    if (ferror(stream)) {
        free(dst);
        fclose(stream);
        return NULL;
    }
    dst[size2] = '\0';

    fclose(stream);
    if (size) *size = size2;
    return dst;
}

static unsigned int
compile_shader(const char *vertex_file,
               const char *fragment_file)
{
    int  success;
    char infolog[512];

    char *vertex_str = load_file(vertex_file, NULL);
    if (vertex_str == NULL) {
        fprintf(stderr, "Couldn't load file '%s'\n", vertex_file);
        return 0;
    }

    char *fragment_str = load_file(fragment_file, NULL);
    if (fragment_str == NULL) {
        fprintf(stderr, "Couldn't load file '%s'\n", fragment_file);
        free(vertex_str);
        return 0;
    }

    unsigned int vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &vertex_str, NULL);
    glCompileShader(vertex_shader);

    glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);
    if(!success) {
        glGetShaderInfoLog(vertex_shader, sizeof(infolog), NULL, infolog);
        fprintf(stderr, "Couldn't compile vertex shader '%s' (%s)\n", vertex_file, infolog);
        free(vertex_str);
        free(fragment_str);
        return 0;
    }

    unsigned int fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &fragment_str, NULL);
    glCompileShader(fragment_shader);

    glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);
    if(!success) {
        glGetShaderInfoLog(fragment_shader, sizeof(infolog), NULL, infolog);
        fprintf(stderr, "Couldn't compile fragment shader '%s' (%s)\n", fragment_file, infolog);
        free(vertex_str);
        free(fragment_str);
        return 0;
    }

    unsigned int shader_program = glCreateProgram();
    glAttachShader(shader_program, vertex_shader);
    glAttachShader(shader_program, fragment_shader);
    glLinkProgram(shader_program);

    glGetProgramiv(shader_program, GL_LINK_STATUS, &success);
    if(!success) {
        glGetProgramInfoLog(shader_program, sizeof(infolog), NULL, infolog);
        fprintf(stderr, "Couldn't link shader program (%s)\n", infolog);
        free(vertex_str);
        free(fragment_str);
        return 0;
    }

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    free(vertex_str);
    free(fragment_str);
    return shader_program;
}

static void set_uniform_m4(unsigned int program, const char *name, Matrix4 value)
{
	int location = glGetUniformLocation(program, name);
	if (location < 0) {
		printf("Can't set uniform '%s'\n", name);
		abort();
	}
	glUniformMatrix4fv(location, 1, GL_FALSE, (float*) &value);
}

static void set_uniform_v3(unsigned int program, const char *name, Vector3 value)
{
	int location = glGetUniformLocation(program, name);
	if (location < 0) {
		printf("Can't set uniform '%s' (program %d, location %d)\n", name, program, location);
		abort();
	}
	glUniform3f(location, value.x, value.y, value.z);
}

static void set_uniform_i(unsigned int program, const char *name, int value)
{
	int location = glGetUniformLocation(program, name);
	if (location < 0) {
		printf("Can't set uniform '%s'\n", name);
		abort();
	}
	glUniform1i(location, value);
}

static void set_uniform_f(unsigned int program, const char *name, float value)
{
	int location = glGetUniformLocation(program, name);
	if (location < 0) {
		printf("Can't set uniform '%s'\n", name);
		abort();
	}
	glUniform1f(location, value);
}

static GPUMeshBuffer create_gpu_mesh_buffer(VertexArray vertices)
{
    GPUMeshBuffer buffer;

    glGenVertexArrays(1, &buffer.vao);
    glGenBuffers(1, &buffer.vbo);

    glBindVertexArray(buffer.vao);

    glBindBuffer(GL_ARRAY_BUFFER, buffer.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * vertices.size, vertices.data, GL_STATIC_DRAW);

    // positions
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(0);

    // normals
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*) (offsetof(Vertex, nx)));
    glEnableVertexAttribArray(1);

    // texture coordinates
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*) (offsetof(Vertex, tx)));
    glEnableVertexAttribArray(2);

    buffer.num_vertices = vertices.size;

    return buffer;
}

static void draw_mesh_for_shadow_map(GPUMeshBuffer buffer, Matrix4 model)
{
    glUseProgram(shadow_program);
	set_uniform_m4(shadow_program, "model", model);
	glBindVertexArray(buffer.vao);
    glDrawArrays(GL_TRIANGLES, 0, buffer.num_vertices);
}

static void draw_mesh(GPUMeshBuffer buffer, Matrix4 model, Material material)
{
	Matrix4 temp;
	assert(invert(model, &temp));
    Matrix4 normal = transpose(temp);

    glUseProgram(shader_program);

    set_uniform_f(shader_program, "perceptualRoughness", material.perceptualRoughness);
    set_uniform_f(shader_program, "metallic", material.metallic);
    set_uniform_f(shader_program, "reflectance", material.reflectance);
    set_uniform_v3(shader_program, "baseColor", material.baseColor);
	set_uniform_m4(shader_program, "model", model);
	set_uniform_m4(shader_program, "norm", normal);

	glBindVertexArray(buffer.vao);
	glDrawArrays(GL_TRIANGLES, 0, buffer.num_vertices);
}

unsigned int cubeVAO = 0;
unsigned int cubeVBO = 0;
void renderCube()
{
    // initialize (if necessary)
    if (cubeVAO == 0)
    {
        float vertices[] = {
            // back face
            -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
             1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
             1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 0.0f, // bottom-right         
             1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
            -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
            -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 1.0f, // top-left
            // front face
            -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
             1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f, // bottom-right
             1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
             1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
            -1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 1.0f, // top-left
            -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
            // left face
            -1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
            -1.0f,  1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-left
            -1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
            -1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
            -1.0f, -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-right
            -1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
            // right face
             1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
             1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
             1.0f,  1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-right         
             1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
             1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
             1.0f, -1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-left     
            // bottom face
            -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
             1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 1.0f, // top-left
             1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
             1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
            -1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 0.0f, // bottom-right
            -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
            // top face
            -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
             1.0f,  1.0f , 1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
             1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 1.0f, // top-right     
             1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
            -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
            -1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 0.0f  // bottom-left        
        };
        glGenVertexArrays(1, &cubeVAO);
        glGenBuffers(1, &cubeVBO);
        // fill buffer
        glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        // link vertex attributes
        glBindVertexArray(cubeVAO);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }
    // render Cube
    glBindVertexArray(cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
}

// renderQuad() renders a 1x1 XY quad in NDC
// -----------------------------------------
unsigned int quadVAO = 0;
unsigned int quadVBO;
void renderQuad()
{
    if (quadVAO == 0)
    {
        float quadVertices[] = {
            // positions        // texture Coords
            -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
            -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
             1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
             1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
        };
        // setup plane VAO
        glGenVertexArrays(1, &quadVAO);
        glGenBuffers(1, &quadVBO);
        glBindVertexArray(quadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    }
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

void init_graphics(void *window)
{
	window_ = window;
	
    shader_program = compile_shader("assets/shaders/vertex.glsl", "assets/shaders/fragment.glsl");
    shadow_program = compile_shader("assets/shaders/shadow_vertex.glsl", "assets/shaders/shadow_fragment.glsl");

    unsigned int equirectangular_to_cubemap_program = compile_shader("assets/shaders/cubemap_vertex.glsl", "assets/shaders/equirectangular_to_cubemap_fragment.glsl");
    unsigned int irradiance_convolution_program = compile_shader("assets/shaders/cubemap_vertex.glsl", "assets/shaders/irradiance_convolution_fragment.glsl");
    background_program = compile_shader("assets/shaders/background_vertex.glsl", "assets/shaders/background_fragment.glsl");

    {
        VertexArray vertices = make_sphere_mesh(0.5);
        sphere_buffer = create_gpu_mesh_buffer(vertices);
		free(vertices.data);
    }

	{
        VertexArray vertices = make_cube_mesh();
        cube_buffer = create_gpu_mesh_buffer(vertices);
		free(vertices.data);
    }

    {
        glGenFramebuffers(1, &captureFBO);
        glGenRenderbuffers(1, &captureRBO);

        glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
        glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, captureRBO);
    }

    {
        stbi_set_flip_vertically_on_load(true);
        int width, height, nrComponents;
        float *data = stbi_loadf("assets/spruit_sunrise_4k.hdr", &width, &height, &nrComponents, 0);
        if (!data) {
            fprintf(stderr, "Couldn't load map\n");
            abort();
        }

        glGenTextures(1, &hdrTexture);
        glBindTexture(GL_TEXTURE_2D, hdrTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, data); // note how we specify the texture's data value to be float

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    }

    // pbr: set up projection and view matrices for capturing data onto the 6 cubemap face directions
    // ----------------------------------------------------------------------------------------------
    Matrix4 captureProjection = perspective_matrix(deg2rad(90.0f), 1.0f, 0.1f, 10.0f);
    Matrix4 captureViews[] = {
        lookat_matrix((Vector3) {0.0f, 0.0f, 0.0f}, (Vector3) {1.0f,  0.0f,  0.0f}, (Vector3) {0.0f, -1.0f,  0.0f}),
        lookat_matrix((Vector3) {0.0f, 0.0f, 0.0f}, (Vector3) {-1.0f,  0.0f,  0.0f}, (Vector3) {0.0f, -1.0f,  0.0f}),
        lookat_matrix((Vector3) {0.0f, 0.0f, 0.0f}, (Vector3) {0.0f,  1.0f,  0.0f}, (Vector3) {0.0f,  0.0f,  1.0f}),
        lookat_matrix((Vector3) {0.0f, 0.0f, 0.0f}, (Vector3) {0.0f, -1.0f,  0.0f}, (Vector3) {0.0f,  0.0f, -1.0f}),
        lookat_matrix((Vector3) {0.0f, 0.0f, 0.0f}, (Vector3) {0.0f,  0.0f,  1.0f}, (Vector3) {0.0f, -1.0f,  0.0f}),
        lookat_matrix((Vector3) {0.0f, 0.0f, 0.0f}, (Vector3) {0.0f,  0.0f, -1.0f}, (Vector3) {0.0f, -1.0f,  0.0f})
    };
    {
        glGenTextures(1, &envCubemap);
        glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);
        for (unsigned int i = 0; i < 6; ++i)
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 512, 512, 0, GL_RGB, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR); 
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // pbr: convert HDR equirectangular environment map to cubemap equivalent
        // ----------------------------------------------------------------------
        glUseProgram(equirectangular_to_cubemap_program);
        glUniform1i(glGetUniformLocation(equirectangular_to_cubemap_program, "equirectangularMap"), 0);
        glUniformMatrix4fv(glGetUniformLocation(equirectangular_to_cubemap_program, "projection"), 1, false, (float*) &captureProjection);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, hdrTexture);

        glViewport(0, 0, 512, 512); // don't forget to configure the viewport to the capture dimensions.
        glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
        for (unsigned int i = 0; i < 6; ++i) {
            glUniformMatrix4fv(glGetUniformLocation(equirectangular_to_cubemap_program, "view"), 1, false, (float*) &captureViews[i]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, envCubemap, 0);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            renderCube();
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    {
        // pbr: create an irradiance cubemap, and re-scale capture FBO to irradiance scale.
        // --------------------------------------------------------------------------------
        glGenTextures(1, &irradianceMap);
        glBindTexture(GL_TEXTURE_CUBE_MAP, irradianceMap);
        for (unsigned int i = 0; i < 6; ++i)
        {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 32, 32, 0, GL_RGB, GL_FLOAT, NULL);
        }
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
        glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 32, 32);
    }

    {
        glUseProgram(irradiance_convolution_program);
        glUniform1i(glGetUniformLocation(irradiance_convolution_program, "environmentMap"), 0);
        glUniformMatrix4fv(glGetUniformLocation(irradiance_convolution_program, "projection"), 1, GL_FALSE, (float*) &captureProjection);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);

        glViewport(0, 0, 32, 32); // don't forget to configure the viewport to the capture dimensions.
        glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
        for (unsigned int i = 0; i < 6; ++i)
        {
            glUniformMatrix4fv(glGetUniformLocation(irradiance_convolution_program, "view"), 1, GL_FALSE, (float*) &captureViews[i]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, irradianceMap, 0);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            renderCube();
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    {
        int w, h;
        glfwGetWindowSize(window, &w, &h);

        Matrix4 projection = perspective_matrix(deg2rad(30.0f), (float) w / (float) h, 0.1f, 100.0f);
        glUseProgram(background_program);
        glUniformMatrix4fv(glGetUniformLocation(background_program, "projection"), 1, false, (float*) &projection);
    }    

    {
        glGenFramebuffers(1, &depth_map_fbo);

        glGenTextures(1, &depth_map);
        glBindTexture(GL_TEXTURE_2D, depth_map);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER); 
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

        glBindFramebuffer(GL_FRAMEBUFFER, depth_map_fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_map, 0);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

	{
		glGenTextures(1, &prefilterMap);
		glBindTexture(GL_TEXTURE_CUBE_MAP, prefilterMap);
		for (unsigned int i = 0; i < 6; ++i)
		{
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 128, 128, 0, GL_RGB, GL_FLOAT, NULL);
		}
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); 
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
	}

	unsigned int prefilter_program = compile_shader("assets/shaders/cubemap_vertex.glsl", "assets/shaders/prefilter_fragment.glsl");

	{
		glUseProgram(prefilter_program);
		glUniform1i(glGetUniformLocation(prefilter_program, "environmentMap"), 0);
		glUniformMatrix4fv(glGetUniformLocation(prefilter_program, "projection"), 1, false, (float*) &captureProjection);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);

		glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
		unsigned int maxMipLevels = 5;
		for (unsigned int mip = 0; mip < maxMipLevels; ++mip)
		{
			// reisze framebuffer according to mip-level size.
			unsigned int mipWidth  = 128 * pow(0.5, mip);
			unsigned int mipHeight = 128 * pow(0.5, mip);
			glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, mipWidth, mipHeight);
			glViewport(0, 0, mipWidth, mipHeight);

			float roughness = (float)mip / (float)(maxMipLevels - 1);

			glUniform1f(glGetUniformLocation(prefilter_program, "roughness"), roughness);

			for (unsigned int i = 0; i < 6; ++i)
			{
				glUniformMatrix4fv(glGetUniformLocation(prefilter_program, "view"), 1, false, (float*) &captureViews[i]);

				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, 
									GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, prefilterMap, mip);

				glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
				renderCube();
			}
		}
		glBindFramebuffer(GL_FRAMEBUFFER, 0);  
	}

	unsigned int brdf_program = compile_shader("assets/shaders/brdf_vertex.glsl", "assets/shaders/brdf_fragment.glsl");

	{
		glGenTextures(1, &brdfLUTTexture);

		// pre-allocate enough memory for the LUT texture.
		glBindTexture(GL_TEXTURE_2D, brdfLUTTexture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, 512, 512, 0, GL_RG, GL_FLOAT, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); 
	
		glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
		glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, brdfLUTTexture, 0);

		glViewport(0, 0, 512, 512);
		glUseProgram(brdf_program);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		renderQuad();

		glBindFramebuffer(GL_FRAMEBUFFER, 0);  
	}

	unsigned int rect_program = compile_shader("assets/shaders/brdf_vertex.glsl", "assets/shaders/rect_fragment.glsl");

    unsigned int depth_render_buffer;

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    //glEnable(GL_DEPTH_TEST);
    //glEnable(GL_CULL_FACE); 
    //glCullFace(GL_BACK);
}

typedef struct {
	GPUMeshBuffer buffer;
	Matrix4       model;
	Material      mat;
} DrawCommand;

#define COMMAND_QUEUE_SIZE 1024
static DrawCommand command_queue[COMMAND_QUEUE_SIZE];
static int command_queue_head;
static int command_queue_used;

static void clear_commands(void)
{
	command_queue_head = 0;
	command_queue_used = 0;
}

static void apply_single_command(DrawCommand command, bool shadow_map)
{
	if (!shadow_map)
		draw_mesh(command.buffer, command.model, command.mat);
	else
		draw_mesh_for_shadow_map(command.buffer, command.model);
}

static void apply_commands(bool shadow_map)
{
	for (int i = 0; i < command_queue_used; i++)
		apply_single_command(command_queue[(command_queue_head + i) % COMMAND_QUEUE_SIZE], shadow_map);
}

static void push_command(DrawCommand command)
{
	if (command_queue_used == COMMAND_QUEUE_SIZE) {
		printf("Command queue full.. Ignoring draw call\n");
		return;
	}
	command_queue[(command_queue_head + command_queue_used) % COMMAND_QUEUE_SIZE] = command;
	command_queue_used++;
}

void draw_sphere(float x, float y, float z, float radius, Material mat)
{
	Matrix4 model = identity_matrix();
	model = dotm(model, translate_matrix((Vector3) {x, y, z}, 1));
	model = dotm(model, scale_matrix((Vector3) {radius, radius, radius}));
	push_command((DrawCommand) {.buffer = sphere_buffer, .model = model, .mat = mat});
}

void draw_cube(float x, float y, float z, float w, float h, float d, Material mat)
{
	Matrix4 model = identity_matrix();
	model = dotm(model, translate_matrix((Vector3) {x, y, z}, 1));
	model = dotm(model, scale_matrix((Vector3) {w, h, d}));
	push_command((DrawCommand) {.buffer = cube_buffer, .model = model, .mat = mat});
}

static Vector3 light_color = {1, 1, 1};
static Vector3 light_dir   = {1, 1, 1};

void set_light(Vector3 dir, Vector3 color)
{
	light_dir = dir;
	light_color = color;
}

void update_graphics(void)
{
	// Just an approximation for directional lighting
	Vector3 light_pos = scale(light_dir, 8);

	/*
	 * First render to depth map
	 */
	Matrix4 light_space_matrix;
	{
		glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
		glBindFramebuffer(GL_FRAMEBUFFER, depth_map_fbo);
		glClear(GL_DEPTH_BUFFER_BIT);

		Matrix4 view = lookat_matrix(light_pos, (Vector3) {0, 0, 0}, (Vector3) {0, 1, 0});
		Matrix4 projection = ortho_matrix(-2, 18, -10, 10, 1, 20);
		light_space_matrix = dotm(projection, view);

		glUseProgram(shadow_program);
		set_uniform_m4(shadow_program, "light_space_matrix", light_space_matrix);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, depth_map);

		apply_commands(true);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	int w, h;
	glfwGetWindowSize(window_, &w, &h);
	glViewport(0, 0, w, h);

	glClearColor(0.2f, 0.0f, 0.0f, 1.0f);
	glClearStencil(0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	Matrix4 view = camera_pov();
	Matrix4 projection = perspective_matrix(deg2rad(30.0f), (float) w / (float) h, 0.1f, 1000.0f);

	{
		glUseProgram(shader_program);
		set_uniform_v3(shader_program, "lightDir", light_dir);
		set_uniform_v3(shader_program, "lightColor", light_color);
		set_uniform_v3(shader_program, "viewPos", get_camera_pos());
		set_uniform_i(shader_program, "irradianceMap", 0);
		set_uniform_i(shader_program, "prefilterMap", 1);
		set_uniform_i(shader_program, "brdfLUT", 2);
		set_uniform_m4(shader_program, "view", view);
		set_uniform_m4(shader_program, "projection", projection);
		set_uniform_m4(shader_program, "light_space_matrix", light_space_matrix);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_CUBE_MAP, irradianceMap);

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_CUBE_MAP, prefilterMap);

		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, brdfLUTTexture);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, depth_map);

		apply_commands(false);
	}

	{
		glUseProgram(background_program);
		set_uniform_m4(background_program, "view", view);
		glUniform1i(glGetUniformLocation(background_program, "environmentMap"), 0);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);
		//glBindTexture(GL_TEXTURE_CUBE_MAP, prefilterMap);
		//glBindTexture(GL_TEXTURE_CUBE_MAP, irradianceMap); // display irradiance map
		renderCube();
	}

	clear_commands();
}
