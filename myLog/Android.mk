LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
        test-log.cpp \
        android/logger_write.cpp \
        android/properties.cpp
        

# LOCAL_SHARED_LIBRARIES := \
# 		liblog
LOCAL_SHARED_LIBRARIES := \
		libcutils libbase

LOCAL_C_INCLUDES := \
    android \
    utils \
    $(VENDOR_SDK_INCLUDES) \

LOCAL_MODULE:= my-autohal-log-test
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)

