#include "math.h"
#include "object.h"
#include <cglm/cglm.h>

float frand48(void) {
    float number = (float) rand() / (float) (RAND_MAX + 1.0);
    float side = rand() % 2;
    if (side == 0) {
        number = -number;
    }

    return number;
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

