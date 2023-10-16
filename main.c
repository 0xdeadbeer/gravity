#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <GL/glew.h>
#include <GL/freeglut.h>
#include <cglm/cglm.h>
#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

unsigned int vao;
unsigned int vbo;
unsigned int ebo;
unsigned int nbo;
unsigned int shader_program;
unsigned int vertex_shader;
unsigned int fragment_shader;

// shaders
const char *vertex_shader_location = "assets/shaders/shader.vert";
const char *fragment_shader_location = "assets/shaders/shader.frag";

// GPU data
float *vertices = NULL;
unsigned int *indices = NULL;
float *normals = NULL;
long vertices_num = 0;
long indices_num = 0;
long normals_num = 0;

// Camera / LookAt
vec3 camera_position;
vec3 world_origin;
vec3 up;
vec3 right;
vec3 forward;

// structs
struct object {
    mat4 rotation_matrix;
    mat4 translation_matrix;
    vec3 translation_force;
    vec3 rotation_force;
    float mass;
    void *next;

    float *vertices;
    unsigned int *indices;
    float *normals;
    long vertices_num;
    long indices_num;
    long normals_num;

    unsigned int vao;
    unsigned int vbo; // buffer for vertices
    unsigned int ebo; // buffer for indices
    unsigned int nbo; // buffer for normals
};

// global objects information
struct object* objects = NULL;

int load_shader(const char *path, unsigned int shader) {
    FILE *fp = fopen(path, "r");
    int len = 0;
    char *ftext;

    if (fp == NULL) {
        fprintf(stderr, "Error: Cannot open file '%s'\n", path);
        return -1;
    }

    fseek(fp, 0L, SEEK_END);
    len = ftell(fp);

    if (len == -1) {
        fprintf(stderr, "Error: Cannot fetch length of file '%s'\n", path);
        return -1;
    }

    fseek(fp, 0L, SEEK_SET);

    ftext = (char *) malloc(len);
    if (ftext == NULL) {
        fprintf(stderr, "Error: Cannot allocate enough memory for file's contents '%s'\n", path);
        return -1;
    }

    fread(ftext, sizeof(char), len, fp);
    fclose(fp);

    glShaderSource(shader, 1, (const char **) &ftext,  &len);
    glCompileShader(shader);

    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

    if (success != GL_TRUE) {
        int log_length;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);

        char log[log_length];
        glGetShaderInfoLog(shader, log_length, NULL, log);

        fprintf(stderr, "Shader Compilation Error: %s\n", log);
        return -1;
    }

    // RUD
    free(ftext);

    return 0;
}

