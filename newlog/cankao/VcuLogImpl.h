#ifndef DIAGNOSTIC_VCU_LOG_IMPL_H
#define DIAGNOSTIC_VCU_LOG_IMPL_H

#include <stdint.h>
#include <sys/types.h>

#include <utils/Thread.h>
#include <utils/AndroidThreads.h>
#include <utils/RefBase.h>
#include <utils/Looper.h>
#include <utils/Mutex.h>
#include <binder/IBinder.h>
#include <utils/Vector.h>
#include <stdio.h>
#include <cutils/properties.h>
#include <fstream>
#include <string>
#include <set>
#include <queue>
#include <vector>
#include <unordered_map>
#include <cutils/sockets.h>

#include "json/json.h"
#include "Constant.h"
#include "EcuModuleBase.h"

using std::string;
using std::set;
using std::queue;
using std::vector;
using std::unordered_map;

namespace android {
/* ---------------------------------------------------------------------------*/

class VcuLogHandlerThread;
class BlackBoxHandlerThread;
class BlackBoxRealTimeSaveHandlerThread;
class VisualDataThread;
class EcuModuleBase;

class VcuLogImpl : public Thread
{
public:
    /**
     * @brief vcu 故障上报开关
     */
    bool mVcuAprSwitch = false;
    /**
     * @brief 同一个error一天上传几次
     */
    int mErrorCount = 10;
    /**
     * @brief 缓存多久的数据，单位秒
     */
    int mAprBufferTime = 30;
    /**
     * @brief 故障信号之后继续保存多久的数据，单位秒
     */
    int mAprSavaTime = 30;
    /**
     * @brief 一次最多保存多久的数据，单位秒
     */
    int mAprMaxSavaTime = 300;
    /**
     * @brief vcu 黑盒子开关状态 0关闭 1实时落盘 2定时落盘
     */
    int mVcuBlackBoxSwitch = 0;

    sp<VcuLogHandlerThread> mHandler;
    sp<BlackBoxHandlerThread> mBlackBoxHandler;
    sp<VisualDataThread> mVisualDataThread;

    std::vector< sp<EcuModuleBase> > mEcuVector;
    std::unordered_map<int, int> mVcuAllCanids;
    std::unordered_map<int, int> mEcuAllCanids;

public:
    void initEcuModules();
    void distributeSpiData(const char* p, int len);
    bool isVcuCanid(int canid) {
        Mutex::Autolock _l(mVcuAllCanidsLock);
        return mVcuAllCanids.find(canid) == mVcuAllCanids.end() ? false : true;
    };
    void addVcuCanidMap(int canid, int route) {
        Mutex::Autolock _l(mVcuAllCanidsLock);
        mVcuAllCanids.insert(std::pair<int, int>(canid, route));
    };
    void clearVcuCanidMap() {
        Mutex::Autolock _l(mVcuAllCanidsLock);
        mVcuAllCanids.clear();
    };
    bool isEcuCanid(int canid) {
        Mutex::Autolock _l(mEcuAllCanidsLock);
        return mEcuAllCanids.find(canid) == mEcuAllCanids.end() ? false : true;
    };
    void addEcuCanidMap(int canid, int route) {
        Mutex::Autolock _l(mEcuAllCanidsLock);
        mEcuAllCanids.insert(std::pair<int, int>(canid, route));
    };
    void clearEcuCanidMap() {
        Mutex::Autolock _l(mEcuAllCanidsLock);
        mEcuAllCanids.clear();
    };
    std::unordered_map<int, int> getAllCanidMap() {
        Mutex::Autolock _l(mVcuAllCanidsLock);
        Mutex::Autolock _ll(mEcuAllCanidsLock);
        std::unordered_map<int, int> ret;
        for (auto it = mVcuAllCanids.begin(); it != mVcuAllCanids.end(); it++)
        {
            ret.insert(std::pair<int, int>(it->first, it->second));
        }
        for (auto it = mEcuAllCanids.begin(); it != mEcuAllCanids.end(); it++)
        {
            ret.insert(std::pair<int, int>(it->first, it->second));
        }
        return ret;
    };

public:
    explicit VcuLogImpl();
    ~VcuLogImpl();

