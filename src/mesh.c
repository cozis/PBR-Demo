#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "mesh.h"

void append_vertex(VertexArray *array, Vertex v)
{
	if (array->size == array->capacity) {
		if (array->capacity == 0) {
			array->data = malloc(8 * sizeof(Vertex));
			array->capacity = 8;
		} else {
			array->data = realloc(array->data, 2 * array->capacity * sizeof(Vertex));
			array->capacity *= 2;
		}
		if (!array->data) {
			printf("OUT OF MEMORY\n");
			abort();
		}
	}
	array->data[array->size++] = v;
}

static Vector3 get_sphere_point(float angle_x, float angle_y, float radius)
{
    Vector3 p;
    p.x = radius * sin(angle_y) * cos(2 * angle_x);
    p.y = radius * cos(angle_y);
    p.z = radius * sin(angle_y) * sin(2 * angle_x);
    return p;
}

typedef struct {
    Vertex a;
    Vertex b;
    Vertex c;
} Triangle;

static void calculate_and_set_normals(Triangle *T)
{
    #define X1 (T->b.x - T->a.x)
    #define Y1 (T->b.y - T->a.y)
    #define Z1 (T->b.z - T->a.z)

    #define X2 (T->c.x - T->a.x)
    #define Y2 (T->c.y - T->a.y)
    #define Z2 (T->c.z - T->a.z)

    /*
     * xnormal = y1*z2 - z1*y2
     * ynormal = z1*x2 - x1*z2
     * znormal = x1*y2 - y1*x2
    */

    Vector3 n;
    n.x = Y1 * Z2 - Z1 * Y2;
    n.y = Z1 * X2 - X1 * Z2;
    n.z = X1 * Y2 - Y1 * X2;

    T->a.nx = n.x;
    T->a.ny = n.y;
    T->a.nz = n.z;

    T->b.nx = n.x;
    T->b.ny = n.y;
    T->b.nz = n.z;

    T->c.nx = n.x;
    T->c.ny = n.y;
    T->c.nz = n.z;

    #undef X1
    #undef Y1
    #undef Z1
    #undef X2
    #undef Y2
    #undef Z2
}

static Triangle make_triangle(Vector3 a, Vector3 b, Vector3 c)
{
    Triangle T;
    T.a = (Vertex) {a.x, a.y, a.z};
    T.b = (Vertex) {b.x, b.y, b.z};
    T.c = (Vertex) {c.x, c.y, c.z};
    calculate_and_set_normals(&T);
    return T;
}

VertexArray make_sphere_mesh_2(float radius, int num_segms, bool fake_normals)
{
    VertexArray vertices = {0, 0, 0};

    int x_num_segms = num_segms;
    int y_num_segms = num_segms;
    for (int i = 0; i < x_num_segms; i++)
        for (int j = 0; j < y_num_segms; j++) {

            int g = j;
            if (g == y_num_segms-1)
                g = 0;

            Vector3 p1 = get_sphere_point((i + 0) * 3.14 / x_num_segms, (g + 0) * 3.14 / y_num_segms, radius);
            Vector3 p2 = get_sphere_point((i + 1) * 3.14 / x_num_segms, (g + 0) * 3.14 / y_num_segms, radius);
            Vector3 p3 = get_sphere_point((i + 1) * 3.14 / x_num_segms, (g + 1) * 3.14 / y_num_segms, radius);
            Vector3 p4 = get_sphere_point((i + 0) * 3.14 / x_num_segms, (g + 1) * 3.14 / y_num_segms, radius);

            Triangle t1 = make_triangle(p1, p2, p3);

            if (fake_normals) {
                t1.a.nx = p1.x;
                t1.a.ny = p1.y;
                t1.a.nz = p1.z;

                t1.b.nx = p2.x;
                t1.b.ny = p2.y;
                t1.b.nz = p2.z;

                t1.c.nx = p3.x;
                t1.c.ny = p3.y;
                t1.c.nz = p3.z;
            }

			append_vertex(&vertices, t1.a);
			append_vertex(&vertices, t1.b);
			append_vertex(&vertices, t1.c);

            Triangle t2 = make_triangle(p4, p1, p3);

            if (fake_normals) {
                t2.a.nx = p4.x;
                t2.a.ny = p4.y;
                t2.a.nz = p4.z;

                t2.b.nx = p1.x;
                t2.b.ny = p1.y;
                t2.b.nz = p1.z;

                t2.c.nx = p3.x;
                t2.c.ny = p3.y;
                t2.c.nz = p3.z;
            }

			append_vertex(&vertices, t2.a);
			append_vertex(&vertices, t2.b);
			append_vertex(&vertices, t2.c);
        }
    return vertices;
}

