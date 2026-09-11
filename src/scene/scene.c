#include "kronyx/scene.h"
#include "kronyx/string.h"
#include "kronyx/file.h"
#include "kronyx/2d.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ── Public API ────────────────────────────────────────────────────── */

kyScene *ky_scene_create(kyAllocator *alloc, const char *name) {
    kyScene *s = (kyScene *)ky_mem_alloc(alloc, sizeof(kyScene));
    if (!s) return NULL;
    s->alloc = *alloc;
    s->world = ky_world_create(alloc);
    s->name  = NULL;
    if (name) {
        size_t n = strlen(name) + 1;
        s->name = (char *)ky_mem_alloc(alloc, n);
        if (s->name) memcpy(s->name, name, n);
    }
    ky_hashmap_init(&s->meta, alloc, 8);
    return s;
}

void ky_scene_destroy(kyScene *s) {
    if (!s) return;
    ky_world_destroy(s->world);
    ky_hashmap_deinit(&s->meta);
    ky_mem_free(&s->alloc, s->name);
    ky_mem_free(&s->alloc, s);
}

void ky_scene_set_meta(kyScene *s, const char *key, const char *value) {
    if (!s || !key) return;
    kyAllocator *al = &s->world->alloc;
    char *k = (char *)ky_mem_dup(al, key, strlen(key) + 1);
    char *v = (char *)ky_mem_dup(al, value, strlen(value) + 1);
    if (k && v) ky_hashmap_set_key(&s->meta, k, v);
    else { ky_mem_free(al, k); ky_mem_free(al, v); }
}

const char *ky_scene_get_meta(const kyScene *s, const char *key) {
    if (!s || !key) return NULL;
    return (const char *)ky_hashmap_get(&s->meta, key);
}

/* ── Save ──────────────────────────────────────────────────────────── */

int ky_scene_save(const kyScene *s, const char *path) {
    if (!s || !path) return -1;
    FILE *f = fopen(path, "w");
    if (!f) return -1;

    fprintf(f, "scene ");
    if (s->name) fprintf(f, "\"%s\" ", s->name);
    fprintf(f, "v1\n");

    for (size_t i = 0; i < s->meta.cap; ++i) {
        const kyHashEntry *h = &s->meta.entries[i];
        if (h->state == KY_HASHMAP_STATE_USED) {
            fprintf(f, "meta %s \"%s\"\n",
                    h->key ? h->key : "",
                    h->value ? (const char *)h->value : "");
        }
    }

    uint32_t tid_transform = (uint32_t)-1, tid_sprite = (uint32_t)-1,
             tid_camera    = (uint32_t)-1;
    for (uint32_t i = 0; ; ++i) {
        const kyComponentType *ct = ky_world_component_type(s->world, i);
        if (!ct) break;
        if (ct->name) {
            if (strcmp(ct->name, "transform") == 0 || strcmp(ct->name, "Transform") == 0)
                tid_transform = i;
            else if (strcmp(ct->name, "sprite") == 0 || strcmp(ct->name, "Sprite") == 0)
                tid_sprite = i;
            else if (strcmp(ct->name, "camera2d") == 0 || strcmp(ct->name, "Camera2D") == 0)
                tid_camera = i;
        }
    }

    int count = ky_world_alive_count(s->world);
    for (int ei = 0; ei < count; ++ei) {
        kyEntity e = ky_world_get_alive_entity(s->world, ei);
        if (!ky_entity_valid(s->world, e)) continue;
        if (!ky_world_has_component(s->world, e, tid_transform)) continue;

        fprintf(f, "\nentity \"%u(%u)\"\n", e.id, e.version);

        const kyTransform *tr = (const kyTransform *)
            ky_world_get_component(s->world, e, tid_transform);
        if (tr)
            fprintf(f, "  transform x=\"%.6g\" y=\"%.6g\" rotation_z=\"%.6g\""
                       " scale_x=\"%.6g\" scale_y=\"%.6g\" z=\"%.6g\"\n",
                    (double)tr->pos.x, (double)tr->pos.y,
                    (double)tr->rotation_z,
                    (double)tr->scale.x, (double)tr->scale.y,
                    (double)tr->z);

        if (tid_sprite != (uint32_t)-1 &&
            ky_world_has_component(s->world, e, tid_sprite)) {
            const kySprite *sp = (const kySprite *)
                ky_world_get_component(s->world, e, tid_sprite);
            if (sp)
                fprintf(f, "  sprite layer=\"%d\" flip=\"%d\"\n", sp->layer, sp->flip);
        }

        if (tid_camera != (uint32_t)-1 &&
            ky_world_has_component(s->world, e, tid_camera)) {
            const kyCamera2D *cam = (const kyCamera2D *)
                ky_world_get_component(s->world, e, tid_camera);
            if (cam)
                fprintf(f, "  camera2d x=\"%.6g\" y=\"%.6g\" rotation_z=\"%.6g\""
                           " zoom=\"%.6g\" viewport_w=\"%.6g\" viewport_h=\"%.6g\""
                           " clear_r=\"%.6g\" clear_g=\"%.6g\" clear_b=\"%.6g\" clear_a=\"%.6g\""
                           " active=\"%d\"\n",
                        (double)cam->pos.x, (double)cam->pos.y,
                        (double)cam->rotation_z, (double)cam->zoom,
                        (double)cam->viewport.x, (double)cam->viewport.y,
                        (double)cam->clear_color.x, (double)cam->clear_color.y,
                        (double)cam->clear_color.z, (double)cam->clear_color.w,
                        cam->active);
        }

        fprintf(f, "end\n");
    }

    fclose(f);
    return 0;
}

