LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := libzmq
LOCAL_SRC_FILES := libzmq.so
include $(PREBUILT_SHARED_LIBRARY)