    void init();
    void setConfigToMCU(Json::Value value);
    void setConfigToMCU(std::unordered_map<int, int> canid);
    void setErrorBlacklist(Json::Value value);
    void clearConfigToMCU();
    void queueData(const char* p, int len);
    string formatData(const char* data, int len);//canfd
    string formatCanData(const char* data, int len);
    void clearOvertimeData(bool forceClean);
    bool isDataOvertime(string head, string tail);
    void packData();
    void printFrontAndBack(std::string info);
    double getTimestamp(string data);
    void onUploadFlagChanged(int value);
    void onUploadReasonChanged(int value);
    void onErrorPartsChanged(int value);
    void setLogLevel(int level);
    void clearErrorSet();

    void setVcuAprSwitch(int s) {
        mVcuAprSwitch = s == VCU_LOG_SWITCH_ON ? true : false;
    }
    void setErrorCount(int s) {
        mErrorCount = s;
    }
    void setVcuAprBufferTime(int s) {
        mAprBufferTime = s;
    }
    void setVcuAprSaveTime(int s) {
        mAprSavaTime = s;
    }
    void setVcuAprSaveMaxTime(int s) {
        mAprMaxSavaTime = s;
    }
    void setVcuBlackboxSwitch(int s) {
        mVcuBlackBoxSwitch = s;
    }
    void setConfigToMCU();
    bool shouldErrorUpload(std::string error);
    void updateErrorRecord(std::string error);

private:
    virtual bool threadLoop();
    string createFilepath(string filename);
    string createFileName(string error);
    void addErrorSet(int error);
    string formatErrorSet();
    void addErrorPartSet(int errorPart);
    std::string formatErrorPartSet();
    void doClearOvertimeData();

