#define LOG_TAG "allocator@2.0-drm_gralloc_vc4"
#define LOG_NDEBUG 0
#include <utils/Log.h>
#include <sys/errno.h>
#include <cutils/atomic.h>

#include <hardware/gralloc.h>
#include <system/graphics.h>

#include <pipe/p_screen.h>
#include <pipe/p_context.h>
#include <state_tracker/drm_driver.h>
#include <util/u_inlines.h>
#include <util/u_memory.h>

#include <vc4/drm/vc4_drm_public.h>
#include <target-helpers/inline_debug_helper.h>


#include "gralloc_drm_priv.h"

struct pipe_manager {
	struct gralloc_drm_drv_t base;
	int fd;
	pthread_mutex_t mutex;
	struct pipe_screen *screen;
	struct pipe_context *context;
};

struct pipe_buffer {
	struct gralloc_drm_bo_t base;
	struct pipe_resource *resource;
	struct winsys_handle winsys;
	struct pipe_transfer *transfer;
};

static enum pipe_format get_pipe_format(int format)
{
	enum pipe_format fmt;

	switch (format) {
	case HAL_PIXEL_FORMAT_RGBA_8888:
		fmt = PIPE_FORMAT_R8G8B8A8_UNORM;
		break;
	case HAL_PIXEL_FORMAT_RGBX_8888:
		fmt = PIPE_FORMAT_R8G8B8X8_UNORM;
		break;
	case HAL_PIXEL_FORMAT_RGB_888:
		fmt = PIPE_FORMAT_R8G8B8_UNORM;
		break;
	case HAL_PIXEL_FORMAT_RGB_565:
		fmt = PIPE_FORMAT_B5G6R5_UNORM;
		break;
	case HAL_PIXEL_FORMAT_BGRA_8888:
		fmt = PIPE_FORMAT_B8G8R8A8_UNORM;
		break;
	case HAL_PIXEL_FORMAT_YV12:
	case HAL_PIXEL_FORMAT_YCbCr_422_SP:
	case HAL_PIXEL_FORMAT_YCrCb_420_SP:
	default:
		fmt = PIPE_FORMAT_NONE;
		break;
	}

	return fmt;
}

static unsigned get_pipe_bind(int usage)
{
	unsigned bind = PIPE_BIND_SHARED;

	if (usage & GRALLOC_USAGE_HW_TEXTURE)
		bind |= PIPE_BIND_SAMPLER_VIEW;
	if (usage & GRALLOC_USAGE_HW_RENDER)
		bind |= PIPE_BIND_RENDER_TARGET;
	if (usage & GRALLOC_USAGE_HW_FB) {
		bind |= PIPE_BIND_RENDER_TARGET;
		bind |= PIPE_BIND_SCANOUT;
	}

	return bind;
}

#ifndef DRM_FORMAT_MOD_INVALID
#define DRM_FORMAT_MOD_INVALID ((1ULL << 56) - 1)
#endif

static struct pipe_buffer *get_pipe_buffer_locked(struct pipe_manager *pm,
		struct gralloc_drm_handle_t *handle)
{
	struct pipe_buffer *buf;
	struct pipe_resource templ;

	memset(&templ, 0, sizeof(templ));
	templ.format = get_pipe_format(handle->format);
	templ.bind = get_pipe_bind(handle->usage);
	templ.target = PIPE_TEXTURE_2D;

	if (templ.format == PIPE_FORMAT_NONE ||
	    !pm->screen->is_format_supported(pm->screen, templ.format,
				templ.target, 0, 0, templ.bind)) {
		ALOGE("unsupported format 0x%x", handle->format);
		return NULL;
	}

	buf = (struct pipe_buffer *)calloc(1, sizeof(*buf));
	if (!buf) {
		ALOGE("failed to allocate pipe buffer");
		return NULL;
	}

	templ.width0 = handle->width;
	templ.height0 = handle->height;
	templ.depth0 = 1;
	templ.array_size = 1;

	buf->resource =
		pm->screen->resource_create(pm->screen, &templ);
	if (!buf->resource)
		goto fail;

	buf->winsys.type = WINSYS_HANDLE_TYPE_FD;
	if (!pm->screen->resource_get_handle(pm->screen, 0,
				buf->resource, &buf->winsys, 0))
		goto fail;
	handle->prime_fd = (int) buf->winsys.handle;

	return buf;

fail:
	ALOGE("failed to allocate pipe buffer");
	if (buf->resource)
		pipe_resource_reference(&buf->resource, NULL);
	FREE(buf);

	return NULL;
}

static struct gralloc_drm_bo_t *pipe_alloc(struct gralloc_drm_drv_t *drv,
		struct gralloc_drm_handle_t *handle)
{
	struct pipe_manager *pm = (struct pipe_manager *) drv;
	struct pipe_buffer *buf;

