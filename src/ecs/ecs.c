#include "kronyx/ecs.h"
#include "kronyx/hashmap.h"

#define KY_ARCH_NONE -1

static uint64_t archetype_hash(const uint32_t *types, uint32_t n) {
    uint64_t h = 1469598103934665603ull;
    for (uint32_t i = 0; i < n; ++i) {
        h ^= (uint64_t)types[i];
        h *= 1099511628211ull;
    }
    return h;
}

static int archetype_matches(const kyArchetype *a, const uint32_t *types, uint32_t n) {
    if (a->type_count != n) return 0;
    for (uint32_t i = 0; i < n; ++i) {
        if (a->types[i] != types[i]) return 0;
    }
    return 1;
}

static int archetype_find(const kyWorld *w, const uint32_t *types, uint32_t n) {
    const kyArray *archs = &w->archetypes;
    for (size_t i = 0; i < archs->len; ++i) {
        kyArchetype *a = (kyArchetype *)ky_array_get(archs, i);
        if (archetype_matches(a, types, n)) return (int)i;
    }
    return KY_ARCH_NONE;
}

static void sort_types(uint32_t *types, uint32_t n) {
    for (uint32_t i = 1; i < n; ++i) {
        uint32_t key = types[i];
        int32_t j = (int32_t)i - 1;
        while (j >= 0 && types[j] > key) {
            types[j + 1] = types[j];
            --j;
        }
        types[j + 1] = key;
    }
}

static int32_t archetype_create(kyWorld *w, const uint32_t *types, uint32_t n) {
    kyArchetype a;
    memset(&a, 0, sizeof(a));
    a.type_count = n;
    a.types = NULL;
    a.columns = NULL;
    a.strides = NULL;
    a.entity_ids = NULL;
    a.count = 0;
    a.capacity = 0;
    if (n > 0) {
        a.types = (uint32_t *)ky_mem_alloc(&w->alloc, n * sizeof(uint32_t));
        memcpy(a.types, types, n * sizeof(uint32_t));
        sort_types(a.types, n);
        a.columns = (void **)ky_mem_alloc(&w->alloc, n * sizeof(void *));
        memset(a.columns, 0, n * sizeof(void *));
        a.strides = (size_t *)ky_mem_alloc(&w->alloc, n * sizeof(size_t));
        for (uint32_t i = 0; i < n; ++i) {
            const kyComponentType *ct = (const kyComponentType *)ky_array_get(&w->component_types, a.types[i]);
            a.strides[i] = ct->size;
        }
    }
    a.type_hash = archetype_hash(a.types, n);
    ky_array_push(&w->archetypes, &a);
    return (int32_t)(w->archetypes.len - 1);
}

static void archetype_grow(kyWorld *w, kyArchetype *a, size_t need) {
    if (need <= a->capacity) return;
    size_t nc = a->capacity ? a->capacity : 16;
    while (nc < need) nc *= 2;
    a->entity_ids = (uint32_t *)ky_mem_realloc(&w->alloc, a->entity_ids, nc * sizeof(uint32_t));
    for (uint32_t i = 0; i < a->type_count; ++i) {
        const kyComponentType *ct = (const kyComponentType *)ky_array_get(&w->component_types, a->types[i]);
        a->columns[i] = ky_mem_realloc(&w->alloc, a->columns[i], nc * ct->size);
    }
    a->capacity = nc;
}

static void archetype_remove_row(kyWorld *w, kyArchetype *a, size_t row) {
    size_t last = a->count - 1;
    if (row != last) {
        uint32_t moved_id = a->entity_ids[last];
        for (uint32_t i = 0; i < a->type_count; ++i) {
            const kyComponentType *ct = (const kyComponentType *)ky_array_get(&w->component_types, a->types[i]);
            memcpy((char *)a->columns[i] + row * ct->size,
                   (char *)a->columns[i] + last * ct->size, ct->size);
        }
        kyEntitySlot *slot = (kyEntitySlot *)ky_array_get(&w->slots, moved_id);
        slot->row = (uint32_t)row;
    }
    a->count--;
}