int load_model_to_object(const char *path, struct object *obj) {
    const struct aiScene *scene = aiImportFile(path, aiProcess_Triangulate);

    if (scene == NULL) {
        return -1;
    }

    for (int mesh_index = 0; mesh_index < scene->mNumMeshes; mesh_index++) {
        struct aiMesh *mesh = scene->mMeshes[mesh_index];
        fprintf(stdout, "Number of vertices in mesh %d: %d\n", mesh_index, mesh->mNumVertices);

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

int load_shaders() {
    glDeleteProgram(shader_program);
    shader_program = glCreateProgram();

    // create and load new shaders
    vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);

    if (load_shader(vertex_shader_location, vertex_shader) == -1) {
        return -1;
    }

    if (load_shader(fragment_shader_location, fragment_shader) == -1) {
        return -1;
    }

    glAttachShader(shader_program, vertex_shader);
    glAttachShader(shader_program, fragment_shader);
    glLinkProgram(shader_program);

    int success;
    glGetProgramiv(shader_program, GL_LINK_STATUS, &success);

    if (success != GL_TRUE) {
        int log_length;
        glGetProgramiv(shader_program, GL_INFO_LOG_LENGTH, &log_length);

        char log[log_length];
        glGetProgramInfoLog(shader_program, log_length, NULL, log);

        fprintf(stderr, "Shader Compilation Error: %s\n", log);
        return -1;
    }

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    return 0;
}

void display() {
    mat4 view;
    mat4 projection;
    vec3 view_translate = {0.0f, 0.0f, -3.0f};
    GLint viewport[4]; // viewport: x,y,width,height

    GLint translation_uniform;
    GLint rotation_uniform;
    GLint view_uniform;
    GLint projection_uniform;

    glClearColor(0.13f, 0.13f, 0.13f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glGetIntegerv(GL_VIEWPORT, viewport);
    glUseProgram(shader_program);

    glm_mat4_identity(view);
    glm_translate(view, view_translate);

    glm_mat4_identity(projection);
    glm_perspective(glm_rad(45.0f), (float) viewport[2]/(float) viewport[3], 0.01f, 100.0f, projection);

    view_uniform = glGetUniformLocation(shader_program, "view");
    projection_uniform = glGetUniformLocation(shader_program, "projection");
    translation_uniform = glGetUniformLocation(shader_program, "translation");
    rotation_uniform = glGetUniformLocation(shader_program, "rotation");

    glUniformMatrix4fv(view_uniform, 1, GL_FALSE, (float *) view);
    glUniformMatrix4fv(projection_uniform, 1, GL_FALSE, (float *) projection);

    for (struct object *obj = objects; obj != NULL; obj = obj->next) {
        glm_translate(obj->translation_matrix, obj->translation_force);
        glUniformMatrix4fv(translation_uniform, 1, GL_FALSE, (float *) obj->translation_matrix);
        glUniformMatrix4fv(rotation_uniform, 1, GL_FALSE, (float *) obj->rotation_matrix);

        glBindVertexArray(obj->vao);
        glDrawElements(GL_TRIANGLES, obj->indices_num, GL_UNSIGNED_INT, (void *) 0);
    }

    glutSwapBuffers();
    glutPostRedisplay();
}

void keyboard(unsigned char key, int x, int y) {
    switch (key) {
        case '\x1B':
        {
            exit(EXIT_SUCCESS);
            break;
        }
        case 'r':
        case 'R':
            if (load_shaders() != 0) {
                fprintf(stderr, "Error: reloading shaders\n");
                exit(EXIT_FAILURE);
            }

            fprintf(stdout, "Status: successfully reloaded shaders\n");

            break;
        default:
            break;
    }
}

void setup() {
    for (struct object *obj = objects; obj != NULL; obj = obj->next) {
        glGenVertexArrays(1, &obj->vao);
        glGenBuffers(1, &obj->vbo);
        glGenBuffers(1, &obj->ebo);
        glGenBuffers(1, &obj->nbo);

        glBindVertexArray(obj->vao);

        glBindBuffer(GL_ARRAY_BUFFER,obj->vbo);
        glBufferData(GL_ARRAY_BUFFER,obj->vertices_num*3*sizeof(float),obj->vertices, GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), (void *) 0);
        glEnableVertexAttribArray(0);

        glBindBuffer(GL_ARRAY_BUFFER, obj->nbo);
        glBufferData(GL_ARRAY_BUFFER, obj->normals_num*3*sizeof(float), obj->normals, GL_STATIC_DRAW);

        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), (void *) 0);
        glEnableVertexAttribArray(1);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,obj->ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,obj->indices_num*sizeof(unsigned int),obj->indices, GL_STATIC_DRAW);

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }

    glEnable(GL_DEPTH_TEST);
}

struct object *create_object(float mass, const char *model) {
    struct object *new_object = (struct object *) malloc(sizeof(struct object));

    if (new_object == NULL) {
        return NULL;
    }

    new_object->mass = mass;
    glm_mat4_identity(new_object->translation_matrix);
    glm_mat4_identity(new_object->rotation_matrix);
    new_object->vertices_num = 0;
    new_object->indices_num = 0;
    new_object->normals_num = 0;
    new_object->vertices = NULL;
    new_object->indices = NULL;
    new_object->normals = NULL;
    new_object->next = NULL;

    if (load_model_to_object(model, new_object) == -1) {
        return NULL;
    }

    if (objects == NULL) {
        objects = new_object;
        return new_object;
    }

    struct object *obj = objects;
    while (obj->next != NULL) {
        obj = obj->next;
    }

    obj->next = new_object;

    return new_object;
}

int main(int argc, char **argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE);
    glutCreateWindow("Simple Space Time Simulator");
    GLenum err = glewInit();
    if (err != GLEW_OK) {
        fprintf(stderr, "Error: %s\n", glewGetErrorString(err));
        return EXIT_FAILURE;
    }

    fprintf(stdout, "Status: using with GLEW %s\n", glewGetString(GLEW_VERSION));

    glutKeyboardFunc(&keyboard);
    glutDisplayFunc(&display);

    if (load_shaders() != 0) {
        fprintf(stderr, "Error: loading shaders\n");
        return EXIT_FAILURE;
    }

    // objects
    struct object *sphere = create_object(10, "assets/models/sphere.obj");
    struct object *kub = create_object(10, "assets/models/kub.obj");

    vec4 sphere_translate = {-8.0f, 2.0f, -10.0f};
    glm_translate(sphere->translation_matrix, sphere_translate);

    float force[] = {0.05f, -0.02f, 0.0f};
    glm_vec3_make(force, sphere->translation_force);

    vec4 kub_translate = {10.0f, -2.0f, -15.0f};
    glm_translate(kub->translation_matrix, kub_translate);

    vec3 kub_rotation_axis = {1.0f, 0.5f, 0.0f};
    glm_rotate(kub->rotation_matrix, glm_rad(45.0f), kub_rotation_axis);

    setup();

    glutMainLoop();

    return EXIT_SUCCESS;
}