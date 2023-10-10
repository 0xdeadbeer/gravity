#include <stdio.h>
#include <GL/glew.h>
#include <GL/freeglut.h>

int main(int argc, char **argv) {

    glutInit(&argc, argv);
    glutCreateWindow("Simple Space Time Simulator");
    GLenum err = glewInit();
    if (err != GLEW_OK) {
        fprintf(stderr, "Error: %s\n", glewGetErrorString(err));
        return -1;
    }

    fprintf(stdout, "Status: using GLEW %s\n", glewGetString(GLEW_VERSION));

    return 0;
}