static int32_t move_entity(kyWorld *w, uint32_t id, const uint32_t *new_types, uint32_t new_count) {
    kyEntitySlot *slot = (kyEntitySlot *)ky_array_get(&w->slots, id);
    int32_t old_arch = slot->archetype_index;
    size_t old_row = slot->row;

    uint32_t sorted[64];
    uint32_t *types = NULL;
    uint32_t *tmp_buf = NULL;
    if (new_count > 0) {
        if (new_count <= KY_ARRAY_LEN(sorted)) {
            types = sorted;
        } else {
            tmp_buf = (uint32_t *)ky_mem_alloc(&w->alloc, new_count * sizeof(uint32_t));
            types = tmp_buf;
        }
        memcpy(types, new_types, new_count * sizeof(uint32_t));
        sort_types(types, new_count);
    }

    int32_t new_arch = archetype_find(w, types, new_count);
    if (new_arch == KY_ARCH_NONE) {
        new_arch = archetype_create(w, types, new_count);
    }
    kyArchetype *na = (kyArchetype *)ky_array_get(&w->archetypes, new_arch);
    archetype_grow(w, na, na->count + 1);
    size_t new_row = na->count++;

    kyArchetype *oa = old_arch == KY_ARCH_NONE ? NULL : (kyArchetype *)ky_array_get(&w->archetypes, old_arch);

    for (uint32_t i = 0; i < new_count; ++i) {
        uint32_t tid = types[i];
        const kyComponentType *ct = (const kyComponentType *)ky_array_get(&w->component_types, tid);
        void *dst = (char *)na->columns[i] + new_row * ct->size;
        memset(dst, 0, ct->size);
        if (oa) {
            for (uint32_t j = 0; j < oa->type_count; ++j) {
                if (oa->types[j] == tid) {
                    memcpy(dst, (char *)oa->columns[j] + old_row * ct->size, ct->size);
                    break;
                }
            }
        }
        if (ct->ctor) ct->ctor(dst);
    }
    na->entity_ids[new_row] = id;

    if (oa) {
        for (uint32_t i = 0; i < oa->type_count; ++i) {
            uint32_t tid = oa->types[i];
            int found = 0;
            for (uint32_t j = 0; j < new_count; ++j) {
                if (types[j] == tid) {
                    found = 1;
                    break;
                }
            }
            if (!found) {
                const kyComponentType *ct = (const kyComponentType *)ky_array_get(&w->component_types, tid);
                if (ct->dtor) ct->dtor((char *)oa->columns[i] + old_row * ct->size);
            }
        }
        archetype_remove_row(w, oa, old_row);
    }

    slot->archetype_index = new_arch;
    slot->row = (uint32_t)new_row;
    if (tmp_buf) ky_mem_free(&w->alloc, tmp_buf);
    return new_arch;
}

kyWorld *ky_world_create(kyAllocator *alloc) {
    kyWorld *w = (kyWorld *)ky_mem_alloc(alloc, sizeof(kyWorld));
    w->alloc = *alloc;
    ky_array_init(&w->component_types, alloc, sizeof(kyComponentType), 8);
    ky_array_init(&w->archetypes, alloc, sizeof(kyArchetype), 8);
    ky_array_init(&w->slots, alloc, sizeof(kyEntitySlot), 64);
    ky_array_init(&w->free_ids, alloc, sizeof(uint32_t), 16);
    ky_array_init(&w->systems, alloc, sizeof(kySystem), 8);
    w->next_version = 1;
    return w;
}

void ky_world_destroy(kyWorld *w) {
    for (size_t i = 0; i < w->archetypes.len; ++i) {
        kyArchetype *a = (kyArchetype *)ky_array_get(&w->archetypes, i);
        ky_mem_free(&w->alloc, a->types);
        ky_mem_free(&w->alloc, a->strides);
        ky_mem_free(&w->alloc, a->entity_ids);
        if (a->columns) {
            for (uint32_t j = 0; j < a->type_count; ++j) {
                ky_mem_free(&w->alloc, a->columns[j]);
            }
        }
        ky_mem_free(&w->alloc, a->columns);
    }
    ky_array_deinit(&w->component_types);
    ky_array_deinit(&w->archetypes);
    ky_array_deinit(&w->slots);
    ky_array_deinit(&w->free_ids);
    ky_array_deinit(&w->systems);
    ky_mem_free(&w->alloc, w);
}

uint32_t ky_world_register_component(kyWorld *w, const kyComponentType *t) {
    kyComponentType ct = *t;
    ct.type_id = (uint32_t)w->component_types.len;
    ky_array_push(&w->component_types, &ct);
    return ct.type_id;
}

const kyComponentType *ky_world_component_type(const kyWorld *w, uint32_t type_id) {
    if (type_id >= w->component_types.len) return NULL;
    return (const kyComponentType *)ky_array_get(&w->component_types, type_id);
}