/* ── Load ──────────────────────────────────────────────────────────── */

static int ky_scene_reinit_world(kyScene *s) {
    ky_world_destroy(s->world);
    s->world = ky_world_create(&s->alloc);
    if (!s->world) return -2;
    return ky2d_register_components(s->world);
}

static void apply_attr(kyScene *s, kyEntity ent, uint32_t tid,
                        uint32_t ttid, uint32_t stid, uint32_t ctid,
                        const char *key, const char *val) {
    if (tid == (uint32_t)-1) return;
    if (tid == ttid) {
        kyTransform *tr = (kyTransform *)ky_world_get_component(s->world, ent, tid);
        if (!tr) return;
        if (strcmp(key, "x") == 0) tr->pos.x = (float)atof(val);
        else if (strcmp(key, "y") == 0) tr->pos.y = (float)atof(val);
        else if (strcmp(key, "rotation_z") == 0) tr->rotation_z = (float)atof(val);
        else if (strcmp(key, "scale_x") == 0) tr->scale.x = (float)atof(val);
        else if (strcmp(key, "scale_y") == 0) tr->scale.y = (float)atof(val);
        else if (strcmp(key, "z") == 0) tr->z = (float)atof(val);
    } else if (tid == stid) {
        kySprite *sp = (kySprite *)ky_world_get_component(s->world, ent, tid);
        if (!sp) return;
        if (strcmp(key, "layer") == 0) sp->layer = atoi(val);
        else if (strcmp(key, "flip") == 0) sp->flip = (uint8_t)atoi(val);
    } else if (tid == ctid) {
        kyCamera2D *cam = (kyCamera2D *)ky_world_get_component(s->world, ent, tid);
        if (!cam) return;
        if (strcmp(key, "x") == 0) cam->pos.x = (float)atof(val);
        else if (strcmp(key, "y") == 0) cam->pos.y = (float)atof(val);
        else if (strcmp(key, "rotation_z") == 0) cam->rotation_z = (float)atof(val);
        else if (strcmp(key, "zoom") == 0) cam->zoom = (float)atof(val);
        else if (strcmp(key, "viewport_w") == 0) cam->viewport.x = (float)atof(val);
        else if (strcmp(key, "viewport_h") == 0) cam->viewport.y = (float)atof(val);
        else if (strcmp(key, "clear_r") == 0) cam->clear_color.x = (float)atof(val);
        else if (strcmp(key, "clear_g") == 0) cam->clear_color.y = (float)atof(val);
        else if (strcmp(key, "clear_b") == 0) cam->clear_color.z = (float)atof(val);
        else if (strcmp(key, "clear_a") == 0) cam->clear_color.w = (float)atof(val);
        else if (strcmp(key, "active") == 0) cam->active = atoi(val);
    }
}

