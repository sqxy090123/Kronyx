#include "kronyx/kronyx.h"
#include "kytest.h"

int ky_test_failures = 0;
int ky_test_assertions = 0;

static void test_vec3(void) {
    kyVec3 a = ky_vec3(1, 2, 3);
    kyVec3 b = ky_vec3(4, 5, 6);
    kyVec3 s = ky_vec3_sub(a, b);
    KY_CHECK(s.x == -3.0f && s.y == -3.0f && s.z == -3.0f);
    KY_CHECK_NEAR(ky_vec3_len(a), sqrtf(14.0f), 1e-5f);
    kyVec3 n = ky_vec3_normalize(a);
    KY_CHECK_NEAR(ky_vec3_len(n), 1.0f, 1e-5f);
    kyVec3 x = ky_vec3(1, 0, 0);
    kyVec3 y = ky_vec3(0, 1, 0);
    kyVec3 z = ky_vec3_cross(x, y);
    KY_CHECK_NEAR(z.z, 1.0f, 1e-6f);
    KY_CHECK_NEAR(ky_vec3_dot(x, y), 0.0f, 1e-6f);
}

static void test_quat(void) {
    kyVec3 z_axis = ky_vec3(0, 0, 1);
    kyQuat q90 = ky_quat_axis_angle(z_axis, (float)(KY_PI / 2.0));
    kyVec3 x = ky_vec3(1, 0, 0);
    kyVec3 r = ky_quat_rotate(q90, x);
    KY_CHECK_NEAR(r.x, 0.0f, 1e-5f);
    KY_CHECK_NEAR(r.y, 1.0f, 1e-5f);
    KY_CHECK_NEAR(r.z, 0.0f, 1e-5f);
    kyQuat qa = ky_quat_axis_angle(z_axis, 0.3f);
    kyQuat qb = ky_quat_axis_angle(z_axis, -0.3f);
    kyQuat qr = ky_quat_mul(qa, qb);
    KY_CHECK_NEAR(qr.w, 1.0f, 1e-5f);
}

static void test_mat4(void) {
    kyMat4 id = ky_mat4_identity();
    kyVec4 v = ky_vec4(1, 2, 3, 1);
    kyVec4 r = ky_mat4_mul_vec4(&id, v);
    KY_CHECK_NEAR(r.x, 1.0f, 1e-6f);
    KY_CHECK_NEAR(r.w, 1.0f, 1e-6f);

    kyMat4 tr = ky_mat4_translate(ky_vec3(10, 20, 30));
    kyVec3 p = ky_mat4_mul_point(&tr, ky_vec3(1, 1, 1));
    KY_CHECK_NEAR(p.x, 11.0f, 1e-5f);
    KY_CHECK_NEAR(p.y, 21.0f, 1e-5f);
    KY_CHECK_NEAR(p.z, 31.0f, 1e-5f);

    kyVec3 dir = ky_mat4_mul_dir(&tr, ky_vec3(1, 0, 0));
    KY_CHECK_NEAR(dir.x, 1.0f, 1e-6f);
    KY_CHECK_NEAR(dir.y, 0.0f, 1e-6f);

    kyMat4 prod = ky_mat4_mul(&tr, &id);
    kyVec3 p2 = ky_mat4_mul_point(&prod, ky_vec3(0, 0, 0));
    KY_CHECK_NEAR(p2.x, 10.0f, 1e-5f);

    kyMat4 inv = ky_mat4_inverse(&tr);
    kyVec3 back = ky_mat4_mul_point(&inv, ky_vec3(11, 21, 31));
    KY_CHECK_NEAR(back.x, 1.0f, 1e-4f);
    KY_CHECK_NEAR(back.y, 1.0f, 1e-4f);
    KY_CHECK_NEAR(back.z, 1.0f, 1e-4f);
}

static void test_perspective(void) {
    kyMat4 proj = ky_mat4_perspective((float)(KY_PI / 3.0), 16.0f / 9.0f, 0.1f, 100.0f);
    kyVec4 clip = ky_mat4_mul_vec4(&proj, ky_vec4(0, 0, -5, 1));
    KY_CHECK_NEAR(clip.x, 0.0f, 1e-5f);
    KY_CHECK(clip.w > 0.0f);
    float ndc_z = clip.z / clip.w;
    KY_CHECK(ndc_z > -1.0f && ndc_z < 1.0f);
}

static void test_look_at(void) {
    kyMat4 view = ky_mat4_look_at(ky_vec3(0, 0, 10), ky_vec3(0, 0, 0), ky_vec3(0, 1, 0));
    kyVec3 p = ky_mat4_mul_point(&view, ky_vec3(0, 0, 10));
    KY_CHECK_NEAR(p.x, 0.0f, 1e-5f);
    KY_CHECK_NEAR(p.y, 0.0f, 1e-5f);
    KY_CHECK_NEAR(p.z, 0.0f, 1e-5f);
}

static void test_ray_aabb(void) {
    kyAABB box;
    box.min = ky_vec3(-1, -1, -1);
    box.max = ky_vec3(1, 1, 1);
    kyVec3 o = ky_vec3(0, 0, 5);
    kyVec3 d = ky_vec3(0, 0, -1);
    kyVec3 inv_d = ky_vec3(1.0f / d.x, 1.0f / d.y, 1.0f / d.z);
    float t = 0;
    KY_CHECK(ky_ray_aabb(o, inv_d, 100.0f, &box, &t) == 1);
    KY_CHECK_NEAR(t, 4.0f, 1e-5f);
    kyVec3 o2 = ky_vec3(0, 10, 5);
    KY_CHECK(ky_ray_aabb(o2, inv_d, 100.0f, &box, &t) == 0);
}

static void test_lerp_clamp(void) {
    KY_CHECK_NEAR(ky_math_lerpf(0, 10, 0.5f), 5.0f, 1e-6f);
    KY_CHECK_NEAR(ky_math_clampf(15, 0, 10), 10.0f, 1e-6f);
    KY_CHECK_NEAR(ky_math_clampf(-5, 0, 10), 0.0f, 1e-6f);
    KY_CHECK_NEAR(ky_math_deg2rad(180.0f), (float)KY_PI, 1e-5f);
}

void ky_test_run_all(void) {
    test_vec3();
    test_quat();
    test_mat4();
    test_perspective();
    test_look_at();
    test_ray_aabb();
    test_lerp_clamp();
}

KY_TEST_MAIN()
