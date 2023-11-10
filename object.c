#include "object.h"

#include "math.h"
#include <math.h>
#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

struct object *objects;

int load_model_to_object(const char *path, struct object *obj) {
    const struct aiScene *scene = aiImportFile(path, aiProcess_Triangulate);

    if (scene == NULL) {
        fprintf(stderr, "Error: failed importing file from path '%s'", path);
    }

    for (int mesh_index = 0; mesh_index < scene->mNumMeshes; mesh_index++) {
        struct aiMesh *mesh = scene->mMeshes[mesh_index];
        
        // fetch vertices
        for (int vertex_index = 0; vertex_index < mesh->mNumVertices; vertex_index++) {
            struct aiVector3D *vertex = &(mesh->mVertices[vertex_index]);
            long start = obj->vertices_num*3;

            obj->vertices_num++;
            obj->vertices = (float *) realloc(obj->vertices, obj->vertices_num*3*sizeof(float));
            if (obj->vertices == NULL) {
                return -1;
            }

            memcpy(&obj->vertices[start], vertex, sizeof(float)*3);
        }

        // fetch indices
        for (int face_index = 0; face_index < mesh->mNumFaces; face_index++) {
            struct aiFace *face = &(mesh->mFaces[face_index]);
            long start = obj->indices_num;

            obj->indices_num += face->mNumIndices;
            obj->indices = (unsigned int *) realloc(obj->indices, sizeof(unsigned int)*obj->indices_num);
            if (obj->indices == NULL) {
                return -1;
            }

            memcpy(&obj->indices[start], face->mIndices, sizeof(unsigned int)*face->mNumIndices);
        }

        // fetch normals
        for (int normal_index = 0; normal_index < mesh->mNumVertices; normal_index++) {
            struct aiVector3D *normal = &(mesh->mNormals[normal_index]);
            long start = obj->normals_num*3;

            obj->normals_num++;
            obj->normals = (float *) realloc(obj->normals,obj->normals_num*3*sizeof(float));
            if (obj->normals == NULL) {
                return -1;
            }

            memcpy(&obj->normals[start], normal, sizeof(float)*3);
        }
    }


    aiReleaseImport(scene);
    return 0;
}

int record_path(struct object *obj) {
    if (obj->paths_num <= obj->paths_max) {
        obj->paths = (float *) reallocarray(obj->paths, (obj->paths_num+1)*3, sizeof(float));
    }

    if (obj->paths == NULL) {
        fprintf(stderr, "Error: failed allocating memory for paths of object\n");
        return -1;
    }

    memcpy(obj->paths+(obj->paths_num*3), obj->position, 3*sizeof(float));

    if (obj->paths_num < obj->paths_max) {
        obj->paths_num++;
        goto end;
    }

    // pop first element 
    memmove(obj->paths, &obj->paths[3], (obj->paths_num)*3*sizeof(float));

end:
    return 0;
}

struct object *create_object(float mass, const char *model) {
    struct object *new_object = (struct object *) calloc(1, sizeof(struct object));

    if (new_object == NULL) {
        fprintf(stderr, "Error: failed allocating memory for a new object\n");
        goto error;
    }

    // initialize default values
    new_object->mass = mass;
    new_object->scale = 1.0f;
    new_object->paths_max = MAX_PATHS;
    glm_vec4_one(new_object->position);
    glm_vec4_one(new_object->rotation);
    glm_vec3_one(new_object->color);

    // choose random color
    for (int i = 0; i < 3; i++) {
        new_object->color[i] = 0.5f + (fabs(frand48()) / 2);
    }

    if (load_model_to_object(model, new_object) == -1) {
        fprintf(stderr, "Error: failed loading model '%s' to object", model);
        goto error;
    }

    if (objects == NULL) {
        objects = new_object;
        goto end;
    }

    struct object *object = objects;
    while (object->next != NULL) {
        object = object->next;
    }

    object->next = new_object;

end:
    return new_object;

error:
    return NULL;
}