VertexArray make_sphere_mesh(float radius)
{
	return make_sphere_mesh_2(radius, 64, true);
}

VertexArray make_cube_mesh(void)
{
    float vertices[] = {

        1.0, 1.0, 0.0,   0.0, 0.0, -1.0,
        1.0, 0.0, 0.0,   0.0, 0.0, -1.0,
        0.0, 0.0, 0.0,   0.0, 0.0, -1.0,
        0.0, 1.0, 0.0,   0.0, 0.0, -1.0,
        1.0, 1.0, 0.0,   0.0, 0.0, -1.0,
        0.0, 0.0, 0.0,   0.0, 0.0, -1.0,

        0.0, 0.0, 1.0,   0.0, 0.0, 1.0,
        1.0, 0.0, 1.0,   0.0, 0.0, 1.0,
        1.0, 1.0, 1.0,   0.0, 0.0, 1.0,
        0.0, 0.0, 1.0,   0.0, 0.0, 1.0,
        1.0, 1.0, 1.0,   0.0, 0.0, 1.0,
        0.0, 1.0, 1.0,   0.0, 0.0, 1.0,

        0.0, 0.0, 0.0,   0.0, -1.0, 0.0,
        1.0, 0.0, 0.0,   0.0, -1.0, 0.0,
        1.0, 0.0, 1.0,   0.0, -1.0, 0.0,
        0.0, 0.0, 0.0,   0.0, -1.0, 0.0,
        1.0, 0.0, 1.0,   0.0, -1.0, 0.0,
        0.0, 0.0, 1.0,   0.0, -1.0, 0.0,

        1.0, 1.0, 1.0,   0.0, 1.0, 0.0,
        1.0, 1.0, 0.0,   0.0, 1.0, 0.0,
        0.0, 1.0, 0.0,   0.0, 1.0, 0.0,
        0.0, 1.0, 1.0,   0.0, 1.0, 0.0,
        1.0, 1.0, 1.0,   0.0, 1.0, 0.0,
        0.0, 1.0, 0.0,   0.0, 1.0, 0.0,

        0.0, 1.0, 1.0,  -1.0, 0.0, 0.0,
        0.0, 1.0, 0.0,  -1.0, 0.0, 0.0,
        0.0, 0.0, 0.0,  -1.0, 0.0, 0.0,

        0.0, 0.0, 1.0,  -1.0, 0.0, 0.0,
        0.0, 1.0, 1.0,  -1.0, 0.0, 0.0,
        0.0, 0.0, 0.0,  -1.0, 0.0, 0.0,

        1.0, 0.0, 0.0,  1.0, 0.0, 0.0,
        1.0, 1.0, 0.0,  1.0, 0.0, 0.0,
        1.0, 1.0, 1.0,  1.0, 0.0, 0.0,
        1.0, 0.0, 0.0,  1.0, 0.0, 0.0,
        1.0, 1.0, 1.0,  1.0, 0.0, 0.0,
        1.0, 0.0, 1.0,  1.0, 0.0, 0.0,
    };

   VertexArray result = {0, 0, 0}; 
   for (int i = 0; i < (int) (sizeof(vertices)/sizeof(vertices[0])); i += 6) {
        Vertex v;
        v.x = vertices[i + 0];
        v.y = vertices[i + 1];
        v.z = vertices[i + 2];
        v.nx = vertices[i + 3];
        v.ny = vertices[i + 4];
        v.nz = vertices[i + 5];
        v.tx = 0;
        v.ty = 0;
		append_vertex(&result, v);
   }

   return result;
}
