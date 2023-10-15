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
const char *vertex_shader_location = "../assets/shaders/shader.vert";
const char *fragment_shader_location = "../assets/shaders/shader.frag";

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

int load_model(const char *path) {
    const struct aiScene *scene = aiImportFile(path, aiProcess_Triangulate);

    if (scene == NULL) {
        return -1;
    }

    // allocate enough memory
    vertices = (float *) malloc(1);

    for (int mesh_index = 0; mesh_index < scene->mNumMeshes; mesh_index++) {
        struct aiMesh *mesh = scene->mMeshes[mesh_index];
        fprintf(stdout, "Number of vertices in mesh %d: %d\n", mesh_index, mesh->mNumVertices);

        // fetch vertices
        for (int vertex_index = 0; vertex_index < mesh->mNumVertices; vertex_index++) {
            struct aiVector3D *vertex = &(mesh->mVertices[vertex_index]);
            long start = vertices_num*3;

            vertices_num++;
            vertices = (float *) realloc(vertices, vertices_num*3*sizeof(float));
            if (vertices == NULL) {
                return -1;
            }

            memcpy(&vertices[start], vertex, sizeof(float)*3);
        }

        // fetch indices
        for (int face_index = 0; face_index < mesh->mNumFaces; face_index++) {
            struct aiFace *face = &(mesh->mFaces[face_index]);
            long start = indices_num;

            indices_num += face->mNumIndices;
            indices = (unsigned int *) realloc(indices, sizeof(unsigned int)*indices_num);
            if (indices == NULL) {
                return -1;
            }

            memcpy(&indices[start], face->mIndices, sizeof(unsigned int)*face->mNumIndices);
        }

        // fetch normals
        for (int normal_index = 0; normal_index < mesh->mNumVertices; normal_index++) {
            struct aiVector3D *normal = &(mesh->mNormals[normal_index]);
            long start = normals_num*3;

            normals_num++;
            normals = (float *) realloc(normals, normals_num*3*sizeof(float));
            if (normals == NULL) {
                return -1;
            }

            memcpy(&normals[start], normal, sizeof(float)*3);
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

float deg = 0;

void display() {
    mat4 model;
    vec3 model_axis = {1.0f, 1.0f, 0.0f};
    mat4 view;
    vec3 view_translate = {0.0f, 0.0f, -10.0f};
    mat4 projection;
    GLint viewport[4]; // viewport: x, y, width, height

    GLint model_uniform;
    GLint view_uniform;
    GLint projection_uniform;

    glClearColor(0.13f, 0.13f, 0.13f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glGetIntegerv(GL_VIEWPORT, viewport);

    glm_mat4_identity(model);
    glm_rotate(model, glm_rad((float)deg), model_axis);
    deg += 1;

    model_uniform = glGetUniformLocation(shader_program, "model");
    glUniformMatrix4fv(model_uniform, 1, GL_FALSE, (float *) model);

    glm_mat4_identity(view);
    glm_translate(view, view_translate);

    view_uniform = glGetUniformLocation(shader_program, "view");
    glUniformMatrix4fv(view_uniform, 1, GL_FALSE, (float *) view);

    glm_mat4_identity(projection);
    glm_perspective(glm_rad(45.0f), (float)viewport[2]/(float)viewport[3], 0.01f, 100.0f, projection);

    projection_uniform = glGetUniformLocation(shader_program, "projection");
    glUniformMatrix4fv(projection_uniform, 1, GL_FALSE, (float *) projection);

    glUseProgram(shader_program);
    glBindVertexArray(vao);

    glDrawElements(GL_TRIANGLES, indices_num, GL_UNSIGNED_INT, (void *) 0);

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

int setup() {
    if (load_shaders() != 0) {
        fprintf(stderr, "Error: loading shaders\n");
        return -1;
    }

    if (load_model("assets/models/sphere.obj") == -1) {
        fprintf(stderr, "Error: loading model\n");
        return -1;
    }

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);
    glGenBuffers(1, &nbo);

    return 0;
}

void post_setup() {
    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, 3*vertices_num*sizeof(float), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*) 0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, nbo);
    glBufferData(GL_ARRAY_BUFFER, 3*normals_num*sizeof(float), normals, GL_STATIC_DRAW);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*) 0);
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices_num*sizeof(unsigned int), indices, GL_STATIC_DRAW);


    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glEnable(GL_DEPTH_TEST);
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

    if (setup() == -1) {
        fprintf(stderr, "Error: Failed to setup\n");
        return -1;
    }

    post_setup();

    glutMainLoop();

    return EXIT_SUCCESS;
}