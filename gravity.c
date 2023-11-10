#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <string.h>
#include <GL/glew.h>
#include <GL/freeglut.h>
#include <cglm/cglm.h>
#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#define MAX_PATHS  1500

float frand48(void) {
    float number = (float) rand() / (float) (RAND_MAX + 1.0);
    float side = rand() % 2;
    if (side == 0) {
        number = -number;
    }

    return number;
}

// structs
struct object {
    vec4 translation_force;
    vec4 rotation_force;
    vec4 position;
    vec4 rotation;
    vec3 color;
    float mass;
    void *next;

    float *paths;
    int paths_num;
    int paths_max;

    float *vertices;
    unsigned int *indices;
    float *normals;
    long vertices_num;
    long indices_num;
    long normals_num;
    float scale;

    unsigned int vao; // array object for the actual object
    unsigned int vbo; // buffer for vertices
    unsigned int ebo; // buffer for indices
    unsigned int nbo; // buffer for normals

    unsigned int pvao; // array object for paths
    unsigned int pbo; // buffer for paths
};

float fov = 80.0f; // default fov
float fov_change = 1.0f;
vec3 camera_pos = { 0.0f, 0.0f, 100.0f };
vec3 camera_front = { 0.0f, 0.0f, -1.0f };
vec3 camera_up = { 0.0f, 1.0f, 0.0f }; 
struct object *camera_lock = NULL; // is camera locked to any object?
float camera_yaw; // x rotation
float camera_pitch; // y rotation
float camera_sensitivity = 0.01f;
float movement_speed = 2.0f;
float time_speed = 33.0f;
float time_speed_change = 1.0f;

GLint screen_viewport[4]; // viewport: x,y,width,height

int toggle_tracing = 0; // true or false

unsigned int shader_program;
unsigned int vertex_shader;
unsigned int fragment_shader;

// shaders
const char *object_vertex_shader_location = "assets/shaders/shader.vert";
const char *object_fragment_shader_location = "assets/shaders/shader.frag";

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

    if (load_shader(object_vertex_shader_location, vertex_shader) == -1) {
        return -1;
    }

    if (load_shader(object_fragment_shader_location, fragment_shader) == -1) {
        return -1;
    }

    // compile object shader program
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

        fprintf(stderr, "[object program] Shader Compilation Error: %s\n", log);
        return -1;
    }

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    return 0;
}

void calculate_gravity(struct object *src, struct object *target, vec3 force) {
    vec4 tmp;
    glm_vec4_sub(target->position, src->position, tmp);

    vec3 distance;
    glm_vec3(tmp, distance);

    float distance_xy = sqrt((distance[0] * distance[0]) + (distance[1] * distance[1]));
    float distance_xyz = sqrt((distance_xy * distance_xy) + (distance[2] * distance[2]));
    float force_scale = 4.0f;

    float g = 6.67f * 1e-11f;
    float top = g * src->mass * target->mass;

    for (int i = 0; i < 3; i++) {
        distance[i] = (distance[i] * distance[i] * distance[i]);
    }

    for (int i = 0; i < 3; i++) {
        if (distance[i] == 0) {
            force[i] = 0.0f;
            continue;
        }

        force[i] = (top / (distance_xyz / (target->position[i] - src->position[i]))) * force_scale;
    }
}

// records the latest obj position to the path ring
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
        return 0;
    }

    // pop first element
    memmove(obj->paths, &obj->paths[3], (obj->paths_num)*3*sizeof(float));

    return 0;
}



