#define LOG_TAG "allocator@2.0-drm_gralloc_rpi3"
#define LOG_NDEBUG 0
#include <utils/Log.h>
#include <sys/errno.h>
#include <drm_fourcc.h>
#include "drm_gralloc_rpi3.h"

extern "C" {
void drm_gralloc_vc4_init();
void drm_gralloc_vc4_destroy();
struct gralloc_drm_bo_t *vc4_bo_create(int width, int height, int format, int usage);
void vc4_bo_destroy(struct gralloc_drm_bo_t *bo);
}

int drm_init() {
	int err = 0;
	drm_gralloc_vc4_init();
	return err;
}

void drm_deinit() {
	drm_gralloc_vc4_destroy();
}

int drm_alloc(int w, int h, int format, int usage,
		buffer_handle_t *handle, int *stride) {
	struct gralloc_drm_bo_t *bo;
	int bpp = gralloc_drm_get_bpp(format);
	if (!bpp) return -EINVAL;

	struct gralloc_drm_bo_t *vc4_bo = vc4_bo_create(w, h, format, usage);
	ALOGV("drm_alloc() vc4_bo_create() called");

	if (!vc4_bo) return -ENOMEM;

	if (stride)
		*stride = vc4_bo->handle->stride;
	*handle = &vc4_bo->handle->base;

	/* in pixels */
	*stride /= bpp;
	return 0;
}

int drm_free(buffer_handle_t handle) {
	struct gralloc_drm_handle_t *drm_handle = gralloc_drm_handle(handle);
	close(drm_handle->prime_fd);
	vc4_bo_destroy(drm_handle->data);
	return 0;
}

