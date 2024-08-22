#include "vector.h"

typedef struct {
    float perceptualRoughness;
    float metallic;
    float reflectance;
    Vector3 baseColor;
} Material;

void init_graphics(void *window);
void update_graphics(void);

void set_light(Vector3 dir, Vector3 color);
void draw_sphere(float x, float y, float z, float radius, Material mat);
void draw_cube(float x, float y, float z, float w, float h, float d, Material mat);
