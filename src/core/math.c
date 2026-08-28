#include "kronyx/math.h"

kyVec3 ky_quat_rotate(kyQuat q, kyVec3 v) {
    kyQuat p = {v.x, v.y, v.z, 0.0f};
    kyQuat qi = ky_quat_conj(q);
    kyQuat t;
    t.x = q.w * p.x + q.x * p.w + q.y * p.z - q.z * p.y;
    t.y = q.w * p.y - q.x * p.z + q.y * p.w + q.z * p.x;
    t.z = q.w * p.z + q.x * p.y - q.y * p.x + q.z * p.w;
    t.w = q.w * p.w - q.x * p.x - q.y * p.y - q.z * p.z;
    kyQuat r;
    r.x = t.w * qi.x + t.x * qi.w + t.y * qi.z - t.z * qi.y;
    r.y = t.w * qi.y - t.x * qi.z + t.y * qi.w + t.z * qi.x;
    r.z = t.w * qi.z + t.x * qi.y - t.y * qi.x + t.z * qi.w;
    return ky_vec3(r.x, r.y, r.z);
}

kyQuat ky_quat_axis_angle(kyVec3 axis, float angle_rad) {
    float h = angle_rad * 0.5f;
    float s = sinf(h);
    kyQuat q = {axis.x * s, axis.y * s, axis.z * s, cosf(h)};
    return ky_quat_normalize(q);
}

kyQuat ky_quat_mul(kyQuat a, kyQuat b) {
    kyQuat q;
    q.x = a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y;
    q.y = a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x;
    q.z = a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w;
    q.w = a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z;
    return q;
}

kyMat4 ky_mat4_identity(void) {
    kyMat4 m;
    memset(m.m, 0, sizeof(m.m));
    m.m[0] = m.m[5] = m.m[10] = m.m[15] = 1.0f;
    return m;
}

kyMat4 ky_mat4_mul(const kyMat4 *a, const kyMat4 *b) {
    kyMat4 r;
    for (int c = 0; c < 4; ++c) {
        for (int row = 0; row < 4; ++row) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) {
                sum += a->m[k * 4 + row] * b->m[c * 4 + k];
            }
            r.m[c * 4 + row] = sum;
        }
    }
    return r;
}

kyMat4 ky_mat4_translate(kyVec3 t) {
    kyMat4 m = ky_mat4_identity();
    m.m[12] = t.x;
    m.m[13] = t.y;
    m.m[14] = t.z;
    return m;
}

kyMat4 ky_mat4_scale(kyVec3 s) {
    kyMat4 m = ky_mat4_identity();
    m.m[0] = s.x;
    m.m[5] = s.y;
    m.m[10] = s.z;
    return m;
}

kyMat4 ky_mat4_rotation(kyQuat q) {
    kyMat4 m;
    float x = q.x, y = q.y, z = q.z, w = q.w;
    float x2 = x + x, y2 = y + y, z2 = z + z;
    float xx = x * x2, xy = x * y2, xz = x * z2;
    float yy = y * y2, yz = y * z2, zz = z * z2;
    float wx = w * x2, wy = w * y2, wz = w * z2;
    m.m[0] = 1.0f - (yy + zz); m.m[4] = xy - wz;         m.m[8] = xz + wy;         m.m[12] = 0.0f;
    m.m[1] = xy + wz;         m.m[5] = 1.0f - (xx + zz); m.m[9] = yz - wx;         m.m[13] = 0.0f;
    m.m[2] = xz - wy;         m.m[6] = yz + wx;          m.m[10] = 1.0f - (xx + yy); m.m[14] = 0.0f;
    m.m[3] = 0.0f;            m.m[7] = 0.0f;             m.m[11] = 0.0f;           m.m[15] = 1.0f;
    return m;
}

kyMat4 ky_mat4_perspective(float fovy_rad, float aspect, float zn, float zf) {
    float f = 1.0f / tanf(fovy_rad * 0.5f);
    kyMat4 m = ky_mat4_identity();
    m.m[0] = f / aspect;
    m.m[5] = f;
    m.m[10] = (zf + zn) / (zn - zf);
    m.m[11] = -1.0f;
    m.m[14] = (2.0f * zf * zn) / (zn - zf);
    m.m[15] = 0.0f;
    return m;
}

