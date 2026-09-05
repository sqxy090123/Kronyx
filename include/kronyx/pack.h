#ifndef KY_PACK_H
#define KY_PACK_H

#include "defines.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum kyPackTarget {
    KY_PACK_EXE = 0,
    KY_PACK_NPM,
    KY_PACK_JAR,
    KY_PACK_APK
} kyPackTarget;

typedef struct kyPackDesc {
    const char *title;
    const char *version;
    const char *script;
    const char *out_path;
} kyPackDesc;

KY_API const char *ky_pack_target_name(kyPackTarget target);
KY_API int ky_pack(const kyPackDesc *desc, kyPackTarget target, char *err, int err_size);

#ifdef __cplusplus
}
#endif

#endif