void display() {
    mat4 view;
    mat4 projection;

    GLint translation_uniform;
    GLint view_uniform;
    GLint projection_uniform;
    GLint color_uniform;
    GLint scale_uniform;

    glClearColor(0.13f, 0.13f, 0.13f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glGetIntegerv(GL_VIEWPORT, screen_viewport);
    glUseProgram(shader_program);

    glm_mat4_identity(view);
    vec3 camera_center;
    glm_vec3_add(camera_pos, camera_front, camera_center);
    glm_lookat(camera_pos, camera_center, camera_up, view);

    glm_mat4_identity(projection);
    glm_perspective(glm_rad(fov), (float) screen_viewport[2]/(float) screen_viewport[3], 0.01f, 10000.0f, projection);

    view_uniform = glGetUniformLocation(shader_program, "view");
    projection_uniform = glGetUniformLocation(shader_program, "projection");
    translation_uniform = glGetUniformLocation(shader_program, "translation");
    color_uniform = glGetUniformLocation(shader_program, "color");
    scale_uniform = glGetUniformLocation(shader_program, "scale");

    glUniformMatrix4fv(view_uniform, 1, GL_FALSE, (float *) view);
    glUniformMatrix4fv(projection_uniform, 1, GL_FALSE, (float *) projection);

    for (struct object *obj = objects; obj != NULL; obj = obj->next) {
        mat4 translation_matrix;
        glm_mat4_identity(translation_matrix);

        // calculate gravity
        for (struct object *target = objects; target != NULL; target = target->next) {
            if (target == obj) {
                continue;
            }

            vec3 force;
            glm_vec3_zero(force);
            calculate_gravity(obj, target, force);

            vec4 force_new;
            for (int i = 0; i < 3; i++) {
                force_new[i] = force[i];
            }
            force_new[3] = 0.0f;

            float n = obj->mass;
            vec4 scaler = {n,n,n,1.0f};
            glm_vec4_div(force_new, scaler, force_new);

            glm_vec4_add(force_new, obj->translation_force, obj->translation_force);
        }

        glm_vec4_add(obj->position, obj->translation_force, obj->position);

        // follow object if camera locked 
        if (camera_lock == obj) {
            glm_vec3_add(camera_pos, obj->translation_force, camera_pos);
        }

        // record path
        if (toggle_tracing == 1) {
            if (record_path(obj) == -1) {
                exit(EXIT_FAILURE);
            }
        }

        glm_translate(translation_matrix, obj->position);

        glUniformMatrix4fv(translation_uniform, 1, GL_FALSE, (float *) translation_matrix);

        glUniform3fv(color_uniform, 1, (float *) obj->color);
        glUniform1f(scale_uniform, obj->scale);

        glBindVertexArray(obj->vao);
        glDrawElements(GL_TRIANGLES, obj->indices_num, GL_UNSIGNED_INT, (void *) 0);

        glBindVertexArray(obj->pvao);

        glBindBuffer(GL_ARRAY_BUFFER, obj->pbo);
        glBufferData(GL_ARRAY_BUFFER, obj->paths_num*3*sizeof(float),obj->paths, GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), (void *) 0);
        glEnableVertexAttribArray(0);

        glm_mat4_identity(translation_matrix);
        glUniformMatrix4fv(translation_uniform, 1, GL_FALSE, (float *) translation_matrix);
        glUniform1f(scale_uniform, 1.0f);

        glDrawArrays(GL_LINE_STRIP, 0, obj->paths_num);
    }

    glutSwapBuffers();
    glutPostRedisplay();
}

void setup() {
    // setup default mouse position
    glGetIntegerv(GL_VIEWPORT, screen_viewport);
    glutWarpPointer(screen_viewport[2]/2, screen_viewport[3]/2);

    for (struct object *obj = objects; obj != NULL; obj = obj->next) {
        glGenVertexArrays(1, &obj->vao);
        glGenVertexArrays(1, &obj->pvao);
        glGenBuffers(1, &obj->vbo);
        glGenBuffers(1, &obj->ebo);
        glGenBuffers(1, &obj->nbo);
        glGenBuffers(1, &obj->pbo);

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
    glm_vec4_one(new_object->position);
    glm_vec4_one(new_object->rotation);
    glm_vec4_zero(new_object->translation_force);
    glm_vec4_zero(new_object->rotation_force);
    new_object->vertices_num = 0;
    new_object->indices_num = 0;
    new_object->normals_num = 0;
    new_object->scale = 1.0f;
    new_object->vertices = NULL;
    new_object->indices = NULL;
    new_object->normals = NULL;
    new_object->next = NULL;
    new_object->paths = NULL;
    new_object->paths_num = 0;
    new_object->paths_max = MAX_PATHS;
    glm_vec3_one(new_object->color);

    // choose random color
    for (int i = 0; i < 3; i++) {
        new_object->color[i] = 0.5f + (fabs(frand48()) / 2);
    }

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
        case 'a':
        case 'A': {
            vec3 side_scalar = { movement_speed, movement_speed, movement_speed };
            vec3 camera_side;
            glm_cross(camera_front, camera_up, camera_side);
            glm_normalize(camera_side);
            glm_vec3_mul(camera_side, side_scalar, camera_side);
            glm_vec3_sub(camera_pos, camera_side, camera_pos);
            break;
        }
        case 'd':
        case 'D': {
            vec3 side_scalar = {movement_speed, movement_speed, movement_speed};
            vec3 camera_side;
            glm_cross(camera_front, camera_up, camera_side);
            glm_normalize(camera_side);
            glm_vec3_mul(camera_side, side_scalar, camera_side);
            glm_vec3_add(camera_pos, camera_side, camera_pos);
            break;
        }
        case 's':
        case 'S': {
            vec3 front_scalar = {movement_speed, movement_speed, movement_speed};
            glm_vec3_mul(front_scalar, camera_front, front_scalar);
            glm_vec3_sub(camera_pos, front_scalar, camera_pos);
            break;
        }
        case 'w':
        case 'W': {
            vec3 front_scalar = {movement_speed, movement_speed, movement_speed};
            glm_vec3_mul(front_scalar, camera_front, front_scalar);
            glm_vec3_add(camera_pos, front_scalar, camera_pos);
            break;
        }
        case 't':
        case 'T':
            toggle_tracing = !toggle_tracing;
            if (toggle_tracing == 0) {
                break;
            }

            // remove all the recorded paths of objects
            for (struct object *obj = objects; obj != NULL; obj = obj->next) {
                obj->paths_num=0;
                free(obj->paths);
                obj->paths = NULL;
            }
            break;
        case 'c':
        case 'C': {
            struct object *a = create_object(1000000.0f, "assets/models/sphere.obj");

            float n = 0.05f;
            vec4 a_pos = {frand48() * 100, frand48() * 100, -150.0f, 0.0f};
            glm_vec4_add(a->position, a_pos, a->position);

            //vec3 a_boost = {-10 * n, 0.0f, 0.0f};
            //glm_vec3_add(a->translation_force, a_boost, a->translation_force);
            setup();
            break;
        }
        default:
            break;
    }
}