kyMat4 ky_mat4_ortho(float left, float right, float bottom, float top, float zn, float zf) {
    kyMat4 m = ky_mat4_identity();
    m.m[0] = 2.0f / (right - left);
    m.m[5] = 2.0f / (top - bottom);
    m.m[10] = -2.0f / (zf - zn);
    m.m[12] = -(right + left) / (right - left);
    m.m[13] = -(top + bottom) / (top - bottom);
    m.m[14] = -(zf + zn) / (zf - zn);
    return m;
}

kyMat4 ky_mat4_look_at(kyVec3 eye, kyVec3 center, kyVec3 up) {
    kyVec3 f = ky_vec3_normalize(ky_vec3_sub(center, eye));
    kyVec3 s = ky_vec3_normalize(ky_vec3_cross(f, up));
    kyVec3 u = ky_vec3_cross(s, f);
    kyMat4 m;
    m.m[0] = s.x; m.m[4] = s.y; m.m[8] = s.z;  m.m[12] = -ky_vec3_dot(s, eye);
    m.m[1] = u.x; m.m[5] = u.y; m.m[9] = u.z;  m.m[13] = -ky_vec3_dot(u, eye);
    m.m[2] = -f.x; m.m[6] = -f.y; m.m[10] = -f.z; m.m[14] = ky_vec3_dot(f, eye);
    m.m[3] = 0.0f; m.m[7] = 0.0f; m.m[11] = 0.0f; m.m[15] = 1.0f;
    return m;
}

kyMat4 ky_mat4_inverse(const kyMat4 *m) {
    const float *a = m->m;
    float inv[16];
    float det;
    inv[0] = a[5] * a[10] * a[15] - a[5] * a[11] * a[14] - a[9] * a[6] * a[15]
           + a[9] * a[7] * a[14] + a[13] * a[6] * a[11] - a[13] * a[7] * a[10];
    inv[4] = -a[4] * a[10] * a[15] + a[4] * a[11] * a[14] + a[8] * a[6] * a[15]
            - a[8] * a[7] * a[14] - a[12] * a[6] * a[11] + a[12] * a[7] * a[10];
    inv[8] = a[4] * a[9] * a[15] - a[4] * a[11] * a[13] - a[8] * a[5] * a[15]
           + a[8] * a[7] * a[13] + a[12] * a[5] * a[11] - a[12] * a[7] * a[9];
    inv[12] = -a[4] * a[9] * a[14] + a[4] * a[10] * a[13] + a[8] * a[5] * a[14]
             - a[8] * a[6] * a[13] - a[12] * a[5] * a[10] + a[12] * a[6] * a[9];
    inv[1] = -a[1] * a[10] * a[15] + a[1] * a[11] * a[14] + a[9] * a[2] * a[15]
            - a[9] * a[3] * a[14] - a[13] * a[2] * a[11] + a[13] * a[3] * a[10];
    inv[5] = a[0] * a[10] * a[15] - a[0] * a[11] * a[14] - a[8] * a[2] * a[15]
           + a[8] * a[3] * a[14] + a[12] * a[2] * a[11] - a[12] * a[3] * a[10];
    inv[9] = -a[0] * a[9] * a[15] + a[0] * a[11] * a[13] + a[8] * a[1] * a[15]
            - a[8] * a[3] * a[13] - a[12] * a[1] * a[11] + a[12] * a[3] * a[9];
    inv[13] = a[0] * a[9] * a[14] - a[0] * a[10] * a[13] - a[8] * a[1] * a[14]
            + a[8] * a[2] * a[13] + a[12] * a[1] * a[10] - a[12] * a[2] * a[9];
    inv[2] = a[1] * a[6] * a[15] - a[1] * a[7] * a[14] - a[5] * a[2] * a[15]
           + a[5] * a[3] * a[14] + a[13] * a[2] * a[7] - a[13] * a[3] * a[6];
    inv[6] = -a[0] * a[6] * a[15] + a[0] * a[7] * a[14] + a[4] * a[2] * a[15]
            - a[4] * a[3] * a[14] - a[12] * a[2] * a[7] + a[12] * a[3] * a[6];
    inv[10] = a[0] * a[5] * a[15] - a[0] * a[7] * a[13] - a[4] * a[1] * a[15]
            + a[4] * a[3] * a[13] + a[12] * a[1] * a[7] - a[12] * a[3] * a[5];
    inv[14] = -a[0] * a[5] * a[14] + a[0] * a[6] * a[13] + a[4] * a[1] * a[14]
             - a[4] * a[2] * a[13] - a[12] * a[1] * a[6] + a[12] * a[2] * a[5];
    inv[3] = -a[1] * a[6] * a[11] + a[1] * a[7] * a[10] + a[5] * a[2] * a[11]
            - a[5] * a[3] * a[10] - a[9] * a[2] * a[7] + a[9] * a[3] * a[6];
    inv[7] = a[0] * a[6] * a[11] - a[0] * a[7] * a[10] - a[4] * a[2] * a[11]
           + a[4] * a[3] * a[10] + a[8] * a[2] * a[7] - a[8] * a[3] * a[6];
    inv[11] = -a[0] * a[5] * a[11] + a[0] * a[7] * a[9] + a[4] * a[1] * a[11]
             - a[4] * a[3] * a[9] - a[8] * a[1] * a[7] + a[8] * a[3] * a[5];
    inv[15] = a[0] * a[5] * a[10] - a[0] * a[6] * a[9] - a[4] * a[1] * a[10]
            + a[4] * a[2] * a[9] + a[8] * a[1] * a[6] - a[8] * a[2] * a[5];
    det = a[0] * inv[0] + a[1] * inv[4] + a[2] * inv[8] + a[3] * inv[12];
    kyMat4 r;
    if (fabsf(det) < 1e-12f) {
        return ky_mat4_identity();
    }
    det = 1.0f / det;
    for (int i = 0; i < 16; ++i) {
        r.m[i] = inv[i] * det;
    }
    return r;
}

