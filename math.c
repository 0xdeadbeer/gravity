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
    vec4 v4distance;
    glm_vec4_sub(target->position, src->position, v4distance);

    vec3 v3distance;
    glm_vec3(v4distance, v3distance);

    float distance_xy = sqrt((v3distance[0] * v3distance[0]) + (v3distance[1] * v3distance[1]));
    float distance_xyz = sqrt((distance_xy * distance_xy) + (v3distance[2] * v3distance[2]));
    float force_scale = 4.0f;

    float g = 6.67f * 1e-11f;
    float top = g * src->mass * target->mass;

    for (int i = 0; i < 3; i++) {
        v3distance[i] = (v3distance[i] * v3distance[i] * v3distance[i]);
    }

    for (int i = 0; i < 3; i++) {
        if (v3distance[i] == 0) {
            force[i] = 0.0f;
            continue;
        }

        force[i] = (top / (distance_xyz / (target->position[i] - src->position[i]))) * force_scale;
    }
}