void mouse(int button, int state, int x, int y) {
    switch (button) {
        case 3:
            if (fov-fov_change < 0.0f) {
                break;
            }

            fov -= fov_change;
            break;
        case 4:
            if (fov+fov_change > 180.0f) {
                break;
            }

            fov += fov_change;
            break;
        default:
            break;
    }
}

int warped_pointer = 1; 
void mouse_motion(int x, int y) {
    if (warped_pointer == 1) {
        warped_pointer = 0;
        return;
    }

    warped_pointer = 1;
    glutWarpPointer((screen_viewport[2]/2), screen_viewport[3]/2);
    float offset_x = (float) (x - (screen_viewport[2]/2)) * camera_sensitivity;
    float offset_y = (float) (y - (screen_viewport[3]/2)) * camera_sensitivity;

    camera_yaw += offset_x;
    camera_pitch -= offset_y;

    // limit view rotation
    if (camera_pitch < -89.9f) {
        camera_pitch = -89.9f;
    }
    if (camera_pitch > 89.9f) {
        camera_pitch = 89.9f;
    }

    vec3 view_direction;
    view_direction[0] = cos(glm_rad(camera_yaw)) * cos(glm_rad(camera_pitch));
    view_direction[1] = sin(glm_rad(camera_pitch));
    view_direction[2] = sin(glm_rad(camera_yaw)) * cos(glm_rad(camera_pitch));
    glm_normalize_to(view_direction, camera_front);
}

int main(int argc, char **argv) {
    srandom(time(NULL));

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE);
    glutCreateWindow("gravity");

    GLenum err = glewInit();
    if (err != GLEW_OK) {
        fprintf(stderr, "Error: %s\n", glewGetErrorString(err));
        return EXIT_FAILURE;
    }

    fprintf(stdout, "Status: using with GLEW %s\n", glewGetString(GLEW_VERSION));

    glutKeyboardFunc(&keyboard);
    glutMouseFunc(&mouse);
    glutPassiveMotionFunc(&mouse_motion);
    glutDisplayFunc(&display);

    if (load_shaders() != 0) {
        fprintf(stderr, "Error: loading shaders\n");
        return EXIT_FAILURE;
    }

    // objects
    struct object *a = create_object(1.0f, "assets/models/sphere.obj");
    struct object *b = create_object(1000000000.0f, "assets/models/sphere.obj");
    struct object *c = create_object(1.0f, "assets/models/sphere.obj");
    float distance = -200.0f;

    vec4 a_pos = {0.0f, 50.0f, distance, 0.0f};
    glm_vec4_add(a->position, a_pos, a->position);
    //vec4 a_pos = {0.0f, -0.0f, -150.0f, 0.0f};
    //glm_vec4_add(a->position, a_pos, a->position);

    vec4 b_pos = {0.0f, -75.0f, -150.0f, 0.0f};
    glm_vec4_add(b->position, b_pos, b->position);

    vec4 c_pos = {0.0f, -20.0f, distance, 0.0f};
    glm_vec4_add(c->position, c_pos, c->position);

    float n = 0.05f;

    vec3 b_boost = {10*n, 0.0f, 0.0f};

    glm_vec3_add(b->translation_force, b_boost , b->translation_force);

    b->scale = 2.0f;
    camera_lock = b;

    setup();

    glutMainLoop();

    return EXIT_SUCCESS;
}
