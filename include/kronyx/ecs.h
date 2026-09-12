#ifndef KRONYX_ECS_H
#define KRONYX_ECS_H

/**
 * @file kronyx/ecs.h
 * @brief Entity-Component-System (ECS) runtime.
 *
 * Archetype-based storage: each entity belongs to exactly one archetype
 * (a bag of component types). Component columns are SoA arrays; moving
 * an entity between archetypes swaps it to the matching archetype row.
 *
 * Key invariants:
 *   - component_types[i] has type_id == i
 *   - name_cache maps component name -> type_id (O(1) lookup)
 *   - slots[id] is valid iff slots[id].version == entity.version
 */

#include "defines.h"
#include "memory.h"
#include "array.h"
#include "hashmap.h"

typedef struct kyEntity {
    uint32_t id;
    uint32_t version;
} kyEntity;

typedef struct kyComponentType {
    const char *name;
    size_t size;
    uint32_t type_id;
    void (*ctor)(void *comp);
    void (*dtor)(void *comp);
} kyComponentType;

typedef struct kyArchetype {
    uint32_t *types;
    uint32_t type_count;
    size_t count;
    size_t capacity;
    void **columns;
    size_t *strides;
    uint32_t *entity_ids;
    uint64_t type_hash;
} kyArchetype;

typedef struct kyEntitySlot {
    uint32_t version;
    int32_t archetype_index;
    uint32_t row;
} kyEntitySlot;

typedef struct kyWorld kyWorld;

typedef struct kySystem {
    const char *name;
    uint32_t order;
    void (*update)(kyWorld *w, float dt, void *user);
    void *user;
} kySystem;

typedef struct kyWorld {
    kyAllocator alloc;
    kyArray component_types;
    kyArray archetypes;
    kyArray slots;
    kyArray free_ids;
    kyArray systems;
    uint32_t next_version;
    kyHashMap name_cache;
} kyWorld;

KY_API kyWorld *ky_world_create(kyAllocator *alloc);
/** Destroy world and all its archetypes, component data, and caches. */
KY_API void ky_world_destroy(kyWorld *w);

/**
 * Register a component type. Returns its type_id (sequential, starting at 0).
 * If t->name is non-NULL it is copied into the name_cache for O(1) lookup.
 */
KY_API uint32_t ky_world_register_component(kyWorld *w, const kyComponentType *t);
KY_API const kyComponentType *ky_world_component_type(const kyWorld *w, uint32_t type_id);
/** Find type_id by component name via O(1) hashmap lookup. Returns UINT32_MAX when not found. */
KY_API uint32_t ky_world_component_type_by_name(const kyWorld *w, const char *name);

KY_API void ky_world_register_system(kyWorld *w, const kySystem *sys);
/** Sort systems in-place by order field using stable insertion sort. */
KY_API void ky_world_sort_systems(kyWorld *w);

KY_API kyEntity ky_world_spawn(kyWorld *w);
KY_API void ky_world_despawn(kyWorld *w, kyEntity e);
KY_API int ky_entity_valid(const kyWorld *w, kyEntity e);
/** Return the number of alive (non-despawned) entities in the world. */
KY_API int ky_world_alive_count(const kyWorld *w);
/* Return the entity at alive-index `idx` (0-based), ordered by id.
 * Returns {0,0} if idx is out of range. */
KY_API kyEntity ky_world_get_alive_entity(const kyWorld *w, int idx);

KY_API void *ky_world_add_component(kyWorld *w, kyEntity e, uint32_t type_id);
KY_API void *ky_world_get_component(const kyWorld *w, kyEntity e, uint32_t type_id);
KY_API int ky_world_has_component(const kyWorld *w, kyEntity e, uint32_t type_id);
KY_API void ky_world_remove_component(kyWorld *w, kyEntity e, uint32_t type_id);
KY_API void ky_world_remove_all_components(kyWorld *w, kyEntity e);

KY_API void ky_world_step(kyWorld *w, float dt);

typedef struct kyViewIter {
    const kyWorld *w;
    const uint32_t *types;
    uint32_t type_count;
    int32_t arch_index;
    size_t row;
    kyEntity current;
} kyViewIter;

KY_API int ky_view_begin(const kyWorld *w, const uint32_t *types, uint32_t type_count, kyViewIter *it);
KY_API int ky_view_next(kyViewIter *it);

#endif
