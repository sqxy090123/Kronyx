#include "kronyx/ecs.h"

#define KY_ARCH_NONE -1

static const kyComponentType *comp_type(const kyWorld *w, uint32_t id) {
    return (const kyComponentType *)ky_array_get(&w->component_types, id);
}

static kyEntitySlot *slot_at(const kyWorld *w, uint32_t id) {
    return (kyEntitySlot *)ky_array_get(&w->slots, id);
}

static kyArchetype *arch_at(const kyWorld *w, int32_t idx) {
    return (kyArchetype *)ky_array_get(&w->archetypes, idx);
}

static int arch_has_type(const kyArchetype *a, uint32_t type_id) {
    /* Types are sorted; binary search for O(log n). */
    uint32_t lo = 0, hi = a->type_count;
    while (lo < hi) {
        uint32_t mid = (lo + hi) >> 1;
        if (a->types[mid] < type_id) lo = mid + 1;
        else hi = mid;
    }
    return (lo < a->type_count && a->types[lo] == type_id) ? (int)lo : -1;
}

static void *find_comp(const kyWorld *w, const kyArchetype *a, size_t row, uint32_t type_id) {
    int col = arch_has_type(a, type_id);
    return col < 0 ? NULL : (char *)a->columns[col] + row * a->strides[col];
}

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
    uint64_t target_hash = archetype_hash(types, n);
    const kyArray *archs = &w->archetypes;
    for (size_t i = 0; i < archs->len; ++i) {
        kyArchetype *a = (kyArchetype *)ky_array_get(archs, i);
        if (a->type_hash == target_hash && archetype_matches(a, types, n)) return (int)i;
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
        for (uint32_t i = 0; i < n; ++i)
            a.strides[i] = comp_type(w, a.types[i])->size;
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
    for (uint32_t i = 0; i < a->type_count; ++i)
        a->columns[i] = ky_mem_realloc(&w->alloc, a->columns[i], nc * comp_type(w, a->types[i])->size);
    a->capacity = nc;
}

static void archetype_remove_row(kyWorld *w, kyArchetype *a, size_t row) {
    size_t last = a->count - 1;
    if (row != last) {
        uint32_t moved_id = a->entity_ids[last];
        for (uint32_t i = 0; i < a->type_count; ++i) {
            size_t sz = comp_type(w, a->types[i])->size;
            memcpy((char *)a->columns[i] + row * sz, (char *)a->columns[i] + last * sz, sz);
        }
        slot_at(w, moved_id)->row = (uint32_t)row;
    }
    a->count--;
}