	pthread_mutex_lock(&pm->mutex);
	buf = get_pipe_buffer_locked(pm, handle);
	pthread_mutex_unlock(&pm->mutex);

	if (buf) {
		handle->stride = (int) buf->winsys.stride;
		buf->base.handle = handle;
	}

	return &buf->base;
}

static void pipe_free(struct gralloc_drm_drv_t *drv, struct gralloc_drm_bo_t *bo)
{
	struct pipe_manager *pm = (struct pipe_manager *) drv;
	struct pipe_buffer *buf = (struct pipe_buffer *) bo;

	pthread_mutex_lock(&pm->mutex);

	if (buf->transfer)
		pipe_transfer_unmap(pm->context, buf->transfer);
	pipe_resource_reference(&buf->resource, NULL);

	pthread_mutex_unlock(&pm->mutex);

	FREE(buf);
}

static void pipe_destroy(struct gralloc_drm_drv_t *drv)
{
	struct pipe_manager *pm = (struct pipe_manager *) drv;

	if (pm->context)
		pm->context->destroy(pm->context);
	pm->screen->destroy(pm->screen);
	FREE(pm);
}

struct pipe_screen *vc4_screen_create(int fd);

static int pipe_init_screen(struct pipe_manager *pm)
{
	struct pipe_screen *screen = vc4_screen_create(fcntl(pm->fd, F_DUPFD_CLOEXEC, 3));

	if (!screen) {
		ALOGW("failed to create screen");
		return -EINVAL;
	}
	pm->screen = debug_screen_wrap(screen);
	return 0;
}

static struct gralloc_drm_drv_t *drv_create_for_pipe(int fd)
{
	struct pipe_manager *pm;

	pm = calloc(1, sizeof(*pm));
	if (!pm) {
		ALOGE("failed to allocate pipe manager");
		return NULL;
	}

	pm->fd = fd;
	pthread_mutex_init(&pm->mutex, NULL);

	if (pipe_init_screen(pm)) {
		free(pm);
		return NULL;
	}

	pm->base.destroy = pipe_destroy;
	pm->base.alloc = pipe_alloc;
	pm->base.free = pipe_free;

	return &pm->base;
}

static struct gralloc_drm_drv_t *drv = NULL;
static void init_drv_from_fd(int fd)
{
	drv = drv_create_for_pipe(fd);
	if (!drv) {
		ALOGE("unsupported driver");
	}
}

static int fd = -1;
void drm_gralloc_vc4_init(void)
{
	char *path = "/dev/dri/card0";
	fd = open(path, O_RDWR);
	if (fd < 0) {
		ALOGE("failed to open %s", path);
		return;
	}
	init_drv_from_fd(fd);
	if (!drv) {
		close(fd);
	}
}
void drm_gralloc_vc4_destroy()
{
	if (drv)
		drv->destroy(drv);
	close(fd);
}

static struct gralloc_drm_handle_t *create_bo_handle(int width,
		int height, int format, int usage)
{
	struct gralloc_drm_handle_t *handle;

	handle = calloc(1, sizeof(*handle));
	if (!handle)
		return NULL;

	handle->base.version = sizeof(handle->base);
	handle->base.numInts = GRALLOC_DRM_HANDLE_NUM_INTS;
	handle->base.numFds = GRALLOC_DRM_HANDLE_NUM_FDS;

	handle->magic = GRALLOC_DRM_HANDLE_MAGIC;
	handle->width = width;
	handle->height = height;
	handle->format = format;
	handle->usage = usage;
	handle->prime_fd = -1;

	return handle;
}

static int32_t gralloc_drm_pid = 0;

static int gralloc_drm_get_pid(void)
{
	if (unlikely(!gralloc_drm_pid))
		android_atomic_write((int32_t) getpid(), &gralloc_drm_pid);

	return gralloc_drm_pid;
}

struct gralloc_drm_bo_t *vc4_bo_create(
		int width, int height, int format, int usage)
{
	struct gralloc_drm_bo_t *bo;
	struct gralloc_drm_handle_t *handle;

	handle = create_bo_handle(width, height, format, usage);
	if (!handle)
		return NULL;

	bo = drv->alloc(drv, handle);
	if (!bo) {
		free(handle);
		return NULL;
	}

	bo->imported = 0;
	bo->handle = handle;
	bo->fb_id = 0;
	bo->refcount = 1;

	handle->data_owner = gralloc_drm_get_pid();
	handle->data = bo;

	return bo;
}

void vc4_bo_destroy(struct gralloc_drm_bo_t *bo)
{
	drv->free(drv, bo);
}


