LOCAL_PATH := $(call my-dir)

# ============================================================
# Dobby (مكتبة ثابتة جاهزة)
# ============================================================
include $(CLEAR_VARS)
LOCAL_MODULE            := dobby
LOCAL_SRC_FILES         := ../third_party/lib/$(TARGET_ARCH_ABI)/libdobby.a
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/../third_party/include
include $(PREBUILT_STATIC_LIBRARY)

# ============================================================
# Erbium Hook
# ============================================================
include $(CLEAR_VARS)
LOCAL_MODULE            := hook
LOCAL_SRC_FILES         := hook.cpp
LOCAL_LDLIBS            := -llog -ldl
LOCAL_CPPFLAGS          := -std=c++17 -O2 -fexceptions -frtti
LOCAL_STATIC_LIBRARIES  := dobby
include $(BUILD_SHARED_LIBRARY)