void ky_world_register_system(kyWorld *w, const kySystem *sys) {
    ky_array_push(&w->systems, sys);
}

static int sys_order_cmp(const void *a, const void *b) {
    const kySystem *sa = (const kySystem *)a;
    const kySystem *sb = (const kySystem *)b;
    return (int)(sa->order - sb->order);
}

void ky_world_sort_systems(kyWorld *w) {
    if (w->systems.len > 1) {
        qsort(w->systems.data, w->systems.len, sizeof(kySystem), sys_order_cmp);
    }
}

kyEntity ky_world_spawn(kyWorld *w) {
    kyEntity e;
    if (w->free_ids.len > 0) {
        e.id = *(uint32_t *)ky_array_get(&w->free_ids, w->free_ids.len - 1);
        w->free_ids.len--;
        kyEntitySlot *slot = (kyEntitySlot *)ky_array_get(&w->slots, e.id);
        slot->version++;
        e.version = slot->version;
        slot->archetype_index = KY_ARCH_NONE;
        slot->row = 0;
        return e;
    }
    e.id = (uint32_t)w->slots.len;
    kyEntitySlot slot;
    slot.version = w->next_version++;
    slot.archetype_index = KY_ARCH_NONE;
    slot.row = 0;
    ky_array_push(&w->slots, &slot);
    e.version = slot.version;
    return e;
}

void ky_world_despawn(kyWorld *w, kyEntity e) {
    if (!ky_entity_valid(w, e)) return;
    kyEntitySlot *slot = (kyEntitySlot *)ky_array_get(&w->slots, e.id);
    if (slot->archetype_index != KY_ARCH_NONE) {
        ky_world_remove_all_components(w, e);
    }
    slot->version++;
    slot->archetype_index = KY_ARCH_NONE;
    slot->row = 0;
    ky_array_push(&w->free_ids, &e.id);
}

int ky_entity_valid(const kyWorld *w, kyEntity e) {
    if (e.id >= w->slots.len) return 0;
    const kyEntitySlot *slot = (const kyEntitySlot *)ky_array_get(&w->slots, e.id);
    return slot->version == e.version;
}

void *ky_world_add_component(kyWorld *w, kyEntity e, uint32_t type_id) {
    if (!ky_entity_valid(w, e)) return NULL;
    kyEntitySlot *slot = (kyEntitySlot *)ky_array_get(&w->slots, e.id);
    if (slot->archetype_index != KY_ARCH_NONE) {
        kyArchetype *a = (kyArchetype *)ky_array_get(&w->archetypes, slot->archetype_index);
        for (uint32_t i = 0; i < a->type_count; ++i) {
            if (a->types[i] == type_id) {
                const kyComponentType *ct = (const kyComponentType *)ky_array_get(&w->component_types, type_id);
                return (char *)a->columns[i] + slot->row * ct->size;
            }
        }
    }
    kyArray tmp;
    ky_array_init(&tmp, &w->alloc, sizeof(uint32_t), 8);
    if (slot->archetype_index != KY_ARCH_NONE) {
        kyArchetype *a = (kyArchetype *)ky_array_get(&w->archetypes, slot->archetype_index);
        for (uint32_t i = 0; i < a->type_count; ++i) {
            ky_array_push(&tmp, &a->types[i]);
        }
    }
    ky_array_push(&tmp, &type_id);
    int32_t na = move_entity(w, e.id, (uint32_t *)tmp.data, (uint32_t)tmp.len);
    ky_array_deinit(&tmp);
    kyArchetype *a = (kyArchetype *)ky_array_get(&w->archetypes, na);
    for (uint32_t i = 0; i < a->type_count; ++i) {
        if (a->types[i] == type_id) {
            const kyComponentType *ct = (const kyComponentType *)ky_array_get(&w->component_types, type_id);
            return (char *)a->columns[i] + slot->row * ct->size;
        }
    }
    return NULL;
}

void *ky_world_get_component(const kyWorld *w, kyEntity e, uint32_t type_id) {
    if (!ky_entity_valid(w, e)) return NULL;
    const kyEntitySlot *slot = (const kyEntitySlot *)ky_array_get(&w->slots, e.id);
    if (slot->archetype_index == KY_ARCH_NONE) return NULL;
    const kyArchetype *a = (const kyArchetype *)ky_array_get(&w->archetypes, slot->archetype_index);
    for (uint32_t i = 0; i < a->type_count; ++i) {
        if (a->types[i] == type_id) {
            const kyComponentType *ct = (const kyComponentType *)ky_array_get(&w->component_types, type_id);
            return (char *)a->columns[i] + slot->row * ct->size;
        }
    }
    return NULL;
}

