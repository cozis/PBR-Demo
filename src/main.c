#include <stdio.h>
#include <glad/glad.h>
//#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "camera.h"
#include "graphics.h"
#include "vector.h"
#include "mesh.h"

static void error_callback(int error, const char* description)
{
    fprintf(stderr, "Error: %s\n", description);
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GLFW_TRUE);
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

void cursor_pos_callback(GLFWwindow *window, double x, double y)
{
    rotate_camera(x, y);
}

int main(void)
{
    glfwSetErrorCallback(error_callback);

    if (!glfwInit())
        return -1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow *window = glfwCreateWindow(2*640, 2*480, "3D Chess", NULL, NULL);
    if (!window) {
        glfwTerminate();
        return -1;
    }

    glfwSetKeyCallback(window, key_callback);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, cursor_pos_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        printf("Failed to initialize GLAD\n");
        return -1;
    }

    glfwSwapInterval(1);

	init_graphics(window);

	set_light((Vector3) {0.6f, 1.0f, 0.3f}, (Vector3) {1, 1, 1});

    while (!glfwWindowShouldClose(window)) {

        float speed = 0.5;
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) move_camera(UP, speed);
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) move_camera(DOWN, speed);
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) move_camera(LEFT, speed);
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) move_camera(RIGHT, speed);

		int num_spheres = 8;
		for (int i = 0; i < num_spheres; i++) {
			for (int j = 0; j < num_spheres; j++) {
				for (int k = 0; k < num_spheres; k++) {
					Material mat;
					mat.baseColor = (Vector3) {0.1921, 0.4039, 0.5764};
					mat.metallic            = 1.0f * (float) k / (num_spheres-1);
					mat.reflectance         = 1.0f * (float) j / (num_spheres-1);
					mat.perceptualRoughness = 1.0f * (float) i / (num_spheres-1);
					float step = 1.3;
					draw_sphere(step * k, step * j, step * i, 1, mat);
				}
			}
		}

		update_graphics();
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
