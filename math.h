#ifndef MATH_H
#define MATH_H

#include "object.h"
#include <cglm/cglm.h>

float frand48(void);
void calculate_gravity(struct object *src, struct object *target, vec3 force);

#endif 
