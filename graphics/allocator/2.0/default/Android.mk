LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := android.hardware.graphics.allocator@2.0-service.rpi3
LOCAL_VENDOR_MODULE := true
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_MODULE_TAGS := optional

LOCAL_INIT_RC := android.hardware.graphics.allocator@2.0-service.rpi3.rc

LOCAL_SRC_FILES := \
        drm_gralloc_vc4.c \
        drm_gralloc_rpi3.cpp \
        Allocator.cpp \
        service.cpp

LOCAL_SHARED_LIBRARIES := \
        android.hardware.graphics.allocator@2.0 \
        libhidlbase \
        libhidltransport \
        libbase \
        libutils \
        libcutils \
        liblog \
        libui \
        libdrm \
        libgralloc_drm

LOCAL_HEADER_LIBRARIES := android.hardware.graphics.mapper@2.0-passthrough_headers

LOCAL_C_INCLUDES := \
        external/drm_gralloc \
        external/libdrm \
        external/libdrm/include/drm \
        system/core/libgrallocusage/include \
        system/core/libutils/include

LOCAL_C_INCLUDES += \
	    external/mesa3d/include \
	    external/mesa3d/src \
	    external/mesa3d/src/gallium \
	    external/mesa3d/src/gallium/include \
	    external/mesa3d/src/gallium/winsys \
	    external/mesa3d/src/gallium/drivers \
	    external/mesa3d/src/gallium/auxiliary
	    
LOCAL_STATIC_LIBRARIES += \
        libmesa_winsys_vc4 \
        libmesa_pipe_vc4 \
        libmesa_gallium \
        libmesa_glsl \
        libmesa_glsl_utils \
        libmesa_nir \
        libmesa_util \
        libmesa_compiler
        
LOCAL_SHARED_LIBRARIES += \
        libexpat \
        libz

LOCAL_CFLAGS += \
        -Wall \
        -Werror \
        -Wno-unused-variable \
        -Wno-unused-parameter

include $(BUILD_EXECUTABLE)