kyVec4 ky_mat4_mul_vec4(const kyMat4 *m, kyVec4 v) {
    kyVec4 r;
    r.x = m->m[0] * v.x + m->m[4] * v.y + m->m[8] * v.z + m->m[12] * v.w;
    r.y = m->m[1] * v.x + m->m[5] * v.y + m->m[9] * v.z + m->m[13] * v.w;
    r.z = m->m[2] * v.x + m->m[6] * v.y + m->m[10] * v.z + m->m[14] * v.w;
    r.w = m->m[3] * v.x + m->m[7] * v.y + m->m[11] * v.z + m->m[15] * v.w;
    return r;
}

kyVec3 ky_mat4_mul_point(const kyMat4 *m, kyVec3 p) {
    kyVec4 v = {p.x, p.y, p.z, 1.0f};
    v = ky_mat4_mul_vec4(m, v);
    return ky_vec3(v.x, v.y, v.z);
}

kyVec3 ky_mat4_mul_dir(const kyMat4 *m, kyVec3 d) {
    kyVec4 v = {d.x, d.y, d.z, 0.0f};
    v = ky_mat4_mul_vec4(m, v);
    return ky_vec3(v.x, v.y, v.z);
}

int ky_ray_aabb(kyVec3 o, kyVec3 inv_d, float t_max, const kyAABB *b, float *out_t) {
    float tmin = (b->min.x - o.x) * inv_d.x;
    float tmax = (b->max.x - o.x) * inv_d.x;
    if (tmin > tmax) {
        float tmp = tmin;
        tmin = tmax;
        tmax = tmp;
    }
    float tymin = (b->min.y - o.y) * inv_d.y;
    float tymax = (b->max.y - o.y) * inv_d.y;
    if (tymin > tymax) {
        float tmp = tymin;
        tymin = tymax;
        tymax = tmp;
    }
    if (tmin > tymax || tymin > tmax) return 0;
    tmin = tymin > tmin ? tymin : tmin;
    tmax = tymax < tmax ? tymax : tmax;
    float tzmin = (b->min.z - o.z) * inv_d.z;
    float tzmax = (b->max.z - o.z) * inv_d.z;
    if (tzmin > tzmax) {
        float tmp = tzmin;
        tzmin = tzmax;
        tzmax = tmp;
    }
    if (tmin > tzmax || tzmin > tmax) return 0;
    tmin = tzmin > tmin ? tzmin : tmin;
    tmax = tzmax < tmax ? tzmax : tmax;
    if (tmin < 0.0f) tmin = 0.0f;
    if (tmin > t_max) return 0;
    *out_t = tmin;
    return 1;
}
