#include "vector.h"

typedef enum {
    UP, DOWN, LEFT, RIGHT,
} Direction;

Matrix4 camera_pov(void);
void move_camera(Direction dir, float speed);
void rotate_camera(double mouse_x, double mouse_y);
Vector3 get_camera_pos(void);