static int32_t move_entity(kyWorld *w, uint32_t id, const uint32_t *new_types, uint32_t new_count) {
    kyEntitySlot *slot = slot_at(w, id);
    int32_t old_arch = slot->archetype_index;
    size_t old_row = slot->row;

    uint32_t sorted[64];
    uint32_t *types = NULL, *tmp_buf = NULL;
    if (new_count > 0) {
        types = new_count <= KY_ARRAY_LEN(sorted)
            ? sorted
            : (tmp_buf = (uint32_t *)ky_mem_alloc(&w->alloc, new_count * sizeof(uint32_t)));
        memcpy(types, new_types, new_count * sizeof(uint32_t));
        sort_types(types, new_count);
    }

    int32_t new_arch = archetype_find(w, types, new_count);
    if (new_arch == KY_ARCH_NONE) new_arch = archetype_create(w, types, new_count);
    kyArchetype *na = arch_at(w, new_arch);
    archetype_grow(w, na, na->count + 1);
    size_t new_row = na->count++;
    kyArchetype *oa = old_arch == KY_ARCH_NONE ? NULL : arch_at(w, old_arch);

    for (uint32_t i = 0; i < new_count; ++i) {
        void *dst = (char *)na->columns[i] + new_row * na->strides[i];
        memset(dst, 0, na->strides[i]);
        int kept = 0;
        if (oa) {
            int col = arch_has_type(oa, types[i]);
            if (col >= 0) {
                memcpy(dst, (char *)oa->columns[col] + old_row * oa->strides[col], na->strides[i]);
                kept = 1;
            }
        }
        /* Only construct components that are genuinely new; ones carried over
         * from the old archetype already have live data (memcpy'd above) and
         * must not have their ctor recalled. */
        if (!kept) {
            const kyComponentType *ct = comp_type(w, types[i]);
            if (ct->ctor) ct->ctor(dst);
        }
    }
    na->entity_ids[new_row] = id;

    if (oa) {
        for (uint32_t i = 0; i < oa->type_count; ++i) {
            int keep = 0;
            for (uint32_t j = 0; j < new_count; ++j) {
                if (types[j] == oa->types[i]) { keep = 1; break; }
            }
            if (!keep) {
                const kyComponentType *ct = comp_type(w, oa->types[i]);
                if (ct->dtor) ct->dtor((char *)oa->columns[i] + old_row * oa->strides[i]);
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
    ky_hashmap_init(&w->name_cache, alloc, 16);
    w->next_version = 1;
    return w;
}

void ky_world_destroy(kyWorld *w) {
    for (size_t i = 0; i < w->archetypes.len; ++i) {
        kyArchetype *a = arch_at(w, (int32_t)i);
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
    ky_hashmap_deinit(&w->name_cache);
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
    if (t->name) {
        char *key = (char *)ky_mem_dup(&w->alloc, t->name, strlen(t->name) + 1);
        if (key) ky_hashmap_set_key(&w->name_cache, key, (void *)(size_t)(ct.type_id + 1));
    }
    return ct.type_id;
}

const kyComponentType *ky_world_component_type(const kyWorld *w, uint32_t type_id) {
    if (type_id >= w->component_types.len) return NULL;
    return (const kyComponentType *)ky_array_get(&w->component_types, type_id);
}

uint32_t ky_world_component_type_by_name(const kyWorld *w, const char *name) {
    if (!w || !name) return (uint32_t)-1;
    void *v = ky_hashmap_get(&w->name_cache, name);
    if (!v) return (uint32_t)-1;
    /* Stored as (type_id + 1) so type_id 0 is not confused with "missing". */
    return (uint32_t)((size_t)v - 1);
}

void ky_world_register_system(kyWorld *w, const kySystem *sys) {
    ky_array_push(&w->systems, sys);
}

void ky_world_sort_systems(kyWorld *w) {
    /* Insertion sort — stable, no heap allocation, optimal for small N */
    size_t n = w->systems.len;
    for (size_t i = 1; i < n; ++i) {
        kySystem key;
        memcpy(&key, ky_array_get(&w->systems, i), sizeof(kySystem));
        size_t j = i;
        while (j > 0) {
            kySystem *prev = (kySystem *)ky_array_get(&w->systems, j - 1);
            if (prev->order <= key.order) break;
            memcpy(ky_array_get(&w->systems, j), prev, sizeof(kySystem));
            --j;
        }
        memcpy(ky_array_get(&w->systems, j), &key, sizeof(kySystem));
    }
}

kyEntity ky_world_spawn(kyWorld *w) {
    kyEntity e;
    if (w->free_ids.len > 0) {
        e.id = *(uint32_t *)ky_array_get(&w->free_ids, w->free_ids.len - 1);
        w->free_ids.len--;
        kyEntitySlot *slot = slot_at(w, e.id);
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
    kyEntitySlot *slot = slot_at(w, e.id);
    if (slot->archetype_index != KY_ARCH_NONE)
        ky_world_remove_all_components(w, e);
    slot->version++;
    slot->archetype_index = KY_ARCH_NONE;
    slot->row = 0;
    ky_array_push(&w->free_ids, &e.id);
}

int ky_entity_valid(const kyWorld *w, kyEntity e) {
    if (e.id >= w->slots.len) return 0;
    return slot_at(w, e.id)->version == e.version;
}

int ky_world_alive_count(const kyWorld *w) {
    if (!w) return 0;
    int count = 0;
    for (size_t i = 0; i < w->slots.len; ++i) {
        const kyEntitySlot *s = slot_at(w, (uint32_t)i);
        if (s->archetype_index != KY_ARCH_NONE) count++;
    }
    return count;
}

kyEntity ky_world_get_alive_entity(const kyWorld *w, int idx) {
    kyEntity zero = {0, 0};
    if (!w || idx < 0) return zero;
    int seen = 0;
    for (size_t i = 0; i < w->slots.len; ++i) {
        const kyEntitySlot *s = slot_at(w, (uint32_t)i);
        if (s->archetype_index != KY_ARCH_NONE) {
            if (seen == idx) return (kyEntity){(uint32_t)i, s->version};
            seen++;
        }
    }
    return zero;
}

void *ky_world_add_component(kyWorld *w, kyEntity e, uint32_t type_id) {
    if (!w || !ky_entity_valid(w, e)) return NULL;
    if (type_id >= w->component_types.len) return NULL;
    kyEntitySlot *slot = slot_at(w, e.id);
    if (slot->archetype_index != KY_ARCH_NONE) {
        void *p = find_comp(w, arch_at(w, slot->archetype_index), slot->row, type_id);
        if (p) return p;
    }
    kyArray tmp;
    ky_array_init(&tmp, &w->alloc, sizeof(uint32_t), 8);
    if (slot->archetype_index != KY_ARCH_NONE) {
        kyArchetype *a = arch_at(w, slot->archetype_index);
        for (uint32_t i = 0; i < a->type_count; ++i)
            ky_array_push(&tmp, &a->types[i]);
    }
    ky_array_push(&tmp, &type_id);
    int32_t na = move_entity(w, e.id, (uint32_t *)tmp.data, (uint32_t)tmp.len);
    ky_array_deinit(&tmp);
    return find_comp(w, arch_at(w, na), slot->row, type_id);
}

void *ky_world_get_component(const kyWorld *w, kyEntity e, uint32_t type_id) {
    if (!ky_entity_valid(w, e)) return NULL;
    const kyEntitySlot *slot = slot_at(w, e.id);
    if (slot->archetype_index == KY_ARCH_NONE) return NULL;
    return find_comp(w, arch_at(w, slot->archetype_index), slot->row, type_id);
}

int ky_world_has_component(const kyWorld *w, kyEntity e, uint32_t type_id) {
    return ky_world_get_component(w, e, type_id) != NULL;
}

void ky_world_remove_component(kyWorld *w, kyEntity e, uint32_t type_id) {
    if (!ky_entity_valid(w, e)) return;
    kyEntitySlot *slot = slot_at(w, e.id);
    if (slot->archetype_index == KY_ARCH_NONE) return;
    kyArchetype *a = arch_at(w, slot->archetype_index);
    if (arch_has_type(a, type_id) < 0) return;

    kyArray tmp;
    ky_array_init(&tmp, &w->alloc, sizeof(uint32_t), 8);
    for (uint32_t i = 0; i < a->type_count; ++i) {
        if (a->types[i] != type_id) ky_array_push(&tmp, &a->types[i]);
    }
    move_entity(w, e.id, (uint32_t *)tmp.data, (uint32_t)tmp.len);
    ky_array_deinit(&tmp);
}

void ky_world_remove_all_components(kyWorld *w, kyEntity e) {
    if (!ky_entity_valid(w, e)) return;
    kyEntitySlot *slot = slot_at(w, e.id);
    if (slot->archetype_index == KY_ARCH_NONE) return;
    kyArchetype *a = arch_at(w, slot->archetype_index);
    size_t row = slot->row;
    for (uint32_t i = 0; i < a->type_count; ++i) {
        const kyComponentType *ct = comp_type(w, a->types[i]);
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
    const kyArchetype *a = arch_at(w, arch_index);
    for (uint32_t i = 0; i < n; ++i) {
        if (arch_has_type(a, types[i]) < 0) return 0;
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
        kyArchetype *a = arch_at(w, it->arch_index);
        if (it->row >= a->count || !view_arch_matches(w, it->arch_index, it->types, it->type_count)) {
            it->arch_index++;
            it->row = 0;
            continue;
        }
        it->current.id = a->entity_ids[it->row];
        it->current.version = slot_at(w, it->current.id)->version;
        it->row++;
        return 1;
    }
}
