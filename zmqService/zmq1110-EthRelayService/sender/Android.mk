LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
       main_sender.cpp \
       ../EthRelayService.cpp

LOCAL_C_INCLUDES := \
       $(LOCAL_PATH)/../zmq \
       $(LOCAL_PATH)/../ \

LOCAL_SHARED_LIBRARIES := libzmq

LOCAL_CFLAGS += -Wall -Wextra -Wno-unused-parameter

LOCAL_MODULE:= my_zmq_sender
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)