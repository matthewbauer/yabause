LOCAL_PATH := $(call my-dir)
DYNAREC := 0
HAVE_GRIFFIN := 1

include $(CLEAR_VARS)

APP_DIR := ../../src

LOCAL_MODULE    := retro

ifeq ($(TARGET_ARCH),arm)
LOCAL_CFLAGS += -DANDROID_ARM
LOCAL_ARM_MODE := arm
endif

ifeq ($(TARGET_ARCH),x86)
LOCAL_CFLAGS +=  -DANDROID_X86
endif

ifeq ($(TARGET_ARCH),mips)
LOCAL_CFLAGS += -DANDROID_MIPS -D__mips__ -D__MIPSEL__
endif

YABAUSE_DIR  := ../../yabause
SOURCE_DIR   := $(YABAUSE_DIR)/src
LIBRETRO_DIR := ../

include ../Makefile.common

LOCAL_SRC_FILES    += $(SOURCES) $(SOURCES_C) $(LIBRETRO_SOURCES) $(C68KEXEC_SOURCE)
LOCAL_CFLAGS += -O2 -std=gnu99 -DINLINE=inline -LSB_FIRST -D__LIBRETRO__ -DFRONTEND_SUPPORTS_RGB565 -DVERSION=\"0.9.12\" -DUSE_SCSP2=1 -DNO_CLI -DHAVE_GETTIMEOFDAY -I$(SOURCE_DIR) -DHAVE_STRCASECMP

include $(BUILD_SHARED_LIBRARY)