int ky_scene_load(kyScene *s, const char *path) {
    if (!s || !path) return -1;

    void *raw = NULL;
    size_t raw_len = 0;
    kyFileCode fc = ky_file_read(path, &raw, &raw_len);
    if (fc != KY_FILE_OK) return -1;

    kyAllocator saved = s->alloc;
    int r = ky_scene_reinit_world(s);
    if (r != 0) { free(raw); return (r < 0) ? r : -3; }

    uint32_t tid_transform = (uint32_t)-1, tid_sprite = (uint32_t)-1,
             tid_camera    = (uint32_t)-1;
    for (uint32_t i = 0; ; ++i) {
        const kyComponentType *ct = ky_world_component_type(s->world, i);
        if (!ct) break;
        if (ct->name) {
            if (strcmp(ct->name, "transform") == 0 || strcmp(ct->name, "Transform") == 0)
                tid_transform = i;
            else if (strcmp(ct->name, "sprite") == 0 || strcmp(ct->name, "Sprite") == 0)
                tid_sprite = i;
            else if (strcmp(ct->name, "camera2d") == 0 || strcmp(ct->name, "Camera2D") == 0)
                tid_camera = i;
        }
    }

    /* Parse: collect component attribute maps per entity, then build world. */
    enum { ST_OUTSIDE, ST_ENTITY } state = ST_OUTSIDE;
    uint32_t cur_tid = (uint32_t)-1;
    kyEntity cur_ent = {0, 0};
    int has_transform = 0;

    #define MAX_ENTITIES 64
    #define MAX_ATTRS 32
    #define MAX_KEY_LEN 32
    #define MAX_VAL_LEN 64
    char attr_keys[MAX_ENTITIES][MAX_ATTRS][MAX_KEY_LEN];
    char attr_vals[MAX_ENTITIES][MAX_ATTRS][MAX_VAL_LEN];
    uint32_t attr_tids[MAX_ENTITIES][MAX_ATTRS];
    int attr_counts[MAX_ENTITIES] = {0};
    int entity_count = 0;

    char *p = (char *)raw;
    char *line_start = p;
    while (1) {
        if (*p == '\n' || *p == '\0') {
            size_t llen = (size_t)(p - line_start);

            while (llen > 0 && (line_start[llen-1] == ' ' || line_start[llen-1] == '\t' ||
                                line_start[llen-1] == '\r' || line_start[llen-1] == '\n'))
                llen--;

            if (llen > 0) {
                const char *ln = line_start;
                while (llen > 0 && (*ln == ' ' || *ln == '\t')) { ln++; llen--; }
                const char *line_end = ln + llen;

                if (strncmp(ln, "entity", 6) == 0 &&
                    (ln[6] == '\0' || isspace((unsigned char)ln[6]))) {
                    if (entity_count >= MAX_ENTITIES) { free(raw); return -3; }
                    state = ST_ENTITY;
                    cur_tid = (uint32_t)-1;
                    attr_counts[entity_count] = 0;
                    entity_count++;
                } else if (strncmp(ln, "end", 3) == 0 &&
                           (ln[3] == '\0' || isspace((unsigned char)ln[3]))) {
                    state = ST_OUTSIDE;
                    cur_tid = (uint32_t)-1;
                } else if (state == ST_ENTITY) {
                    /* Check for component declaration */
                    int is_comp = 0;
                    if (strncmp(ln, "transform", 9) == 0 &&
                        (ln[9] == '\0' || isspace((unsigned char)ln[9]))) {
                        cur_tid = tid_transform;
                        has_transform = 1;
                        is_comp = 1;
                    } else if (strncmp(ln, "sprite", 6) == 0 &&
                               (ln[6] == '\0' || isspace((unsigned char)ln[6]))) {
                        cur_tid = tid_sprite;
                        is_comp = 1;
                    } else if (strncmp(ln, "camera2d", 8) == 0 &&
                               (ln[8] == '\0' || isspace((unsigned char)ln[8]))) {
                        cur_tid = tid_camera;
                        is_comp = 1;
                    }

                    /* Parse all attributes remaining on this line */
                    if (is_comp && cur_tid != (uint32_t)-1) {
                        const char *after = ln;
                        if (strncmp(after, "transform", 9) == 0) after += 9;
                        else if (strncmp(after, "sprite", 6) == 0) after += 6;
                        else if (strncmp(after, "camera2d", 8) == 0) after += 8;
                        while (*after == ' ' || *after == '\t') after++;
                        const char *attr_end = line_end;
                        const char *attr = after;
                        while (attr < attr_end) {
                            while (attr < attr_end && (*attr == ' ' || *attr == '\t')) attr++;
                            if (attr >= attr_end) break;
                            int ac = attr_counts[entity_count - 1];
                            if (ac >= MAX_ATTRS) break;
                            const char *key = attr;
                            while (attr < attr_end && *attr != '=' && !isspace((unsigned char)*attr)) attr++;
                            size_t klen = attr - key;
                            if (*attr != '=') break;
                            attr++;
                            const char *val = attr;
                            size_t vlen = 0;
                            if (*attr == '"') {
                                attr++; /* skip opening quote */
                                val = attr;
                                while (attr < attr_end && *attr != '"') attr++;
                                vlen = attr - val;
                                if (attr < attr_end) attr++; /* skip closing quote */
                            } else {
                                while (attr < attr_end && !isspace((unsigned char)*attr)) attr++;
                                vlen = attr - val;
                            }
                            size_t mk = klen < MAX_KEY_LEN-1 ? klen : MAX_KEY_LEN-1;
                            size_t mv = vlen < MAX_VAL_LEN-1 ? vlen : MAX_VAL_LEN-1;
                            memcpy(attr_keys[entity_count-1][ac], key, mk);
                            attr_keys[entity_count-1][ac][mk] = '\0';
                            memcpy(attr_vals[entity_count-1][ac], val, mv);
                            attr_vals[entity_count-1][ac][mv] = '\0';
                            attr_tids[entity_count-1][ac] = cur_tid;
                            attr_counts[entity_count - 1] = ac + 1;
                        }
                    }
                }
            }

            if (*p == '\0') break;
            line_start = p + 1;
        }
        p++;
    }

    free(raw);

    /* Build world from parsed data. */
    for (int ei = 0; ei < entity_count; ei++) {
        /* Spawn entity */
        kyEntity ent = ky_world_spawn(s->world);
        int has_tr = 0;

        /* First pass: add components */
        for (int ai = 0; ai < attr_counts[ei]; ai++) {
            uint32_t t = attr_tids[ei][ai];
            if (t == tid_transform && !has_tr) {
                ky_world_add_component(s->world, ent, tid_transform);
                has_tr = 1;
            } else if (t == tid_sprite) {
                ky_world_add_component(s->world, ent, tid_sprite);
            } else if (t == tid_camera) {
                ky_world_add_component(s->world, ent, tid_camera);
            }
        }

        if (!has_tr) return -4;

        /* Second pass: apply attributes */
        for (int ai = 0; ai < attr_counts[ei]; ai++) {
            apply_attr(s, ent, attr_tids[ei][ai],
                       tid_transform, tid_sprite, tid_camera,
                       attr_keys[ei][ai], attr_vals[ei][ai]);
        }
    }

    return 0;
}
