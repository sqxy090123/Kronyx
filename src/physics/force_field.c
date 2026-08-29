#include "kronyx/force_field.h"
#include <math.h>

kyForceField ky_force_field_create(kyVec3 position, float strength, float radius, kyForceFieldType type) {
    kyForceField field = {0};
    field.position = position;
    field.strength = strength;
    field.radius = radius;
    field.type = type;
    return field;
}

/*
 * Force field calculation formulas:
 * 
 * Gravity (attractive):
 *   F = G * m1 * m2 / r^2  (toward source)
 * 
 * Repulsion:
 *   F = -G * m1 * m2 / r^2  (away from source)
 * 
 * With softening to prevent singularity at r=0:
 *   F = G * m1 * m2 * direction / (r^2 + epsilon^2)
 * 
 * With radius-based falloff:
 *   F = G * m1 * m2 * factor(r, radius) / r^2
 *   where factor = 1.0 inside radius, 0.0 outside
 */

static const float KY_FORCE_SOFTENING = 0.01f; /* Prevent division by zero */

float ky_force_field_calc_distance_factor(float distance, float radius) {
    if (radius <= 0.0f) return 1.0f; /* Infinite range */
    if (distance > radius) return 0.0f; /* Outside range */
    /* Smooth falloff near edge */
    float t = distance / radius;
    return 1.0f - t * t; /* Quadratic falloff */
}

void ky_force_field_apply(kyForceField *field, kyVec3 body_pos, float body_mass, kyVec3 *out_force) {
    if (!field || !out_force) return;
    
    /* Calculate direction from body to field source */
    kyVec3 to_source = ky_vec3_sub(field->position, body_pos);
    float dist_sq = ky_vec3_len_sq(to_source);
    float dist = sqrtf(dist_sq);
    
    if (dist < 1e-6f) {
        /* Body is at field center, no net force */
        *out_force = ky_vec3_zero();
        return;
    }
    
    /* Normalize direction */
    kyVec3 direction = ky_vec3_scale(to_source, 1.0f / dist);
    
    /* Calculate force magnitude */
    /* F = strength * body_mass / (dist^2 + softening) */
    float factor = ky_force_field_calc_distance_factor(dist, field->radius);
    float force_mag = field->strength * body_mass * factor / (dist_sq + KY_FORCE_SOFTENING);
    
    /* Apply direction based on field type */
    if (field->type == KY_FORCE_FIELD_REPULSION) {
        /* Repulsion: push away from source */
        direction = ky_vec3_scale(direction, -1.0f);
    }
    
    /* Calculate final force vector */
    *out_force = ky_vec3_scale(direction, force_mag);
    
    /* For vortex fields, apply tangential force */
    if (field->type == KY_FORCE_FIELD_VORTEX) {
        /* Tangent = cross(product of up vector and direction) */
        kyVec3 up = ky_vec3(0.0f, 1.0f, 0.0f);
        kyVec3 tangent = ky_vec3_cross(up, direction);
        tangent = ky_vec3_normalize(tangent);
        *out_force = ky_vec3_scale(tangent, force_mag);
    }
}
