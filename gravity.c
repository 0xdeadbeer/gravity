#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <GL/glew.h>
#include <GL/freeglut.h>
#include <cglm/cglm.h>
#include "math.h"
#include "object.h"

// global settings
float fov = 80.0f; // default fov
float fov_change = 1.0f;
vec3 camera_pos = { 0.0f, 0.0f, 0.0f };
vec3 camera_front = { 0.0f, 0.0f, -1.0f };
vec3 camera_up = { 0.0f, 1.0f, 0.0f }; 
struct object *camera_lock = NULL; // is camera locked to any object?
float camera_yaw = -90.0f; // x rotation
float camera_pitch = 0.0f; // y rotation
float camera_sensitivity = 0.01f;
float movement_speed = 2.0f;
GLint screen_viewport[4]; // viewport: x,y,width,height
int toggle_tracing = 0; // true or false

// opengl
unsigned int shader_program;
unsigned int vertex_shader;
unsigned int fragment_shader;

// shaders
const char *object_vertex_shader_location = "assets/shaders/shader.vert";
const char *object_fragment_shader_location = "assets/shaders/shader.frag";

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

    glutPostRedisplay();
    glutSwapBuffers();
}

void setup() {
    // setup default mouse position
    glGetIntegerv(GL_VIEWPORT, screen_viewport);

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
            vec3 side_scalar = { movement_speed, movement_speed, movement_speed };
            vec3 camera_side;
            glm_cross(camera_front, camera_up, camera_side);
            glm_normalize(camera_side);
            glm_vec3_mul(camera_side, side_scalar, camera_side);
            glm_vec3_add(camera_pos, camera_side, camera_pos);
            break;
        }
        case 's':
        case 'S': {
            vec3 front_scalar = { movement_speed, movement_speed, movement_speed};
            glm_vec3_mul(front_scalar, camera_front, front_scalar);
            glm_vec3_sub(camera_pos, front_scalar, camera_pos);
            break;
        }
        case 'w':
        case 'W': {
            vec3 front_scalar = { movement_speed, movement_speed, movement_speed };
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

int warped_pointer = 0;
int first_pointer = 1;
void mouse_motion(int x, int y) {
    if (warped_pointer == 1) {
        warped_pointer = 0;
        return;
    }

    warped_pointer = 1;
    glutWarpPointer((screen_viewport[2]/2), screen_viewport[3]/2);

    if (first_pointer == 1) {
        first_pointer = 0;
        return;
    }

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

    // scene setup
    struct object *a = create_object(100000000.0f, "assets/models/kub.obj");
    struct object *b = create_object(100000.0f, "assets/models/kub.obj");
    struct object *c = create_object(10000000.0f, "assets/models/sphere.obj");
    float distance = -500.0f;

    vec4 a_pos = {0.0f, 0.0f, distance, 0.0f};
    glm_vec4_add(a->position, a_pos, a->position);
    vec4 b_pos= {100.0f, 300.0f, distance, 0.0f};
    glm_vec4_add(b->position, b_pos, b->position);
    //vec4 a_pos = {0.0f, -0.0f, -150.0f, 0.0f};
    //glm_vec4_add(a->position, a_pos, a->position);

    // vec4 b_pos = {0.0f, -75.0f, -150.0f, 0.0f};
    // glm_vec4_add(b->position, b_pos, b->position);

    vec4 c_pos = {-100.0f, -400.0f, distance, 0.0f};
    glm_vec4_add(c->position, c_pos, c->position);

    float n = 0.05f;

    vec3 b_boost = {-70*n, 0.0f, 0.0f};

    glm_vec3_add(b->translation_force, b_boost , b->translation_force);
    glm_vec3_add(c->translation_force, b_boost , c->translation_force);

    // b->scale = 2.0f;
    a->scale = 5.0f;
    b->scale = 10.0f;

    setup();
    glutMainLoop();

    return EXIT_SUCCESS;
}
