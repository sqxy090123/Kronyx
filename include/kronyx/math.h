#ifndef KRONYX_MATH_H
#define KRONYX_MATH_H

#include "defines.h"

typedef struct kyVec2 { float x, y; } kyVec2;
typedef struct kyVec3 { float x, y, z; } kyVec3;
typedef struct kyVec4 { float x, y, z, w; } kyVec4;
typedef struct kyQuat { float x, y, z, w; } kyQuat;
typedef struct kyMat4 { float m[16]; } kyMat4;
typedef struct kyAABB { kyVec3 min, max; } kyAABB;
typedef struct kySphere { kyVec3 center; float radius; } kySphere;

KY_INLINE float ky_math_minf(float a, float b) { return a < b ? a : b; }
KY_INLINE float ky_math_maxf(float a, float b) { return a > b ? a : b; }
KY_INLINE float ky_math_clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
KY_INLINE float ky_math_lerpf(float a, float b, float t) { return a + (b - a) * t; }
KY_INLINE float ky_math_deg2rad(float d) { return d * (float)KY_DEG2RAD; }
KY_INLINE float ky_math_rad2deg(float r) { return r * (float)KY_RAD2DEG; }
KY_INLINE float ky_math_absf(float a) { return a < 0.0f ? -a : a; }

KY_INLINE kyVec2 ky_vec2(float x, float y) { kyVec2 v = {x, y}; return v; }
KY_INLINE kyVec2 ky_vec2_add(kyVec2 a, kyVec2 b) { kyVec2 v = {a.x + b.x, a.y + b.y}; return v; }
KY_INLINE kyVec2 ky_vec2_sub(kyVec2 a, kyVec2 b) { kyVec2 v = {a.x - b.x, a.y - b.y}; return v; }
KY_INLINE kyVec2 ky_vec2_scale(kyVec2 a, float s) { kyVec2 v = {a.x * s, a.y * s}; return v; }
KY_INLINE float ky_vec2_dot(kyVec2 a, kyVec2 b) { return a.x * b.x + a.y * b.y; }
KY_INLINE float ky_vec2_len(kyVec2 a) { return sqrtf(ky_vec2_dot(a, a)); }

KY_INLINE kyVec3 ky_vec3(float x, float y, float z) { kyVec3 v = {x, y, z}; return v; }
KY_INLINE kyVec3 ky_vec3_zero(void) { kyVec3 v = {0, 0, 0}; return v; }
KY_INLINE kyVec3 ky_vec3_add(kyVec3 a, kyVec3 b) { kyVec3 v = {a.x + b.x, a.y + b.y, a.z + b.z}; return v; }
KY_INLINE kyVec3 ky_vec3_sub(kyVec3 a, kyVec3 b) { kyVec3 v = {a.x - b.x, a.y - b.y, a.z - b.z}; return v; }
KY_INLINE kyVec3 ky_vec3_scale(kyVec3 a, float s) { kyVec3 v = {a.x * s, a.y * s, a.z * s}; return v; }
KY_INLINE kyVec3 ky_vec3_mul(kyVec3 a, kyVec3 b) { kyVec3 v = {a.x * b.x, a.y * b.y, a.z * b.z}; return v; }
KY_INLINE float ky_vec3_dot(kyVec3 a, kyVec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
KY_INLINE kyVec3 ky_vec3_cross(kyVec3 a, kyVec3 b) {
    kyVec3 v = {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
    return v;
}
KY_INLINE float ky_vec3_len_sq(kyVec3 a) { return ky_vec3_dot(a, a); }
KY_INLINE float ky_vec3_len(kyVec3 a) { return sqrtf(ky_vec3_len_sq(a)); }
KY_INLINE kyVec3 ky_vec3_normalize(kyVec3 a) {
    float l = ky_vec3_len(a);
    if (l > 1e-8f) { return ky_vec3_scale(a, 1.0f / l); }
    return ky_vec3_zero();
}
KY_INLINE kyVec3 ky_vec3_lerp(kyVec3 a, kyVec3 b, float t) {
    kyVec3 v = {ky_math_lerpf(a.x, b.x, t), ky_math_lerpf(a.y, b.y, t), ky_math_lerpf(a.z, b.z, t)};
    return v;
}
KY_INLINE float ky_vec3_dist(kyVec3 a, kyVec3 b) { return ky_vec3_len(ky_vec3_sub(a, b)); }

KY_INLINE kyVec4 ky_vec4(float x, float y, float z, float w) { kyVec4 v = {x, y, z, w}; return v; }

KY_INLINE kyQuat ky_quat_identity(void) { kyQuat q = {0, 0, 0, 1}; return q; }
KY_INLINE kyQuat ky_quat_normalize(kyQuat q) {
    float l = sqrtf(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    if (l > 1e-12f) { kyQuat r = {q.x / l, q.y / l, q.z / l, q.w / l}; return r; }
    return ky_quat_identity();
}
KY_INLINE kyQuat ky_quat_conj(kyQuat q) { kyQuat r = {-q.x, -q.y, -q.z, q.w}; return r; }

KY_API kyVec3 ky_quat_rotate(kyQuat q, kyVec3 v);
KY_API kyQuat ky_quat_axis_angle(kyVec3 axis, float angle_rad);
KY_API kyQuat ky_quat_mul(kyQuat a, kyQuat b);

KY_API kyMat4 ky_mat4_identity(void);
KY_API kyMat4 ky_mat4_mul(const kyMat4 *a, const kyMat4 *b);
KY_API kyMat4 ky_mat4_translate(kyVec3 t);
KY_API kyMat4 ky_mat4_scale(kyVec3 s);
KY_API kyMat4 ky_mat4_rotation(kyQuat q);
KY_API kyMat4 ky_mat4_perspective(float fovy_rad, float aspect, float zn, float zf);
KY_API kyMat4 ky_mat4_ortho(float left, float right, float bottom, float top, float zn, float zf);
KY_API kyMat4 ky_mat4_look_at(kyVec3 eye, kyVec3 center, kyVec3 up);
KY_API kyMat4 ky_mat4_inverse(const kyMat4 *m);
KY_API kyVec4 ky_mat4_mul_vec4(const kyMat4 *m, kyVec4 v);
KY_API kyVec3 ky_mat4_mul_point(const kyMat4 *m, kyVec3 p);
KY_API kyVec3 ky_mat4_mul_dir(const kyMat4 *m, kyVec3 d);

KY_INLINE int ky_aabb_contains(const kyAABB *a, const kyVec3 *p) {
    return p->x >= a->min.x && p->x <= a->max.x &&
           p->y >= a->min.y && p->y <= a->max.y &&
           p->z >= a->min.z && p->z <= a->max.z;
}
KY_INLINE int ky_aabb_overlap(const kyAABB *a, const kyAABB *b) {
    return a->min.x <= b->max.x && a->max.x >= b->min.x &&
           a->min.y <= b->max.y && a->max.y >= b->min.y &&
           a->min.z <= b->max.z && a->max.z >= b->min.z;
}
KY_API int ky_ray_aabb(kyVec3 o, kyVec3 inv_d, float t_max, const kyAABB *b, float *out_t);

#endif
