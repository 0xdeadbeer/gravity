#ifndef OBJECT_H
#define OBJECT_H 

#include <cglm/cglm.h>

#define MAX_PATHS 2000

struct model {
    float *vertices;
    unsigned int *indices;
    float *normals;

    long vertices_num;
    long indices_num;
    long normals_num;
};

struct object {
    vec4 translation_force;
    vec4 position;
    vec3 color;
    float mass;
    void *next;

    float *paths;
    int paths_num;
    int paths_max;

    struct model *model;
    float scale;

    unsigned int vao; // array object for the actual object
    unsigned int vbo; // buffer for vertices
    unsigned int ebo; // buffer for indices
    unsigned int nbo; // buffer for normals

    unsigned int pvao; // array object for paths
    unsigned int pbo; // buffer for paths
};

extern struct object *objects;

//int load_model_to_object(const char *path, struct object *obj);
struct model *load_model(const char *path);
int record_path(struct object *obj);
struct object *create_object(float mass, struct model *model);

#endif
