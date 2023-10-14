#include <stdio.h>
#include <GL/glew.h>
#include <GL/freeglut.h>
#include <unistd.h>
#include <cglm/cglm.h>

unsigned int vao;
unsigned int vbo;
unsigned int shader_program;
unsigned int vertex_shader;
unsigned int fragment_shader;

const char *vertex_shader_location = "../shader.vert";
const char *fragment_shader_location = "../shader.frag";

float vertices[] = {
        // positions         // colors
        0.5f, -0.5f, 0.0f,  1.0f, 0.0f, 0.0f,   // bottom right
        -0.5f, -0.5f, 0.0f,  0.0f, 1.0f, 0.0f,   // bottom left
        0.0f,  0.5f, 0.0f,  0.0f, 0.0f, 1.0f    // top
};

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

float degs = 0;

void display() {
    mat4 model;
    vec3 model_axis = {0.5f, 0.5f, 0.5f};
    mat4 view;
    vec3 view_translate = {0.0f, 0.0f, -3.0f};
    mat4 projection;
    GLint viewport[4]; // viewport: x, y, width, height

    GLint model_uniform;
    GLint view_uniform;
    GLint projection_uniform;

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glGetIntegerv(GL_VIEWPORT, viewport);

    glm_mat4_identity(model);

    glm_rotate(model, glm_rad((float) degs ), model_axis);
    degs += 0.01f;

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

    glDrawArrays(GL_TRIANGLES, 0, 3);

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

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    return 0;
}

void post_setup() {
    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*) 0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *) (3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

int main(int argc, char **argv) {
    glutInit(&argc, argv);
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