    Mutex mQueueLock;
    Mutex mErrorSetLock;
    Mutex mErrorPartLock;
    Mutex mErrorRecordLock;
    Mutex mErrorBlacklistLock;
    Mutex mEcuVectorLock;
    Mutex mVcuAllCanidsLock;
    Mutex mEcuAllCanidsLock;
};

class VcuLogHandlerThread :
            public MessageHandler,
            public Thread
{
public:
    sp<Looper> mLooper;
private:
    std::set<int> mMessageSet;
    Mutex mMessageSetLock;
    sp<VcuLogImpl> mVcuLogImpl;
private:
    virtual bool threadLoop();
    void addMessageSet(int what) {
        Mutex::Autolock _l(mMessageSetLock);
        mMessageSet.insert(what);
    }
    void removeMessageSet(int what) {
        Mutex::Autolock _l(mMessageSetLock);
        mMessageSet.erase(what);
    }
    void removeAllMessageSet() {
        Mutex::Autolock _l(mMessageSetLock);
        mMessageSet.clear();
    }
public:
    explicit VcuLogHandlerThread(sp<VcuLogImpl> v);
    /**
     * Handles a message.
     */
    virtual void handleMessage(const Message& message);

    void sendMessage(const Message& message) {
        mLooper->sendMessage(this, message);
        addMessageSet(message.what);
    }
    void sendMessageDelayed(nsecs_t uptimeDelay, const Message& message) {
        mLooper->sendMessageDelayed(uptimeDelay, this, message);
        addMessageSet(message.what);
    }
    void sendMessageAtTime(nsecs_t uptime, const Message& message) {
        mLooper->sendMessageAtTime(uptime, this, message);
        addMessageSet(message.what);
    }
    void removeAllMessages() {
        mLooper->removeMessages(this);
        removeAllMessageSet();
    }
    void removeMessages(int what) {
        mLooper->removeMessages(this, what);
        removeMessageSet(what);
    }
    bool hasMessage(int what) {
        Mutex::Autolock _l(mMessageSetLock);
        return mMessageSet.find(what) != mMessageSet.end();
    }
    bool isEmpty() {
        Mutex::Autolock _l(mMessageSetLock);
        return mMessageSet.empty();
    }
    bool isBuffering() {
        return hasMessage(MSG_STOP_BUFFER) || hasMessage(MSG_FORCE_STOP_BUFFER);
    }

protected:
    ~VcuLogHandlerThread() {}
};

/*
    VCU黑盒子
*/
class BlackBoxHandlerThread :
            public MessageHandler,
            public Thread
{
public:
    sp<Looper> mLooper;
    /**
     * @brief 压缩落盘的时间间隔，单位秒，默认60秒
     */
    int mInterval = 60;
    /**
     * @brief 压缩落盘，创建新文件的时间间隔，单位分钟，默认30分钟创建一个新文件
     */
    int mCreateFileInterval = 30;
    /**
     * @brief 压缩落盘，创建新文件，默认 30MB 创建一个新文件
     */
    int mCreateFileSize = 30;
    /**
     * @brief 实时落盘的时间间隔，单位ms，默认1000
     * 
     */
    int mRealtimeInterval = 1000;
    /**
     * @brief 实时落盘的单个文件大小 MB
     */
    int mRealtimeSize = 30;
    /**
     * @brief 最大占用存储空间 单位MB
     */
    int mMaxSize = 3072;
    sp<VcuLogImpl> mVcuLogImpl;

    int mTestDataCount = 160000;

private:
    std::set<int> mMessageSet;
    Mutex mMessageSetLock;
    std::queue<std::string> mBlackBoxDataQueue;
    Mutex mBlackBoxDataQueueLock;
    std::queue<std::string> mBlackBoxAscDataQueue;
    Mutex mBlackBoxAscDataQueueLock;

    int mTestType;

    FILE* rtFile;
    int rtSize;
    Mutex mRtFileLock;

    std::string mCurrentBlackboxFilepath;

    /**
     * @brief 用来模拟更多数据量，对实际数据进行加倍
     */
    int mDataX = 0;

public:
    explicit BlackBoxHandlerThread(sp<VcuLogImpl> v);
    /**
     * Handles a message.
     */
    virtual void handleMessage(const Message& message);

    void sendMessage(const Message& message) {
        mLooper->sendMessage(this, message);
        addMessageSet(message.what);
    }
    void sendMessageDelayed(nsecs_t uptimeDelay, const Message& message) {
        mLooper->sendMessageDelayed(uptimeDelay, this, message);
        addMessageSet(message.what);
    }
    void sendMessageAtTime(nsecs_t uptime, const Message& message) {
        mLooper->sendMessageAtTime(uptime, this, message);
        addMessageSet(message.what);
    }
    void removeAllMessages() {
        mLooper->removeMessages(this);
        removeAllMessageSet();
    }
    void removeMessages(int what) {
        mLooper->removeMessages(this, what);
        removeMessageSet(what);
    }
    bool hasMessage(int what) {
        Mutex::Autolock _l(mMessageSetLock);
        return mMessageSet.find(what) != mMessageSet.end();
    }
    bool isEmpty() {
        Mutex::Autolock _l(mMessageSetLock);
        return mMessageSet.empty();
    }

    void enqueueData(std::string s) {
        Mutex::Autolock _l(mBlackBoxDataQueueLock);
        mBlackBoxDataQueue.push(s);
    }
    std::string dequeueData() {
        Mutex::Autolock _l(mBlackBoxDataQueueLock);
        string tmp;
        if (!mBlackBoxDataQueue.empty())
        {
            tmp = mBlackBoxDataQueue.front();
            mBlackBoxDataQueue.pop();
        }
        return tmp;
    }
    void enqueueAscData(std::string s) {
        Mutex::Autolock _l(mBlackBoxAscDataQueueLock);
        mBlackBoxAscDataQueue.push(s);
    }
    void setInterval(int i) {
        mInterval = i;
    }
    void setCreateFileInterval(int i) {
        mCreateFileInterval = i;
    }
    void setCreateFileSize(int i) {
        mCreateFileSize = i;
    }
    void setDataX(int i) {
        mDataX = i;
    }
    void setRealtimeInterval(int i) {
        mRealtimeInterval = i;
    }
    void setRealtimeSize(int i) {
        mRealtimeSize = i;
    }
    void setBlackboxMaxSize(int i) {
        mMaxSize = i;
    }
    void init();
    void setTestDataCount(int i) {
        mTestDataCount = i;
    }
    void testSavaData(int type) {
        mTestType = type;
        sendMessage(Message(MSG_BLACK_BOX_WRITE_TEST));
    }
    std::string generateFilepath(std::string filename);
    std::string generateFilename();
    void endSaveDataRealtime();
    void findBlackboxSpace(long size/*KB*/);

private:
    virtual bool threadLoop();
    void doSaveDataInterval();
    void doSaveDataRealtime();
    void doTestSavaData(int type);
    void clearData(queue<std::string>& q) {
        std::queue<std::string> empty;
        q.swap(empty);
    }
    void addMessageSet(int what) {
        Mutex::Autolock _l(mMessageSetLock);
        mMessageSet.insert(what);
    }
    void removeMessageSet(int what) {
        Mutex::Autolock _l(mMessageSetLock);
        mMessageSet.erase(what);
    }
    void removeAllMessageSet() {
        Mutex::Autolock _l(mMessageSetLock);
        mMessageSet.clear();
    }

protected:
    ~BlackBoxHandlerThread() {}
};

/*
    数据可视化，使用socket传数据给应用
*/
class VisualDataThread :
            public MessageHandler,
            public Thread
{
public:
    sp<Looper> mLooper;
    sp<VcuLogImpl> mVcuLogImpl;

private:
    std::set<int> mMessageSet;
    Mutex mMessageSetLock;
    std::queue<std::string> mVisualDataQueue;
    Mutex mVisualDataQueueLock;

    bool mVisualDataSwitch = false;
    cutils_socket_t server_fd;
    int client_fd;

public:
    explicit VisualDataThread(sp<VcuLogImpl> v);
    /**
     * Handles a message.
     */
    virtual void handleMessage(const Message& message);

    void sendMessage(const Message& message) {
        mLooper->sendMessage(this, message);
        addMessageSet(message.what);
    }
    void sendMessageDelayed(nsecs_t uptimeDelay, const Message& message) {
        mLooper->sendMessageDelayed(uptimeDelay, this, message);
        addMessageSet(message.what);
    }
    void sendMessageAtTime(nsecs_t uptime, const Message& message) {
        mLooper->sendMessageAtTime(uptime, this, message);
        addMessageSet(message.what);
    }
    void removeAllMessages() {
        mLooper->removeMessages(this);
        removeAllMessageSet();
    }
    void removeMessages(int what) {
        mLooper->removeMessages(this, what);
        removeMessageSet(what);
    }
    bool hasMessage(int what) {
        Mutex::Autolock _l(mMessageSetLock);
        return mMessageSet.find(what) != mMessageSet.end();
    }
    bool isEmpty() {
        Mutex::Autolock _l(mMessageSetLock);
        return mMessageSet.empty();
    }

    void enqueueData(std::string s) {
        Mutex::Autolock _l(mVisualDataQueueLock);
        mVisualDataQueue.push(s);
    }
    void init();
    int doSendData();
    void doEndSocket();
    bool getVisualDataSwitch() {
        return mVisualDataSwitch;
    }

private:
    virtual bool threadLoop();
    void clearData(queue<std::string>& q) {
        std::queue<std::string> empty;
        q.swap(empty);
    }
    void addMessageSet(int what) {
        Mutex::Autolock _l(mMessageSetLock);
        mMessageSet.insert(what);
    }
    void removeMessageSet(int what) {
        Mutex::Autolock _l(mMessageSetLock);
        mMessageSet.erase(what);
    }
    void removeAllMessageSet() {
        Mutex::Autolock _l(mMessageSetLock);
        mMessageSet.clear();
    }

protected:
    ~VisualDataThread() {}
};



// ---------------------------------------------------------------------------
}; // namespace android

#endif
