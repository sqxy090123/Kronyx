#ifndef KRONYX_FORCE_FIELD_H
#define KRONYX_FORCE_FIELD_H

#include "defines.h"
#include "math.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Force field types */
typedef enum kyForceFieldType {
    KY_FORCE_FIELD_GRAVITY = 0,    /* Attractive: pulls bodies toward source */
    KY_FORCE_FIELD_REPULSION,      /* Repulsive: pushes bodies away from source */
    KY_FORCE_FIELD_VORTEX,         /* Tangential force (rotational) */
    KY_FORCE_FIELD_CUSTOM          /* User-defined force calculation */
} kyForceFieldType;

/* Force field descriptor */
typedef struct kyForceField {
    kyVec3             position;    /* Field center position */
    float              strength;    /* Force magnitude (positive = gravity, negative = repulsion) */
    float              radius;      /* Effective radius (0 = infinite) */
    kyForceFieldType   type;
    void              *user_data;   /* For custom force fields */
} kyForceField;

/* Maximum number of force fields per world */
#define KY_FORCE_FIELD_MAX 64

/* Force field API */
KY_API kyForceField ky_force_field_create(kyVec3 position, float strength, float radius, kyForceFieldType type);
KY_API void         ky_force_field_apply(kyForceField *field, kyVec3 body_pos, float body_mass, kyVec3 *out_force);
KY_API float        ky_force_field_calc_distance_factor(float distance, float radius);

#ifdef __cplusplus
}
#endif

#endif
