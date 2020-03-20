#ifndef VC4_SCREEN_H
#define VC4_SCREEN_H

#define HAVE_PTHREAD
#define HAVE_TIMESPEC_GET

#include "pipe/p_screen.h"
#include "renderonly/renderonly.h"
#include "os/os_thread.h"
#include "state_tracker/drm_driver.h"
#include "util/list.h"
#include "util/slab.h"

#ifndef DRM_VC4_PARAM_SUPPORTS_ETC1
#define DRM_VC4_PARAM_SUPPORTS_ETC1		4
#endif

struct vc4_bo;

#define VC4_DEBUG_CL        0x0001
#define VC4_DEBUG_QPU       0x0002
#define VC4_DEBUG_QIR       0x0004
#define VC4_DEBUG_TGSI      0x0008
#define VC4_DEBUG_SHADERDB  0x0010
#define VC4_DEBUG_PERF      0x0020
#define VC4_DEBUG_NORAST    0x0040
#define VC4_DEBUG_ALWAYS_FLUSH 0x0080
#define VC4_DEBUG_ALWAYS_SYNC  0x0100
#define VC4_DEBUG_NIR       0x0200
#define VC4_DEBUG_DUMP      0x0400
#define VC4_DEBUG_SURFACE   0x0800

#define VC4_MAX_MIP_LEVELS 12
#define VC4_MAX_TEXTURE_SAMPLERS 16

struct vc4_simulator_file;

struct vc4_screen {
        struct pipe_screen base;
        struct renderonly *ro;

        int fd;

        int v3d_ver;

        const char *name;

        /** The last seqno we've completed a wait for.
         *
         * This lets us slightly optimize our waits by skipping wait syscalls
         * if we know the job's already done.
         */
        uint64_t finished_seqno;

        struct slab_parent_pool transfer_pool;

        struct vc4_bo_cache {
                /** List of struct vc4_bo freed, by age. */
                struct list_head time_list;
                /** List of struct vc4_bo freed, per size, by age. */
                struct list_head *size_list;
                uint32_t size_list_size;

                mtx_t lock;

                uint32_t bo_size;
                uint32_t bo_count;
        } bo_cache;

        struct hash_table *bo_handles;
        mtx_t bo_handles_mutex;

        uint32_t bo_size;
        uint32_t bo_count;
        bool has_control_flow;
        bool has_etc1;
        bool has_threaded_fs;
        bool has_madvise;
        bool has_tiling_ioctl;
        bool has_perfmon_ioctl;
        bool has_syncobj;

        struct vc4_simulator_file *sim_file;
};

static inline struct vc4_screen *
vc4_screen(struct pipe_screen *screen)
{
        return (struct vc4_screen *)screen;
}

struct pipe_screen *vc4_screen_create(int fd);

const void *
vc4_screen_get_compiler_options(struct pipe_screen *pscreen,
                                enum pipe_shader_ir ir,
                                enum pipe_shader_type shader);

extern uint32_t vc4_debug;

void
vc4_fence_screen_init(struct vc4_screen *screen);

struct vc4_fence *
vc4_fence_create(struct vc4_screen *screen, uint64_t seqno, int fd);

#endif /* VC4_SCREEN_H */
