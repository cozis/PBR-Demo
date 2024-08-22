#include "vector.h"

typedef struct {
    float perceptualRoughness;
    float metallic;
    float reflectance;
    Vector3 baseColor;
} Material;

typedef uint32_t ModelID;
#define MODEL_INVALID ((ModelID) 0)
#define MODEL_SPHERE  ((ModelID) 1)
#define MODEL_CUBE    ((ModelID) 2)

ModelID load_3d_model(const char *file);
void    free_3d_model(ModelID id);

void init_graphics(void *window);
void update_graphics(void);

void set_clear_color(Vector3 color);
void set_light(Vector3 dir, Vector3 color);
void show_environment(bool yes);

void draw_model(ModelID id, Vector3 pos, Vector3 scale, Vector3 rotate, Material mat);
void draw_sphere(float x, float y, float z, float radius, Material mat);
void draw_cube(float x, float y, float z, float w, float h, float d, Material mat);