/*
 * Copyright (C) 2026. All rights reserved.
 * kronyx
 *
 *  G3 kyx game bindings — entry point for exposing world/Transform/input/log
 *  to kyx scripts via `KyxBinds`.
 */

#ifndef KRONYX_BINDINGS_H
#define KRONYX_BINDINGS_H

#include "kronyx/script.h"
#include "kronyx/ecs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct KyxBinds KyxBinds;

/*
 * Register natives under `std.*`:
 *   std.spawn()           -> int (packed entity)
 *   std.alive(ent)        -> int 0/1
 *   std.despawn(ent)      -> nil
 *   std.setpos(ent,x,y)   -> nil
 *   std.getpos(ent,axis)  -> float   axis 0 = x, 1 = y
 *   std.setscale(ent,sx,sy)-> nil
 *   std.getscale(ent,axis)-> float
 *   std.poll_input()      -> nil (drains input buffer, caches axis_x/last_key)
 *   std.axis_x()          -> float -1.0 / 0.0 / +1.0
 *   std.last_key()        -> int key code (KY_KEY_*) or 0
 *   std.log(varargs)      -> nil (prints to stderr)
 *
 * `ent` is an opaque packed integer: low 32 bits = entity id, high 32 bits
 * = entity version. Pass it back through unchanged.
 *
 * Returns the context pointer (caller owns; free via ky_bind_destroy).
 */
KyxBinds *ky_bind_create(kyVM *vm, kyWorld *world);
void      ky_bind_destroy(KyxBinds *b);

#ifdef __cplusplus
}
#endif

#endif /* KRONYX_BINDINGS_H */