int ky_world_has_component(const kyWorld *w, kyEntity e, uint32_t type_id) {
    return ky_world_get_component(w, e, type_id) != NULL;
}

void ky_world_remove_component(kyWorld *w, kyEntity e, uint32_t type_id) {
    if (!ky_entity_valid(w, e)) return;
    kyEntitySlot *slot = (kyEntitySlot *)ky_array_get(&w->slots, e.id);
    if (slot->archetype_index == KY_ARCH_NONE) return;
    kyArchetype *a = (kyArchetype *)ky_array_get(&w->archetypes, slot->archetype_index);
    int found = 0;
    for (uint32_t i = 0; i < a->type_count; ++i) {
        if (a->types[i] == type_id) {
            found = 1;
            break;
        }
    }
    if (!found) return;

    kyArray tmp;
    ky_array_init(&tmp, &w->alloc, sizeof(uint32_t), 8);
    for (uint32_t i = 0; i < a->type_count; ++i) {
        if (a->types[i] != type_id) {
            ky_array_push(&tmp, &a->types[i]);
        }
    }
    move_entity(w, e.id, (uint32_t *)tmp.data, (uint32_t)tmp.len);
    ky_array_deinit(&tmp);
}

void ky_world_remove_all_components(kyWorld *w, kyEntity e) {
    if (!ky_entity_valid(w, e)) return;
    kyEntitySlot *slot = (kyEntitySlot *)ky_array_get(&w->slots, e.id);
    if (slot->archetype_index == KY_ARCH_NONE) return;
    kyArchetype *a = (kyArchetype *)ky_array_get(&w->archetypes, slot->archetype_index);
    size_t row = slot->row;
    for (uint32_t i = 0; i < a->type_count; ++i) {
        const kyComponentType *ct = (const kyComponentType *)ky_array_get(&w->component_types, a->types[i]);
        if (ct->dtor) ct->dtor((char *)a->columns[i] + row * ct->size);
    }
    archetype_remove_row(w, a, row);
    slot->archetype_index = KY_ARCH_NONE;
    slot->row = 0;
}

void ky_world_step(kyWorld *w, float dt) {
    for (size_t i = 0; i < w->systems.len; ++i) {
        kySystem *s = (kySystem *)ky_array_get(&w->systems, i);
        if (s->update) s->update(w, dt, s->user);
    }
}

static int view_arch_matches(const kyWorld *w, int32_t arch_index, const uint32_t *types, uint32_t n) {
    if (arch_index == KY_ARCH_NONE) return n == 0;
    const kyArchetype *a = (const kyArchetype *)ky_array_get(&w->archetypes, arch_index);
    for (uint32_t i = 0; i < n; ++i) {
        int found = 0;
        for (uint32_t j = 0; j < a->type_count; ++j) {
            if (a->types[j] == types[i]) {
                found = 1;
                break;
            }
        }
        if (!found) return 0;
    }
    return 1;
}

int ky_view_begin(const kyWorld *w, const uint32_t *types, uint32_t type_count, kyViewIter *it) {
    it->w = w;
    it->types = types;
    it->type_count = type_count;
    it->arch_index = -1;
    it->row = 0;
    it->current.id = 0;
    it->current.version = 0;
    return ky_view_next(it);
}

int ky_view_next(kyViewIter *it) {
    const kyWorld *w = it->w;
    while (1) {
        if (it->arch_index < 0) {
            it->arch_index = 0;
        }
        if ((size_t)it->arch_index >= w->archetypes.len) return 0;
        kyArchetype *a = (kyArchetype *)ky_array_get(&w->archetypes, it->arch_index);
        if (it->row >= a->count) {
            it->arch_index++;
            it->row = 0;
            continue;
        }
        if (!view_arch_matches(w, it->arch_index, it->types, it->type_count)) {
            it->arch_index++;
            it->row = 0;
            continue;
        }
        it->current.id = a->entity_ids[it->row];
        const kyEntitySlot *slot = (const kyEntitySlot *)ky_array_get(&w->slots, it->current.id);
        it->current.version = slot->version;
        it->row++;
        return 1;
    }
}
