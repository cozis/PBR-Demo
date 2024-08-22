#include <math.h>
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

typedef enum {
	PIECE_PAWN,
	PIECE_KING,
	PIECE_QUEEN,
	PIECE_ROOK,
	PIECE_BISHOP,
	PIECE_KNIGHT,
	PIECE_VOID, // Last
} PieceType;

typedef struct {
    bool is_black;
    PieceType type;
} Piece;

typedef struct {
    Piece pieces[8][8];
} Board;

void init_board(Board *board)
{
    for (int i = 0; i < 8; i++)
        for (int j = 0; j < 8; j++) {
            board->pieces[i][j] = (Piece) {false, PIECE_VOID};
        }
    for (int i = 0; i < 8; i++) {
        board->pieces[i][1] = (Piece) {true, PIECE_PAWN};
        board->pieces[i][6] = (Piece) {false, PIECE_PAWN};
    }

    board->pieces[3][0] = (Piece) {true, PIECE_KING};
    board->pieces[3][7] = (Piece) {false, PIECE_KING};
    board->pieces[4][0] = (Piece) {true, PIECE_QUEEN};
    board->pieces[4][7] = (Piece) {false, PIECE_QUEEN};

    board->pieces[2][7] = (Piece) {false, PIECE_BISHOP};
    board->pieces[5][7] = (Piece) {false, PIECE_BISHOP};

    board->pieces[2][0] = (Piece) {true, PIECE_BISHOP};
    board->pieces[5][0] = (Piece) {true, PIECE_BISHOP};

    board->pieces[1][7] = (Piece) {false, PIECE_KNIGHT};
    board->pieces[6][7] = (Piece) {false, PIECE_KNIGHT};

    board->pieces[1][0] = (Piece) {true, PIECE_KNIGHT};
    board->pieces[6][0] = (Piece) {true, PIECE_KNIGHT};

    board->pieces[0][0] = (Piece) {true, PIECE_ROOK};
    board->pieces[7][0] = (Piece) {true, PIECE_ROOK};

    board->pieces[0][7] = (Piece) {false, PIECE_ROOK};
    board->pieces[7][7] = (Piece) {false, PIECE_ROOK};

    /*
    board->pieces[3][4] = {false, PIECE_KING};
    board->pieces[4][5] = {false, PIECE_PAWN};
    board->pieces[4][6] = {false, PIECE_PAWN};
    */
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

	ModelID piece_models[] = {
		[PIECE_PAWN]   = load_3d_model("assets/pieces/pawn.obj"),
		[PIECE_BISHOP] = load_3d_model("assets/pieces/bishop.obj"),
		[PIECE_KING]   = load_3d_model("assets/pieces/king.obj"),
		[PIECE_KNIGHT] = load_3d_model("assets/pieces/knight.obj"),
		[PIECE_QUEEN]  = load_3d_model("assets/pieces/queen.obj"),
		[PIECE_ROOK]   = load_3d_model("assets/pieces/rook.obj"),
		[PIECE_VOID]   = MODEL_INVALID,
	};
	for (int i = 0; i < 6; i++)
		if (piece_models[i] == MODEL_INVALID) {
			printf("Couldn't load model\n");
			piece_models[i] = MODEL_SPHERE;
		}
	
	Board board;
	init_board(&board);

	show_environment(true);
	set_clear_color((Vector3) {0.2, 0.5, 0.1});

	while (!glfwWindowShouldClose(window)) {

		float speed = 0.5;
		if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) move_camera(UP, speed);
		if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) move_camera(DOWN, speed);
		if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) move_camera(LEFT, speed);
		if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) move_camera(RIGHT, speed);

		float cell_w = 1.5;
		float cell_d = 1.5;
		float board_h = 0.5;

		for (int i = 0; i < 8; i++)
			for (int j = 0; j < 8; j++) {
				
				Material material;
				if ((i + j) & 1) material = (Material) {.baseColor={0, 0, 0}, .metallic=0, .perceptualRoughness=0};
				else             material = (Material) {.baseColor={1, 1, 1}, .metallic=0, .perceptualRoughness=0};
				draw_cube(cell_w * i, -board_h, cell_d * j, cell_w, board_h, cell_d, material);

				Material piece_material = board.pieces[i][j].is_black
					? (Material) {.baseColor={0, 0, 0}, .metallic=0.0, .perceptualRoughness=0}
					: (Material) {.baseColor={1, 1, 1}, .metallic=0.0, .perceptualRoughness=0};

				float rotation = board.pieces[i][j].is_black ? -3.14/2 : 3.14/2;
				draw_model(piece_models[board.pieces[i][j].type], (Vector3) {cell_w * (i + 0.5), 0, cell_d * (j + 0.5)}, (Vector3) {1, 1, 1}, (Vector3) {0, rotation, 0}, piece_material);
			}

		update_graphics();
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
