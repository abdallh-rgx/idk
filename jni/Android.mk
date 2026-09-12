LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE     := erbium_hook
LOCAL_SRC_FILES  := hook.cpp
LOCAL_LDLIBS     := -llog -ldl
LOCAL_CPPFLAGS   := -std=c++17 -O2 -fexceptions -frtti -fvisibility=hidden
LOCAL_CPP_FEATURES := exceptions rtti
include $(BUILD_SHARED_LIBRARY)
