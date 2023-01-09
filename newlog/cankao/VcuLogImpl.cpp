#include "VcuLogImpl.h"
#include "SpiCommon.h"
#include "HttpCommon.h"
#include "FileCommon.h"
#include "EcuModuleBase.h"

#include <queue>
#include <set>
#include <fstream>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <utils/Atomic.h>
#include <sys/stat.h>
#include <cutils/sockets.h>
#include <errno.h>
#include <vector>
#include <limits.h>

#include <android-base/properties.h>
#include <android-base/logging.h>
#include <bydauto/BYDAutoManager.h>
#include <hardware/auto_native_ids_all.h>

#include <ziparchive/zip_writer.h>

using std::string;
using std::stoi;
using std::ofstream;
using std::ifstream;
using std::queue;
using std::map;
using std::set;

namespace android {

sp<SpiCommon> mSpiCommon;
char mData[DIAG_READ_SIZE_MAX];
unsigned int count = 0; // max 4294967295
unsigned int lenCount = 0;

queue<string> mBufQueue;
set<int> mErrorRecord;
set<int> mErrorPartRecord;
set<std::string> mErrorBlacklist;

// const bool isCanFD = (strcmp(android::base::GetProperty("sys.car.protocol", "").c_str(), "CANFD") == 0)
//     || (strcmp(android::base::GetProperty("persist.sys.protocol.record", "").c_str(), "CANFD") == 0);
const bool isCanFD = false;

/* -----------------------------------BlackBoxHandlerThread start----------------------------------------*/

BlackBoxHandlerThread::BlackBoxHandlerThread(sp<VcuLogImpl> v) {
    LOG1("BlackBoxHandlerThread::%s enter", __func__);
    mVcuLogImpl = v;
    mLooper = new Looper(true);
    mLooper->prepare(true);
    Looper::setForThread(mLooper);
    run("BlackBoxHandlerThread");
    rtFile = NULL;
    rtSize = 0;
    init();
    LOG1("BlackBoxHandlerThread::%s exit", __func__);
}

bool BlackBoxHandlerThread::threadLoop() {
    if (mLooper)
    {
        // LOG3("VcuLogHandlerThread::%s pollOnce", __func__);
        mLooper->pollOnce(-1);
    }
    return true;
}

void BlackBoxHandlerThread::init() {
    int blackboxSwitch = mVcuLogImpl->mVcuBlackBoxSwitch;
    LOG1("BlackBoxHandlerThread::%s blackboxSwitch =%d", __func__, blackboxSwitch);
    removeAllMessages();
    if (blackboxSwitch == BLACK_BOX_SAVE_INTERVAL)
    {
        sendMessageDelayed(seconds_to_nanoseconds(mInterval), Message(MSG_BLACK_BOX_WRITE));
        // sendMessageDelayed(seconds_to_nanoseconds(mCreateFileInterval * 60), Message(MSG_BLACK_BOX_CREATE_FILE));
        endSaveDataRealtime();
    } else if (blackboxSwitch == BLACK_BOX_SAVE_REAL_TIME)
    {
        sendMessageDelayed(milliseconds_to_nanoseconds(mRealtimeInterval), Message(MSG_BLACK_BOX_WRITE_REAL_TIME));
    }
}

void BlackBoxHandlerThread::handleMessage(const Message& message) {
    switch (message.what)
    {
    case MSG_BLACK_BOX_WRITE:
        LOG1("BlackBoxHandlerThread::%s MSG_BLACK_BOX_WRITE", __func__);
        doSaveDataInterval();
        if (mVcuLogImpl->mVcuBlackBoxSwitch == BLACK_BOX_SAVE_INTERVAL)
        {
            removeMessages(MSG_BLACK_BOX_WRITE);
            sendMessageDelayed(seconds_to_nanoseconds(mInterval), Message(MSG_BLACK_BOX_WRITE));
        }
        LOG1("BlackBoxHandlerThread::%s MSG_BLACK_BOX_WRITE over mInterval=%d", __func__, mInterval);
        break;
    case MSG_BLACK_BOX_WRITE_TEST:
        LOG1("BlackBoxHandlerThread::%s MSG_BLACK_BOX_WRITE_TEST mTestType=%d", __func__, mTestType);
        doTestSavaData(mTestType);
        break;
    case MSG_BLACK_BOX_WRITE_REAL_TIME:
        LOG3("BlackBoxHandlerThread::%s MSG_BLACK_BOX_WRITE_REAL_TIME", __func__);
        if (mVcuLogImpl->mVcuBlackBoxSwitch == BLACK_BOX_SAVE_REAL_TIME)
        {
            doSaveDataRealtime();
            removeMessages(MSG_BLACK_BOX_WRITE_REAL_TIME);
            sendMessageDelayed(milliseconds_to_nanoseconds(mRealtimeInterval), Message(MSG_BLACK_BOX_WRITE_REAL_TIME));
        }
        break;
    // case MSG_BLACK_BOX_CREATE_FILE:
    //     LOG1("BlackBoxHandlerThread::%s MSG_BLACK_BOX_CREATE_FILE", __func__);
    //     mCurrentBlackboxFilepath.clear();
    //     if (mVcuLogImpl->mVcuBlackBoxSwitch == BLACK_BOX_SAVE_INTERVAL)
    //     {
    //         removeMessages(MSG_BLACK_BOX_CREATE_FILE);
    //         sendMessageDelayed(seconds_to_nanoseconds(mCreateFileInterval * 60), Message(MSG_BLACK_BOX_CREATE_FILE));
    //     }
    //     break;
    default: break;
    }
}

void BlackBoxHandlerThread::doSaveDataInterval() {
    LOG3("BlackBoxHandlerThread::%s enter", __func__);
    mBlackBoxDataQueueLock.lock();
    LOG1("BlackBoxHandlerThread::%s mBlackBoxDataQueue size=%lu", __func__, mBlackBoxDataQueue.size());
    std::queue<std::string> data;
    mBlackBoxDataQueue.swap(data);
    clearData(mBlackBoxDataQueue);
    LOG1("BlackBoxHandlerThread::%s mBlackBoxDataQueue size=%lu", __func__, mBlackBoxDataQueue.size());
    mBlackBoxDataQueueLock.unlock();

    if (data.empty()) {
        LOG1("BlackBoxHandlerThread::%s data is empty", __func__);
        return;
    }

    std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds> tpMicro1
            = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
    time_t begin = tpMicro1.time_since_epoch().count();

    std::vector<uint8_t> src, dst;
    while (!data.empty())
    {
        string tmp = data.front();
        src.insert(src.end(), tmp.begin(), tmp.end());
        if (mDataX > 0)
        {
            for (size_t i = 0; i < mDataX; i++)
            {
                src.insert(src.end(), tmp.begin(), tmp.end());
            }
        }
        data.pop();
    }

    XzCompress(src, &dst, 1);

    findBlackboxSpace(dst.size() / KB);

    if (mCurrentBlackboxFilepath.empty() || !isFileExist(mCurrentBlackboxFilepath) || getFileSize(mCurrentBlackboxFilepath) > (mCreateFileSize * KB) )
    {
        mCurrentBlackboxFilepath = generateFilepath(generateFilename());
    }

    FILE* fout = fopen(mCurrentBlackboxFilepath.c_str(), "a");
    if (fout == NULL)
    {
        ALOGE("VcuLogImpl::%s create file %s failed errno=%d strerror(errno) = %s", __func__, mCurrentBlackboxFilepath.c_str(), errno, strerror(errno));
        return;
    }
    int len = dst.size();
    unsigned char lenPrefix[4] = {'\0'};
    lenPrefix[0] = (len>>24)&0xFF;
    lenPrefix[1] = (len>>16)&0xFF;
    lenPrefix[2] = (len>>8)&0xFF;
    lenPrefix[3] = len&0xFF;
    int ret = fwrite(lenPrefix, 4, 1, fout);
    ret += fwrite(dst.data(), dst.size(), 1, fout);
    if (ret != 2)
    {
        ALOGE("VcuLogImpl::%s fwrite file %s failed len=%d ret=%d errno=%d strerror(errno) = %s", __func__, mCurrentBlackboxFilepath.c_str(), len, ret, errno, strerror(errno));
    }
    fclose(fout);
    LOG3("BlackBoxHandlerThread::%s fwrite ret=%d", __func__, ret);

    std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds> tpMicro2
            = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
    time_t end = tpMicro2.time_since_epoch().count();
    LOG1("BlackBoxHandlerThread::%s beginning data size=%lu cost time = %ld", __func__, data.size(), end -begin);
}

void BlackBoxHandlerThread::doSaveDataRealtime() {
    LOG3("BlackBoxHandlerThread::%s enter", __func__);
    mBlackBoxDataQueueLock.lock();
    LOG3("BlackBoxHandlerThread::%s mBlackBoxDataQueue size=%lu", __func__, mBlackBoxDataQueue.size());
    std::queue<std::string> data;
    mBlackBoxDataQueue.swap(data);
    clearData(mBlackBoxDataQueue);
    LOG3("BlackBoxHandlerThread::%s mBlackBoxDataQueue size=%lu", __func__, mBlackBoxDataQueue.size());
    mBlackBoxDataQueueLock.unlock();

    if (data.empty()) {
        LOG1("BlackBoxHandlerThread::%s data is empty", __func__);
        return;
    }

    std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds> tpMicro1
            = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
    time_t begin = tpMicro1.time_since_epoch().count();

    Mutex::Autolock _l(mRtFileLock);
    while (!data.empty())
    {
        string tmp = data.front();
        data.pop();
        if (rtFile != NULL)
        {
            fwrite(tmp.data(), tmp.size(), 1, rtFile);
            rtSize+=tmp.size();
            if ( rtSize >= mRealtimeSize * 1024 * 1024) {
                endSaveDataRealtime();
            }
        }
        else
        {
            std::string filename = generateFilename();
            std::string filepath = generateFilepath(filename.append("-rt"));
            filepath.pop_back();
            filepath.pop_back();
            filepath.pop_back();
            findBlackboxSpace(mRealtimeSize * KB);
            rtFile = fopen(filepath.c_str(), "a+");
            if (rtFile == NULL)
            {
                ALOGE("BlackBoxRealTimeSaveHandlerThread::%s create file failed errno=%d strerror(errno) = %s", __func__, errno, strerror(errno));
                return;
            }
            else
            {
                fwrite(tmp.data(), tmp.size(), 1, rtFile);
                rtSize+=tmp.size();
                if ( rtSize >= mRealtimeSize * 1024 * 1024) {
                    endSaveDataRealtime();
                }
            }
        }
    }

    std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds> tpMicro2
            = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
    time_t end = tpMicro2.time_since_epoch().count();
    LOG3("BlackBoxHandlerThread::%s beginning data size=%lu cost time = %ld", __func__, data.size(), end -begin);
}

void BlackBoxHandlerThread::endSaveDataRealtime() {
    LOG1("BlackBoxHandlerThread::%s enter", __func__);
    Mutex::Autolock _l(mRtFileLock);
    if (rtFile != NULL)
    {
        fclose(rtFile);
    }
    rtFile = NULL;
    rtSize = 0;
}

void BlackBoxHandlerThread::doTestSavaData(int type) {
    std::queue<std::string> originQueue;
    std::queue<std::string> originQueue2;
    std::queue<std::string> ascQueue;
    std::queue<std::string> ascQueue2;
    for (size_t i = 0; i < mTestDataCount; i++)
    {
        // 模拟跟实车压缩率相同的数据，数据越随机，压缩率越低
        char init_data[] = {
            0x99, 0x00, 0x02, 0x00, 0x0F,
            0x00, 0x00, 0x00, 0x00,
            0x02, 0x01, 0x01,
            0xAA, 0x00, 0x01, 0x45, 0x01, 0x01, 0xAA, 0x00,
        };
        init_data[5] = (i >> 24) & 0xFF;
        init_data[6] = (i >> 16) & 0xFF;
        init_data[7] = (i >> 8) & 0xFF;
        init_data[8] = i & 0xFF;
        init_data[9] = (rand() % 0xF) & 0xF;
        init_data[10] = (rand() % 0xF) & 0xF;
        init_data[12] = (rand() % 0xFF) & 0xFF;
        // init_data[13] = (rand() % 0xF) & 0xF;
        // init_data[14] = (rand() % 0xFF) & 0xFF;
        // init_data[15] = (rand() % 0xFF) & 0xFF;
        // init_data[16] = (rand() % 0xFF) & 0xFF;
        // init_data[17] = (rand() % 0xFF) & 0xFF;
        // init_data[18] = (rand() % 0xFF) & 0xFF;
        // init_data[19] = (rand() % 0xFF) & 0xFF;
        std::string s(init_data+4, 16);
        originQueue.push(s);
        ascQueue.push(mVcuLogImpl->formatCanData(init_data+5, 15));
    }
    for (size_t i = mTestDataCount; i < mTestDataCount*2; i++)
    {
        // 模拟跟实车压缩率相同的数据，数据越随机，压缩率越低
        char init_data[] = {
            0x99, 0x00, 0x02, 0x00, 0x0F,
            0x00, 0x00, 0x00, 0x00,
            0x02, 0x01, 0x01,
            0xAA, 0x00, 0x01, 0x45, 0x01, 0x01, 0xAA, 0x00,
        };
        init_data[5] = (i >> 24) & 0xFF;
        init_data[6] = (i >> 16) & 0xFF;
        init_data[7] = (i >> 8) & 0xFF;
        init_data[8] = i & 0xFF;
        init_data[9] = (rand() % 0xF) & 0xF;
        init_data[10] = (rand() % 0xF) & 0xF;
        // init_data[12] = (rand() % 0xFF) & 0xFF;
        // init_data[13] = (rand() % 0xF) & 0xF;
        init_data[14] = (rand() % 0xFF) & 0xFF;
        // init_data[15] = (rand() % 0xFF) & 0xFF;
        // init_data[16] = (rand() % 0xFF) & 0xFF;
        // init_data[17] = (rand() % 0xFF) & 0xFF;
        // init_data[18] = (rand() % 0xFF) & 0xFF;
        // init_data[19] = (rand() % 0xFF) & 0xFF;
        std::string s(init_data+4, 16);
        originQueue2.push(s);
        ascQueue2.push(mVcuLogImpl->formatCanData(init_data+5, 15));
    }
    sleep(5);//等cpu降下来
    if (type == 0) //原始数据deflate压缩
    {
        std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds> tpMicro1
            = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
        time_t begin = tpMicro1.time_since_epoch().count();

        ALOGE("BlackBoxHandlerThread::%s 0 begin size=%lu", __func__, originQueue.size());
        FILE* file = fopen("/data/system/diag/blackbox/BlackBoxDeflateOrigin.zip", "wb");
        ZipWriter writer(file);
        writer.StartEntry("BlackBoxDeflateOrigin", ZipWriter::kCompress | ZipWriter::kAlign32);
        while (!originQueue.empty())
        {
            string tmp = originQueue.front();
            writer.WriteBytes(tmp.c_str(), tmp.length());
            originQueue.pop();
        }
        writer.FinishEntry();
        writer.Finish();
        fclose(file);

        std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds> tpMicro2
            = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
        time_t end = tpMicro2.time_since_epoch().count();
        ALOGE("BlackBoxHandlerThread::%s 0 end size=%lu cost time = %ld", __func__, originQueue.size(), end -begin);
    }
    else if (type == 1) //asc数据deflate压缩
    {
        std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds> tpMicro1
            = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
        time_t begin = tpMicro1.time_since_epoch().count();

        ALOGE("BlackBoxHandlerThread::%s 1 begin size=%lu", __func__, ascQueue.size());
        FILE* file = fopen("/data/system/diag/blackbox/BlackBoxDeflateAsc.zip", "wb");
        ZipWriter writer(file);
        writer.StartEntry("BlackBoxDeflateAsc", ZipWriter::kCompress | ZipWriter::kAlign32);
        while (!ascQueue.empty())
        {
            string tmp = ascQueue.front();
            writer.WriteBytes(tmp.c_str(), tmp.length());
            ascQueue.pop();
        }
        writer.FinishEntry();
        writer.Finish();
        fclose(file);

        std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds> tpMicro2
            = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
        time_t end = tpMicro2.time_since_epoch().count();
        ALOGE("BlackBoxHandlerThread::%s 1 end size=%lu cost time = %ld", __func__, originQueue.size(), end -begin);
    }
    else if (type == 2) //asc数据LZMA压缩
    {
        std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds> tpMicro1
            = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
        time_t begin = tpMicro1.time_since_epoch().count();
        ALOGE("BlackBoxHandlerThread::%s 2 begin size=%lu", __func__, ascQueue.size());

        std::vector<uint8_t> src, dst;
        while (!ascQueue.empty())
        {
            string tmp = ascQueue.front();
            src.insert(src.end(), tmp.begin(), tmp.end());
            ascQueue.pop();
        }

        ALOGE("BlackBoxHandlerThread::%s 2 src size=%lu", __func__, src.size());
        XzCompress(src, &dst, 1);
        ALOGE("BlackBoxHandlerThread::%s 2 dst size=%lu", __func__, dst.size());

        FILE* fout = fopen("/data/system/diag/blackbox/BlackBoxXzAsc.xz", "wb");
        int ret = fwrite(dst.data(), dst.size(), 1, fout);
        fclose(fout);
        ALOGE("BlackBoxHandlerThread::%s 2 fwrite ret=%d", __func__, ret);

        std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds> tpMicro2
            = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
        time_t end = tpMicro2.time_since_epoch().count();
        ALOGE("BlackBoxHandlerThread::%s 2 end size=%lu cost time = %ld", __func__, ascQueue.size(), end -begin);
    }
    else if (type == 3) //原始数据LZMA压缩
    {
        std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds> tpMicro1
            = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
        time_t begin = tpMicro1.time_since_epoch().count();
        ALOGE("BlackBoxHandlerThread::%s 3 begin size=%lu", __func__, originQueue.size());

        std::vector<uint8_t> src, dst;
        while (!originQueue.empty())
        {
            string tmp = originQueue.front();
            src.insert(src.end(), tmp.begin(), tmp.end());
            originQueue.pop();
        }

        ALOGE("BlackBoxHandlerThread::%s 3 src size=%lu", __func__, src.size());
        XzCompress(src, &dst, 1);
        ALOGE("BlackBoxHandlerThread::%s 3 src size=%lu dst size=%lu", __func__, src.size(), dst.size());

        FILE* fout = fopen("/data/system/diag/blackbox/BlackBoxXzOrigin.xz", "wb");
        int ret = fwrite(dst.data(), dst.size(), 1, fout);
        fclose(fout);
        ALOGE("BlackBoxHandlerThread::%s 3 fwrite ret=%d", __func__, ret);

        std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds> tpMicro2
            = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
        time_t end = tpMicro2.time_since_epoch().count();
        ALOGE("BlackBoxHandlerThread::%s 3 end size=%lu cost time = %ld", __func__, originQueue.size(), end -begin);
    }
    else if (type == 5) //原始数据和asc数据同时LZMA压缩
    {
        std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds> tpMicro1
            = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
        time_t begin = tpMicro1.time_since_epoch().count();
        ALOGE("BlackBoxHandlerThread::%s 5-1 begin size=%lu", __func__, originQueue.size());

        std::vector<uint8_t> src, dst;
        while (!originQueue.empty())
        {
            string tmp = originQueue.front();
            src.insert(src.end(), tmp.begin(), tmp.end());
            originQueue.pop();
        }

        ALOGE("BlackBoxHandlerThread::%s 5-1 src size=%lu", __func__, src.size());
        XzCompress(src, &dst, 1);
        ALOGE("BlackBoxHandlerThread::%s 5-1 src size=%lu dst size=%lu", __func__, src.size(), dst.size());

        FILE* fout = fopen("/data/system/diag/blackbox/BlackBoxXzOrigin.xz", "wb");
        int len0 = dst.size();
        unsigned char data0[4] = {'\0'};
        data0[0] = (len0>>24)&0xFF;
        data0[1] = (len0>>16)&0xFF;
        data0[2] = (len0>>8)&0xFF;
        data0[3] = len0&0xFF;
        fwrite(data0, 4, 1, fout);
        int ret = fwrite(dst.data(), dst.size(), 1, fout);
        fclose(fout);
        ALOGE("BlackBoxHandlerThread::%s 5-1 fwrite ret=%d", __func__, ret);

        src.clear();
        dst.clear();
        while (!originQueue2.empty())
        {
            string tmp = originQueue2.front();
            src.insert(src.end(), tmp.begin(), tmp.end());
            originQueue2.pop();
        }
        XzCompress(src, &dst, 1);
        FILE* foutA = fopen("/data/system/diag/blackbox/BlackBoxXzOrigin.xz", "a");
        int len = dst.size();
        unsigned char data[4] = {'\0'};
        data[0] = (len>>24)&0xFF;
        data[1] = (len>>16)&0xFF;
        data[2] = (len>>8)&0xFF;
        data[3] = len&0xFF;
        fwrite(data, 4, 1, foutA);
        int retA = fwrite(dst.data(), len, 1, foutA);
        fclose(foutA);

        std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds> tpMicro2
            = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
        time_t end = tpMicro2.time_since_epoch().count();
        ALOGE("BlackBoxHandlerThread::%s 5-1 end size=%lu cost time = %ld", __func__, originQueue.size(), end -begin);


        std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds> tpMicro12
            = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
        time_t begin2 = tpMicro12.time_since_epoch().count();
        ALOGE("BlackBoxHandlerThread::%s 5-2 begin size=%lu", __func__, ascQueue.size());

        std::vector<uint8_t> src2, dst2;
        while (!ascQueue.empty())
        {
            string tmp = ascQueue.front();
            src2.insert(src2.end(), tmp.begin(), tmp.end());
            ascQueue.pop();
        }
        while (!ascQueue2.empty())
        {
            string tmp = ascQueue2.front();
            src2.insert(src2.end(), tmp.begin(), tmp.end());
            ascQueue2.pop();
        }

        ALOGE("BlackBoxHandlerThread::%s 5-2 src2 size=%lu", __func__, src2.size());
        XzCompress(src2, &dst2, 1);
        ALOGE("BlackBoxHandlerThread::%s 5-2 dst2 size=%lu", __func__, dst2.size());

        FILE* fout2 = fopen("/data/system/diag/blackbox/BlackBoxXzAsc.xz", "wb");
        int ret2 = fwrite(dst2.data(), dst2.size(), 1, fout2);
        fclose(fout2);
        ALOGE("BlackBoxHandlerThread::%s 5-2 fwrite ret=%d", __func__, ret2);

        std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds> tpMicro22
            = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
        time_t end2 = tpMicro22.time_since_epoch().count();
        ALOGE("BlackBoxHandlerThread::%s 5-2 end size=%lu cost time = %ld", __func__, ascQueue.size(), end2 - begin2);
    }

}

std::string BlackBoxHandlerThread::generateFilepath(std::string filename) {
    std::string ret(FILE_PATH_BASE);
    ret.append(RELATIVE_PATH_BLACK_BOX);
    struct stat buffer;
    if (stat(ret.c_str(), &buffer) == -1)
    {
        ALOGE("BlackBoxHandlerThread::%s stat %s failed error=%d %s", __func__, ret.c_str(), errno, strerror(errno));
        if (mkdir(ret.c_str(), 0700) == -1)
        {
            ALOGE("BlackBoxHandlerThread::%s mkdir %s failed error=%d %s", __func__, ret.c_str(), errno, strerror(errno));
            ret = FILE_PATH_BASE;
        }
    }
    return ret.append(filename).append(".xz");
}

/**
 * @brief 获取黑盒子文件名
 * 
 * @return std::string vculog_blackbox_none@2017-1-1-14-12-54
 */
std::string BlackBoxHandlerThread::generateFilename() {
    std::string ret("vculog_blackbox_none@");
    time_t rawtime;
    struct tm * timeinfo;
    time (&rawtime);
    timeinfo = localtime (&rawtime);
    ret.append(std::to_string(timeinfo->tm_year+1900).append("-"));
    ret.append(std::to_string(timeinfo->tm_mon+1).append("-"));
    ret.append(std::to_string(timeinfo->tm_mday).append("-"));
    ret.append(std::to_string(timeinfo->tm_hour).append("-"));
    ret.append(std::to_string(timeinfo->tm_min).append("-"));
    ret.append(std::to_string(timeinfo->tm_sec));
    LOG0("BlackBoxHandlerThread::%s ret=%s", __func__, ret.c_str());
    return ret;
    // return ret.append(".asc");
}

/**
 * @brief 是否有足够空间存储黑盒子数据
 * 
 * @param size KB
 */
void BlackBoxHandlerThread::findBlackboxSpace(long size) {
    std::string dirpath = std::string(FILE_PATH_BASE) + std::string(RELATIVE_PATH_BLACK_BOX);
    long dirSize = getDirSize(dirpath);
    long deviceFreeSize = getDeviceFreeSize();
    LOG1("BlackBoxHandlerThread::%s dirSize=%ld KB, deviceFreeSize=%ld KB, mMaxSize=%d MB, size=%ld KB", __func__, dirSize, deviceFreeSize, mMaxSize, size);
    if (dirSize > mMaxSize * 1024 || deviceFreeSize < size)
    {
        std::map<long, std::string> files = getCreatedTimeFileList(dirpath);
        if (files.empty())
        {
            ALOGE("BlackBoxHandlerThread::%s empty files", __func__);
            return;
        }
        std::vector<long> times;
        for (auto it = files.begin(); it != files.end(); it++)
        {
            times.push_back(it->first);
        }
        std::sort(times.begin(), times.end());//默认升序排序
        auto it = times.begin();
        removeFile(files.at(*it));
        while ( ( getDirSize(dirpath) > mMaxSize * 1024 || deviceFreeSize < size ) && it != times.end())
        {
            it++;
            removeFile(files.at(*it));
        }
    }
}

/* -----------------------------------BlackBoxHandlerThread end---------------------------------------*/

/* -----------------------------------BlackBoxHandlerThread start----------------------------------------*/

VisualDataThread::VisualDataThread(sp<VcuLogImpl> v) {
    LOG1("VisualDataThread::%s enter", __func__);
    mVcuLogImpl = v;
    mLooper = new Looper(true);
    mLooper->prepare(true);
    Looper::setForThread(mLooper);
    run("VisualDataThread");
    sendMessageDelayed(milliseconds_to_nanoseconds(10), Message(MSG_VISUAL_DATA_INIT));
    LOG1("VisualDataThread::%s exit", __func__);
}

bool VisualDataThread::threadLoop() {
    if (mLooper)
    {
        mLooper->pollOnce(-1);
    }
    return true;
}

void VisualDataThread::handleMessage(const Message& message) {
    switch (message.what)
    {
    case MSG_VISUAL_DATA_INIT:
        LOG1("VisualDataThread::%s MSG_VISUAL_DATA_INIT", __func__);
        if (server_fd <= 0)
        {
            server_fd = socket_local_server("vcu_visual_data", ANDROID_SOCKET_NAMESPACE_ABSTRACT, SOCK_STREAM);
        }
        if (server_fd < 0)
        {
            ALOGE("VisualDataThread::%s open socket server failed server_fd=%d errno=%d strerror(errno) = %s", __func__, server_fd, errno, strerror(errno));
            return;
        }
        client_fd = accept(server_fd, NULL, NULL);
        if (client_fd > 0)
        {
            LOG1("VisualDataThread::%s socket accept client_fd=%d", __func__, client_fd);
            // socket_set_receive_timeout(server_fd, 1000);
            if (mVcuLogImpl->mVcuAprSwitch || mVcuLogImpl->mVcuBlackBoxSwitch > BLACK_BOX_SWITCH_OFF)
            {
                mVisualDataSwitch = true;
                sendMessageDelayed(milliseconds_to_nanoseconds(10), Message(MSG_VISUAL_DATA_SEND));
            } else
            {
                mVisualDataSwitch = true;
                mVcuLogImpl->init();

                mVcuLogImpl->setConfigToMCU();
                sendMessageDelayed(milliseconds_to_nanoseconds(10), Message(MSG_VISUAL_DATA_SEND));
            }
        }
        break;
    case MSG_VISUAL_DATA_SEND:
        LOG3("VisualDataThread::%s MSG_VISUAL_DATA_SEND mVisualDataSwitch=%d", __func__, mVisualDataSwitch);
        if (mVisualDataSwitch)
        {
            int ret = doSendData();
            if (ret == 1)
            {
                sendMessageDelayed(milliseconds_to_nanoseconds(10), Message(MSG_VISUAL_DATA_SEND));
                break;
            } else if (ret == -1)
            {
                doEndSocket();
                break;
            }

            char data[10];
            memset(data, 0, sizeof(data));
            int readlen = read(client_fd, data, sizeof(data));
            if (readlen < 0)
            {
                ALOGE("VisualDataThread::%s read errno=%d strerror(errno) = %s", __func__, errno, strerror(errno));
                doEndSocket();
                break;
            }
            if (string(data, readlen) == "exit")
            {
                LOG1("VisualDataThread::%s read exit", __func__);
                doEndSocket();
                break;
            }

            sendMessageDelayed(milliseconds_to_nanoseconds(10), Message(MSG_VISUAL_DATA_SEND));
        }
        break;
    default: break;
    }
}

void VisualDataThread::doEndSocket() {
    LOG1("VisualDataThread::%s enter", __func__);
    shutdown(client_fd, SHUT_RDWR);
    mVisualDataSwitch = false;
    sendMessageDelayed(milliseconds_to_nanoseconds(10), Message(MSG_VISUAL_DATA_INIT));
    mVcuLogImpl->setConfigToMCU();
}

/**
 * @brief 
 * 
 * @return int 0 success; -1 socket send failed, need init; 1 data is empty, wait for data
 */
int VisualDataThread::doSendData() {
    LOG3("VisualDataThread::%s enter", __func__);
    mVisualDataQueueLock.lock();
    LOG3("VisualDataThread::%s mVisualDataQueue size=%lu", __func__, mVisualDataQueue.size());
    std::queue<std::string> data;
    mVisualDataQueue.swap(data);
    clearData(mVisualDataQueue);
    LOG3("VisualDataThread::%s mVisualDataQueue size=%lu", __func__, mVisualDataQueue.size());
    mVisualDataQueueLock.unlock();

    if (data.empty()) {
        LOG3("VisualDataThread::%s data is empty", __func__);
        return 1;
    }

    std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds> tpMicro1
            = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
    time_t begin = tpMicro1.time_since_epoch().count();

    std::vector<uint8_t> src;
    while (!data.empty())
    {
        string tmp = data.front();
        src.insert(src.end(), tmp.begin(), tmp.end());
        data.pop();
    }
    // string src = "test";
    if (client_fd > 0)
    {
        int ret = write(client_fd, src.data(), src.size());
        if (ret < 0)
        {
            ALOGE("VisualDataThread::%s failed src.size=%lu ret=%d errno=%d strerror(errno) = %s", __func__, src.size(), ret, errno, strerror(errno));
            return -1;
        } else
        {
            LOG3("VisualDataThread::%s send success", __func__);
        }
    }

    std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds> tpMicro2
            = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
    time_t end = tpMicro2.time_since_epoch().count();
    LOG3("VisualDataThread::%s exit data size=%lu cost time = %ld", __func__, data.size(), end -begin);
    return 0;
}

/* -----------------------------------BlackBoxHandlerThread end---------------------------------------*/


/* -----------------------------------VcuLogHandler start----------------------------------------*/

VcuLogHandlerThread::VcuLogHandlerThread(sp<VcuLogImpl> v) {
    LOG1("VcuLogImpl::VcuLogHandlerThread::%s enter", __func__);
    mVcuLogImpl = v;
    mLooper = new Looper(true);
    mLooper->prepare(true);
    Looper::setForThread(mLooper);
    run("VcuLogHandlerThread");
    LOG1("VcuLogImpl::VcuLogHandlerThread::%s exit", __func__);
}

bool VcuLogHandlerThread::threadLoop() {
    if (mLooper)
    {
        // LOG3("VcuLogHandlerThread::%s pollOnce", __func__);
        mLooper->pollOnce(-1);
    }
    return true;
}

void VcuLogHandlerThread::handleMessage(const Message& message) {
    switch (message.what)
    {
    case MSG_STOP_BUFFER:
        LOG1("VcuLogImpl::VcuLogHandlerThread::%s MSG_STOP_BUFFER", __func__);
        removeMessages(MSG_STOP_BUFFER);
        removeMessages(MSG_FORCE_STOP_BUFFER);
        mVcuLogImpl->packData();
        LOG1("VcuLogImpl::VcuLogHandlerThread::%s MSG_STOP_BUFFER over", __func__);
        break;

    case MSG_FORCE_STOP_BUFFER:
        LOG1("VcuLogImpl::VcuLogHandlerThread::%s MSG_FORCE_STOP_BUFFER", __func__);
        removeMessages(MSG_STOP_BUFFER);
        removeMessages(MSG_FORCE_STOP_BUFFER);
        mVcuLogImpl->packData();
        LOG1("VcuLogImpl::VcuLogHandlerThread::%s MSG_FORCE_STOP_BUFFER over", __func__);
        break;

    case MSG_APR_UPLOAD_TODO_FILES:
        LOG1("VcuLogImpl::VcuLogHandlerThread::%s MSG_APR_UPLOAD_TODO_FILES", __func__);
        removeMessages(MSG_APR_UPLOAD_TODO_FILES);
        if (getAccStatus() == "OFF")
        {
            LOG1("VcuLogImpl::VcuLogHandlerThread::%s MSG_APR_UPLOAD_TODO_FILES OFF return", __func__);
            break;
        }
        std::vector<std::string> todolist = getFileList(FILE_PATH_BASE, APR_BACKUP_FILE_POSTFIX);
        if (!todolist.empty()) {
            sp<HttpCommon> httpCommon = new HttpCommon();
            httpCommon->uploadMulti(todolist);
        }
        sp<HttpCommon> httpCommon2 = new HttpCommon();
        httpCommon2->recordReupload();
        break;
    }
}

/* -----------------------------------VcuLogHandler end---------------------------------------*/


VcuLogImpl::VcuLogImpl()
{
    LOG1("VcuLogImpl::%s", __func__);
    mSpiCommon = new SpiCommon();
    mHandler = new VcuLogHandlerThread(this);
    mBlackBoxHandler = new BlackBoxHandlerThread(this);
    mVisualDataThread = new VisualDataThread(this);
}

VcuLogImpl::~VcuLogImpl() {}

void VcuLogImpl::init() {
    LOG1("VcuLogImpl::%s mVcuAprSwitch=%d mVcuBlackBoxSwitch=%d getVisualDataSwitch=%d", __func__, mVcuAprSwitch, mVcuBlackBoxSwitch, mVisualDataThread->getVisualDataSwitch());
    initEcuModules();
    mBlackBoxHandler->init();
    int fd = mSpiCommon->openDevice();

    if (fd > 0) {
        if (mVcuAprSwitch || mVcuBlackBoxSwitch > BLACK_BOX_SWITCH_OFF || mVisualDataThread->getVisualDataSwitch() || mEcuVector.size() > 0)
        {
            run("VcuLogImpl");
        }
    } else {
        ALOGE("VcuLogImpl::%s openDevice failed", __FUNCTION__);
    }
}

void VcuLogImpl::onUploadFlagChanged(int value) {
    LOG2("VcuLogImpl::%s value=%d ", __func__, value);
    if (value != UPLOAD_FLAG_VALID) return;

    int reason;
    BYDAutoManager& autoManager = BYDAutoManager::getInstance();
    autoManager.getInt(AUTO_FEATURE_COLLISION, COLLISION_VCU_FAULT_REASON, &reason);
    addErrorSet(reason);

    int parts;
    autoManager.getInt(AUTO_FEATURE_COLLISION, COLLISION_VCU_FAULT_PARTS, &parts);
    addErrorPartSet(parts);

    if (!mHandler->isBuffering())
    {
        LOG1("VcuLogImpl::%s value=%d mHandler is Empty ", __func__, value);
        mHandler->sendMessageDelayed(seconds_to_nanoseconds(mAprSavaTime), Message(MSG_STOP_BUFFER));
        mHandler->sendMessageDelayed(seconds_to_nanoseconds(mAprMaxSavaTime - mAprSavaTime), Message(MSG_FORCE_STOP_BUFFER));
    }
    else
    {
        LOG2("VcuLogImpl::%s value=%d mHandler is not Empty ", __func__, value);
        mHandler->removeMessages(MSG_STOP_BUFFER);
        mHandler->sendMessageDelayed(seconds_to_nanoseconds(mAprSavaTime), Message(MSG_STOP_BUFFER));
        if (!mHandler->hasMessage(MSG_FORCE_STOP_BUFFER))
        {
            LOG1("VcuLogImpl::%s value=%d add MSG_FORCE_STOP_BUFFER ", __func__, value);
            mHandler->sendMessageDelayed(seconds_to_nanoseconds(mAprMaxSavaTime - mAprSavaTime), Message(MSG_FORCE_STOP_BUFFER));
        }
    }
}

void VcuLogImpl::onUploadReasonChanged(int value) {
    bool isBuffering = mHandler->isBuffering();
    LOG2("VcuLogImpl::%s value=%d isBuffering=%d", __func__, value, isBuffering);
    if (isBuffering && value != INVALID_UPLOAD_REASON)
    {
        addErrorSet(value);
    }
}

void VcuLogImpl::onErrorPartsChanged(int value) {
    bool isBuffering = mHandler->isBuffering();
    LOG2("VcuLogImpl::%s value=%d isBuffering=%d", __func__, value, isBuffering);
    if (isBuffering && value != INVALID_ERROR_PART)
    {
        addErrorPartSet(value);
    }
}

void VcuLogImpl::addErrorSet(int value) {
    Mutex::Autolock _l(mErrorSetLock);
    if (value != INVALID_UPLOAD_REASON)
    {
        mErrorRecord.insert(value);
    }
}

string VcuLogImpl::formatErrorSet() {
    Mutex::Autolock _l(mErrorSetLock);
    string ret = "";
    if (mErrorRecord.empty())
    {
        int value;
        BYDAutoManager& autoManager = BYDAutoManager::getInstance();
        autoManager.getInt(AUTO_FEATURE_COLLISION, COLLISION_VCU_FAULT_REASON, &value);
        mErrorRecord.insert(value);
        LOG1("VcuLogImpl::%s mErrorRecord is empty get value=%d", __func__, value);
    }

    set<int>::iterator it=mErrorRecord.begin();
    while (it != mErrorRecord.end())
    {
        ret.append(std::to_string(*it));
        if (++it != mErrorRecord.end())
        {
            ret.append("&");
        }
    }
    mErrorRecord.clear();
    LOG1("VcuLogImpl::%s ret=%s", __func__, ret.c_str());
    return ret;
}

void VcuLogImpl::addErrorPartSet(int value) {
    Mutex::Autolock _l(mErrorPartLock);
    if (value != INVALID_ERROR_PART)
    {
        mErrorPartRecord.insert(value);
    }
}

std::string VcuLogImpl::formatErrorPartSet() {
    Mutex::Autolock _l(mErrorPartLock);
    string ret = "";
    if (mErrorPartRecord.empty())
    {
        int value;
        BYDAutoManager& autoManager = BYDAutoManager::getInstance();
        autoManager.getInt(AUTO_FEATURE_COLLISION, COLLISION_VCU_FAULT_PARTS, &value);
        mErrorPartRecord.insert(value);
        LOG1("VcuLogImpl::%s mErrorPartRecord is empty get value=%d", __func__, value);
    }

    set<int>::iterator it=mErrorPartRecord.begin();
    while (it != mErrorPartRecord.end())
    {
        ret.append(std::to_string(*it));
        if (++it != mErrorPartRecord.end())
        {
            ret.append("&");
        }
    }
    mErrorPartRecord.clear();
    LOG1("VcuLogImpl::%s ret=%s", __func__, ret.c_str());
    return ret;
}

bool VcuLogImpl::threadLoop() {
    LOG3("VcuLogImpl::%s mVcuAprSwitch=%d mVcuBlackBoxSwitch=%d", __func__, mVcuAprSwitch, mVcuBlackBoxSwitch);

    memset(mData, 0, sizeof(mData));
    int len = mSpiCommon->readDevice(mData, sizeof(mData));

    if (len < DIAG_BASIC_MSG_LEN)
    {
        // sleep(2);
        // LOG1("VcuLogImpl::%s no spi data", __func__);
        clearOvertimeData(false);
        return true;
    }

    count++;lenCount+=len;
    PRINT_BUF_LOG_READ(mData, len, count, lenCount);

    char* p = mData;
    while(len > DIAG_BASIC_MSG_LEN) {
        distributeSpiData( p + DIAG_BASIC_MSG_LEN, *(p + DIAG_BASIC_ID_LEN) );

        int canid = (int)(p[9] << 8) + (int)(p[10]);
        // LOG3("VcuLogImpl::%s canid=%x", __func__, canid);
        if (isVcuCanid(canid))
        {
            // LOG3("VcuLogImpl::%s isVcuCanid", __func__);
            if (mVcuAprSwitch)
            {
                queueData(p + DIAG_BASIC_MSG_LEN, *(p + DIAG_BASIC_ID_LEN) );
            }
            if (mVcuBlackBoxSwitch > BLACK_BOX_SWITCH_OFF)
            {
                mBlackBoxHandler->enqueueData(std::string(p + DIAG_BASIC_ID_LEN, *(p + DIAG_BASIC_ID_LEN) + 1));//包括数据长度字节
                // mBlackBoxHandler->enqueueData( formatCanData(p + DIAG_BASIC_MSG_LEN, *(p + DIAG_BASIC_ID_LEN)) );//asc格式数据
            }
            if (mVisualDataThread->getVisualDataSwitch())
            {
                mVisualDataThread->enqueueData(std::string(p + DIAG_BASIC_ID_LEN, *(p + DIAG_BASIC_ID_LEN) + 1));//包括数据长度字节
            }
        }

        len -= *(p + DIAG_BASIC_ID_LEN) + DIAG_BASIC_MSG_LEN;
        p += *(p + DIAG_BASIC_ID_LEN) + DIAG_BASIC_MSG_LEN;
    }

    clearOvertimeData(false);

    LOG2("VcuLogImpl::%s mBufQueue size = %lu", __func__, mBufQueue.size());

    return true;
}

void VcuLogImpl::initEcuModules() {
    Mutex::Autolock _l(mEcuVectorLock);
    int cur_size = mEcuVector.size();
    for (size_t i = 0; i < cur_size; i++)
    {
        auto item = mEcuVector.at(i);
        item->stop();
    }
    mEcuVector.clear();

    Json::Value domain_config = fileToJson(std::string(ECU_CONFIG_FILE_PATH));
    Json::Value config = domain_config["domain_config"];
    int size = config.size();
    LOG3("%s size=%d config = %s", __func__, size, config.toStyledString().c_str());
    for (int i = 0; i < size; i++)
    {
        Json::Value item = config[i];
        LOG3("%s item = %s", __func__, item.toStyledString().c_str());

        sp<EcuModuleBase> base = new EcuModuleBase(item["domain"].asString());
        base->setSignalVersion(item["version"].asString());

        std::unordered_map<int, int> all_canid;
        Json::Value analysis_signal_config = item["analysis_signal"];
        LOG3("%s analysis_signal_config = %s", __func__, analysis_signal_config.toStyledString().c_str());
        int canid_size = analysis_signal_config.size();
        for (int j = 0; j < canid_size; j++)
        {
            Json::Value single_canid = analysis_signal_config[j];
            LOG3("%s single_canid = %s", __func__, single_canid.toStyledString().c_str());
            int analy_canid = std::stoi(single_canid["canid"].asString(), nullptr, 16);
            int analy_route = std::stoi(single_canid["route"].asString(), nullptr, 10);
            all_canid.insert(std::pair<int, int>(analy_canid, analy_route));
            addEcuCanidMap(analy_canid, analy_route);
        }
        base->setAllSignalCanidMap(all_canid);

        std::unordered_map< int, std::vector< sp<UploadSignalBean> > > upload_signal_map;
        Json::Value upload_signal_config = item["upload_signal"];
        LOG3("%s upload_signal_config = %s", __func__, upload_signal_config.toStyledString().c_str());
        Json::Value::Members upload_signal_config_members = upload_signal_config.getMemberNames();
        for (auto it = upload_signal_config_members.begin(); it != upload_signal_config_members.end(); it++)
        {
            std::string unit = *it;
            Json::Value unit_arrays = upload_signal_config[*it];
            LOG3("%s unit = %s unit_arrays = %s", __func__, unit.c_str(), unit_arrays.toStyledString().c_str());
            for (int k = 0; k < unit_arrays.size(); k++)
            {
                Json::Value single_error = unit_arrays[k];
                LOG3("%s single_error = %s", __func__, single_error.toStyledString().c_str());
                std::string error_signal = single_error["error_signal"].asString();
                std::string error_des = single_error["error_des"].asString();
                int can_route = single_error["can_route"].asInt();
                sp<UploadSignalBean> upload_signal_bean = new UploadSignalBean(error_signal, error_des, can_route, unit);
                int canid = std::stoi(error_signal.substr(0, error_signal.find_first_of("_")), nullptr, 16);
                if (upload_signal_map.find(canid) == upload_signal_map.end())
                {
                    std::vector< sp<UploadSignalBean> > upload_signal_bean_vector;
                    upload_signal_bean_vector.push_back(upload_signal_bean);
                    upload_signal_map.insert(std::pair<int, std::vector< sp<UploadSignalBean> > > (canid, upload_signal_bean_vector));
                } else
                {
                    upload_signal_map.find(canid)->second.push_back(upload_signal_bean);
                }
            }
        }

        base->setUploadSignalCanidMap(upload_signal_map);
        base->startModule();
        mEcuVector.push_back(base);
    }

}
//00 03 B3 EA 02 51 02 30 75 E0 2E E0 2E 38 06
void VcuLogImpl::distributeSpiData(const char* buf, int len) {
    Mutex::Autolock _l(mEcuVectorLock);
    int size = mEcuVector.size();
    int canid = (int)(buf[4] << 8) + (int)(buf[5]);
    LOG3("VcuLogImpl::%s canid=%x mEcuVector size=%d", __func__, canid, size);
    for (size_t i = 0; i < size; i++)
    {
        auto item = mEcuVector.at(i);
        if ( item->isMySignal(canid) )
        {
            item->handleSpiData(buf, len);
        }
        item->clearOverTimeData();
    }
}

void VcuLogImpl::queueData(const char* p, int len) {
    Mutex::Autolock _l(mQueueLock);
    string s;
    if (isCanFD)
    {
        s = formatData(p, len);
    }
    else
    {
        s = formatCanData(p, len);
    }
    mBufQueue.push(s);
}

/**
 * @brief 清除缓存队列中超时数据
 * true 强制清除，上传之后调用
 * @param forceClean
 * false 不强制清除
 */
void VcuLogImpl::clearOvertimeData(bool forceClean) {
    if (forceClean || !mHandler->isBuffering())
    {
        doClearOvertimeData();
    }
}

void VcuLogImpl::doClearOvertimeData() {
    Mutex::Autolock _l(mQueueLock);
    while (true)
    {
        if (mBufQueue.empty())
        {
            break;
        }
        /**vcu数据大概30秒8万条，增加size的判断，避免时间戳出问题导致误清除数据**/
        if (mBufQueue.size() > 80000 && isDataOvertime(mBufQueue.front(), mBufQueue.back()))
        {
            mBufQueue.pop();
            continue;
        }
        else
        {
            break;
        }
    }
}

bool VcuLogImpl::isDataOvertime(string head, string tail) {
    double offset = getTimestamp(tail) - getTimestamp(head);
    /* 增加 <0 逻辑，mcu异常时上报时间戳 */
    return (offset < 0) || (offset > mAprBufferTime);
}

double VcuLogImpl::getTimestamp(string data) {
    if (data.empty()) {return 0;}
    //9021334.700000 CANFD 2 445 Rx 1 0 d 8 8 00 00 01 00 00 00 00 00
    auto pos = data.find(" ");
    string sub = data.substr(0, pos);
    double timestamp = std::stod(sub);
    LOG3("VcuLogImpl::%s data=%s sub=%s timestamp=%f", __func__, data.c_str(), sub.c_str(), timestamp);
    return timestamp;
}

/**
 * 打包上传当前缓存的数据
*/
void VcuLogImpl::packData() {
    string currentError = formatErrorSet();

    mErrorBlacklistLock.lock();
    if (mErrorBlacklist.find(currentError) != mErrorBlacklist.end())
    {
        ALOGI("VcuLogImpl::%s currentError %s is in blacklist ", __func__, currentError.c_str());
        mErrorBlacklistLock.unlock();
        printFrontAndBack("blacklist begin");
        clearOvertimeData(true);
        printFrontAndBack("blacklist end");
        return;
    }
    mErrorBlacklistLock.unlock();

    LOG2("VcuLogImpl::%s lock mBufQueue ", __func__);
    mQueueLock.lock();
    if (mBufQueue.empty()) {
        ALOGE("VcuLogImpl::%s mBufQueue is empty", __func__);
        mQueueLock.unlock();
        return;
    } else if ( shouldErrorUpload(currentError) || currentError.find(MANUAL_REASON) != std::string::npos)
    {
        //没有上传过这个故障
        LOG1("VcuLogImpl::%s mBufQueue size = %lu", __func__, mBufQueue.size());
        queue<string> packQueue(mBufQueue);
        LOG1("VcuLogImpl::%s packQueue size = %lu", __func__, packQueue.size());
        mQueueLock.unlock();
        LOG2("VcuLogImpl::%s unlock mBufQueue ", __func__);

        printFrontAndBack("begin");
        clearOvertimeData(true);
        printFrontAndBack("end");

        string filename = createFileName(currentError);
        string filepath = createFilepath(filename);

        std::vector<uint8_t> src, dst;

        string title = "date ";
        time_t rawtime;
        time (&rawtime);
        title.append(ctime (&rawtime) );
        title.append("base hex timestamps absolute\n");
        title.append("// version 7.0.0\n");
        src.insert(src.end(), title.begin(), title.end());
        while (!packQueue.empty())
        {
            string tmp = packQueue.front();
            src.insert(src.end(), tmp.begin(), tmp.end());
            packQueue.pop();
        }
        string end("End Triggerblock");
        src.insert(src.end(), end.begin(), end.end());

        XzCompress(src, &dst, 1);

        FILE* fout = fopen(filepath.c_str(), "wb");
        if (fout == NULL)
        {
            ALOGE("VcuLogImpl::%s create file failed errno=%d strerror(errno) = %s", __func__, errno, strerror(errno));
            return;
        }
        int ret = fwrite(dst.data(), dst.size(), 1, fout);
        fclose(fout);

        LOG1("VcuLogImpl::%s packQueue end size = %lu", __func__, packQueue.size());

        sp<HttpCommon> httpCommon = new HttpCommon();
        httpCommon->crashUnit = formatErrorPartSet();
        httpCommon->uploadToApr(filepath);

        // Json::Value issue, issues, para;
        // issue[KEY_ISSUE_MODULE] = "vculog";
        // issue[KEY_ISSUE_TYPE] = "Fault";
        // issue[KEY_ISSUE_SIGNAL] = currentError;
        // issue[KEY_ISSUE_UNIT] = formatErrorPartSet();
        // issue[KEY_ISSUE_COUNT] = 1;
        // issue[KEY_ACC_STATUS] = getAccStatus();
        // std::string filetime = filename.substr(filename.find_first_of("@")+1, filename.find_first_of(".") - filename.find_first_of("@") - 1 );
        // issue[KEY_ISSUE_TIME] = httpCommon->formatCrashTime(filetime);
        // issues.append(issue);
        // para[KEY_ISSUES] = issues;
        // httpCommon->uploadSingleV2(filepath, para);

        updateErrorRecord(currentError);
    } else
    {
        mQueueLock.unlock();
        LOG0("VcuLogImpl::%s currentError=%s limit upload", __func__, currentError.c_str());
        printFrontAndBack("limit begin");
        clearOvertimeData(true);
        printFrontAndBack("limit end");
        return;
    }
}

/**
 * @brief print front() and back() in mBufQueue
 * @param info info to print
 */
void VcuLogImpl::printFrontAndBack(std::string info) {
    Mutex::Autolock _l(mQueueLock);
    LOG1("VcuLogImpl::%s %s \n front=%s back= %s size=%lu", __func__, info.c_str(), mBufQueue.front().c_str(), mBufQueue.back().c_str(), mBufQueue.size());
}

/**
 * @brief 故障类型是否应该触发上传
 * 
 * 故障日志上传机制
 *（1）某个故障原因（根据组合做判断，例如5&6，5&6&7，5&7认为是3个不同原因），读取日历日期，当天同一原因最多上传10次。
 *（2）日期更新，上传次数清零，重新开始计数。
 *（3）用户手动触发（即故障原因包括0x3FF）每次都上传日志。
 * 
 * @param error 故障类型
 * @return true 
 * @return false 
 */
bool VcuLogImpl::shouldErrorUpload(std::string error) {
    Mutex::Autolock _l(mErrorRecordLock);

    if (isFileExist(ERROR_RECORD_FILE_PATH))
    {
        time_t rawtime;
        struct tm * timeinfo;
        time (&rawtime);
        timeinfo = localtime (&rawtime);
        std::string current_tm_yday = std::to_string(timeinfo->tm_yday);

        std::ifstream inFile(ERROR_RECORD_FILE_PATH);
        if (inFile && inFile.is_open())
        {
            Json::Value valueAll;
            Json::Reader reader;
            if (reader.parse(inFile, valueAll))
            {
                LOG1("VcuLogImpl::%s: valueAll = %s current_tm_yday=%s",  __func__, valueAll.toStyledString().c_str(), current_tm_yday.c_str());
                if (valueAll["tm_yday"] == current_tm_yday && valueAll.isMember(error) && valueAll[error] >= mErrorCount)
                {
                    inFile.close();
                    return false;
                }
            } else
            {
                ALOGE("VcuLogImpl::%s parse json error getFormattedErrorMessages=%s", __func__, reader.getFormattedErrorMessages().c_str());
            }
            inFile.close();
        } else
        {
            ALOGE("VcuLogImpl::%s open file error 2 errno=%d strerror(errno) = %s", __func__, errno, strerror(errno));
        }
    }
    return true;
}

void VcuLogImpl::updateErrorRecord(std::string error) {
    Mutex::Autolock _l(mErrorRecordLock);
    time_t rawtime;
    struct tm * timeinfo;
    time (&rawtime);
    timeinfo = localtime (&rawtime);
    std::string current_tm_yday = std::to_string(timeinfo->tm_yday);

    if (isFileExist(ERROR_RECORD_FILE_PATH))
    {
        std::ifstream inFile(ERROR_RECORD_FILE_PATH);
        if (inFile && inFile.is_open())
        {
            Json::Value valueAll;
            Json::Reader reader;
            if (reader.parse(inFile, valueAll))
            {
                LOG3("VcuLogImpl::%s current valueAll = %s current_tm_yday=%s", __func__, valueAll.toStyledString().c_str(), current_tm_yday.c_str());
                if (valueAll["tm_yday"] != current_tm_yday)
                {
                    Json::Value valueW;
                    valueW["tm_yday"] = current_tm_yday;
                    valueW[error] = 1;
                    std::ofstream outFile(ERROR_RECORD_FILE_PATH, std::ofstream::out);
                    outFile << valueW;
                    outFile.close();
                    return;
                }

                if (valueAll.isMember(error))
                {
                    valueAll[error] = valueAll[error].asInt() + 1;
                } else
                {
                    valueAll[error] = 1;
                }
                valueAll["tm_yday"] = current_tm_yday;
                std::ofstream outFile(ERROR_RECORD_FILE_PATH, std::ofstream::out);
                LOG3("VcuLogImpl::%s outFile valueAll = %s", __func__, valueAll.toStyledString().c_str());
                outFile << valueAll;
                outFile.close();
            } else
            {
                ALOGE("VcuLogImpl::%s parse json error 2 getFormattedErrorMessages=%s", __func__, reader.getFormattedErrorMessages().c_str());
            }
            inFile.close();
        } else
        {
            ALOGE("VcuLogImpl::%s open file error 2 errno=%d strerror(errno) = %s", __func__, errno, strerror(errno));
        }
    } else
    {
        LOG1("VcuLogImpl::%s create record file", __func__);
        Json::Value valueW;
        valueW["tm_yday"] = current_tm_yday;
        valueW[error] = 1;
        std::ofstream outFile(ERROR_RECORD_FILE_PATH, std::ofstream::out);
        LOG3("VcuLogImpl::%s outFile valueW = %s", __func__, valueW.toStyledString().c_str());
        outFile << valueW;
        outFile.close();
    }

}

string VcuLogImpl::createFilepath(string filename) {
    std::string ret(FILE_PATH_BASE);
    struct stat buffer;
    if (stat(ret.c_str(), &buffer) == -1)
    {
        ALOGE("VcuLogImpl::%s stat %s failed error=%d %s", __func__, ret.c_str(), errno, strerror(errno));
        if (mkdir(ret.c_str(), 0700) == -1)
        {
            ALOGE("VcuLogImpl::%s mkdir %s failed error=%d %s", __func__, ret.c_str(), errno, strerror(errno));
        }
    }
    return ret.append(filename).append(".xz");
}

string VcuLogImpl::createFileName(string error) {
    //vculog_45&48_none@2022-12-30-10-20-47.asc.xz
    std::string name_base = "vculog__none@2022-12-30-10-20-47.asc.xz";
    int max_error_len = NAME_MAX - name_base.length();
    if (error.length() > max_error_len)
    {
        error = error.substr(0, (max_error_len - 3));
        error.append("etc");
    }

    string ret = APR_MODULE_TYPE;
    ret.append("_");
    // string error_type = "1&2&3&4&5&6&7&8&9&10&11&12&13&14&15&16&17&1&2&3&4&5&6&7&8&9&10&11&12&13&14&15&16&17";
    ret.append(error);
    ret.append("_none@");

    time_t rawtime;
    struct tm * timeinfo;
    time (&rawtime);
    timeinfo = localtime (&rawtime);
    ret.append(std::to_string(timeinfo->tm_year+1900).append("-"));
    ret.append(std::to_string(timeinfo->tm_mon+1).append("-"));
    ret.append(std::to_string(timeinfo->tm_mday).append("-"));
    ret.append(std::to_string(timeinfo->tm_hour).append("-"));
    ret.append(std::to_string(timeinfo->tm_min).append("-"));
    ret.append(std::to_string(timeinfo->tm_sec));

    return ret.append(".asc");
}

//00 FB B2 68 01 1A 01 01 03 03 33 33 33 33 78
//0.009900 CANFD 1 243 Rx 1 0 d 8 8 00 00 b8 0b 8f b4 98 61
string VcuLogImpl::formatData(const char* buf, int len) {
    char temp_buf[4];
    memset(temp_buf, 0, sizeof(temp_buf));

    unsigned int timestamp_int = (int)(buf[0] << 24) + (int)(buf[1] << 16) + (int)(buf[2] << 8) + (int)(buf[3]);
    double timestamp_double = timestamp_int * 0.0001;
    // LOG1("VcuLogImpl::%s timestamp_int=%d timestamp_double=%f", __func__, timestamp_int, timestamp_double);
    string ret(std::to_string(timestamp_double)+" CANFD ");

    int route = (int)(buf[6]);
    sprintf(temp_buf, "%x", route);
    ret.append(temp_buf);
    ret.append(" ");

    int canid = (int)(buf[4] << 8) + (int)(buf[5]);
    memset(temp_buf, 0, sizeof(temp_buf));
    sprintf(temp_buf, "%03x", canid);
    ret.append(temp_buf);
    ret.append(" Rx 1 0 d ");

    int data_len = len - 7;
    memset(temp_buf, 0, sizeof(temp_buf));
    if (data_len < 15)
    {
        sprintf(temp_buf, "%x", data_len);
    } else
    {
        sprintf(temp_buf, "%x", 15);
    }
    ret.append(temp_buf);
    ret.append(" ");

    memset(temp_buf, 0, sizeof(temp_buf));
    sprintf(temp_buf, "%x", data_len);
    ret.append(temp_buf);
    ret.append(" ");

    char _d[data_len * 3];
    char* _p = _d;
    int _i = 0;
    char _h = 0, _l = 0;
    for (_i = 7; _i < len; _i++) {
        _h = ((buf[_i] >> 4) & 0xF);
        _l = (buf[_i] & 0xF);
        _h += (_h >= 0 && _h <= 9) ? 0x30 : 0x37;
        _l += (_l >= 0 && _l <= 9) ? 0x30 : 0x37;
        *_p++ = _h;
        *_p++ = _l;
        *_p++ = ' ';
    }
    _d[data_len * 3] = '\0';
    string content(_d, data_len*3);

    ret.append(content);
    ret.append("\n");

    LOG2("VcuLogImpl::%s %s", __func__, ret.c_str());
    return ret;
}

//00 FB B2 68 01 1A 01 01 03 03 33 33 33 33 78
//0.000900 1 20a Rx d 8 02 1e e0 00 20 ff ff 00
string VcuLogImpl::formatCanData(const char* buf, int len) {
    char temp_buf[4];
    memset(temp_buf, 0, sizeof(temp_buf));

    unsigned int timestamp_int = (int)(buf[0] << 24) + (int)(buf[1] << 16) + (int)(buf[2] << 8) + (int)(buf[3]);
    double timestamp_double = timestamp_int * 0.0001;
    // LOG1("VcuLogImpl::%s timestamp_int=%d timestamp_double=%f", __func__, timestamp_int, timestamp_double);
    string ret(std::to_string(timestamp_double)+" ");

    int route = (int)(buf[6]);
    sprintf(temp_buf, "%x", route);
    ret.append(temp_buf);
    ret.append(" ");

    int canid = (int)(buf[4] << 8) + (int)(buf[5]);
    memset(temp_buf, 0, sizeof(temp_buf));
    sprintf(temp_buf, "%03x", canid);
    ret.append(temp_buf);
    ret.append(" Rx d ");

    int data_len = len - 7;
    memset(temp_buf, 0, sizeof(temp_buf));
    if (data_len < 8)
    {
        sprintf(temp_buf, "%x", data_len);
    } else
    {
        sprintf(temp_buf, "%x", 8);
    }
    ret.append(temp_buf);
    ret.append(" ");

    char _d[data_len * 3];
    char* _p = _d;
    int _i = 0;
    char _h = 0, _l = 0;
    for (_i = 7; _i < len; _i++) {
        _h = ((buf[_i] >> 4) & 0xF);
        _l = (buf[_i] & 0xF);
        _h += (_h >= 0 && _h <= 9) ? 0x30 : 0x37;
        _l += (_l >= 0 && _l <= 9) ? 0x30 : 0x37;
        *_p++ = _h;
        *_p++ = _l;
        *_p++ = ' ';
    }
    _d[data_len * 3] = '\0';
    string content(_d, data_len*3);

    ret.append(content);
    ret.append("\n");

    LOG3("VcuLogImpl::%s %s", __func__, ret.c_str());
    return ret;
}

/**
 * @brief 把配置发送给MCU，每次配置是所有域的全量配置，MCU收到之后覆盖旧配置。canid不重复配置。
 * vcu功能为关，并且ecu域配置为空时，配置清空；
 */
void VcuLogImpl::setConfigToMCU() {
    std::set<int> canidSet;
    Json::Value canidArray;
    clearEcuCanidMap();
    clearVcuCanidMap();
    // VCU
    Json::Value vcu_config = fileToJson(std::string(CONFIG_FILE_PATH));
    if (vcu_config["vcu_config"]["ctrl_config"]["vcu_apr_switch"].asInt() > 0
        || vcu_config["vcu_config"]["ctrl_config"]["vcu_blackbox_switch"].asInt() > 0)
    {
        Json::Value vcuCanidArray = vcu_config["vcu_config"]["canid_config"];
        for (int i = 0; i < vcuCanidArray.size(); i++)
        {
            Json::Value item = vcuCanidArray[i];
            int canid = stoi(item["canid"].asString(), nullptr, 16);
            int route = item["route"].asInt();
            addVcuCanidMap(canid, route);
            if (canidSet.find(canid) == canidSet.end())
            {
                canidSet.insert(canid);
                canidArray.append(item);
            } else
            {
                LOG1("vcu_config %s canid=%x is repeat", __func__, canid);
            }
        }
        LOG1("VcuLogImpl::%s vcuCanidArray size=%d getAllCanidMap size=%d", __func__, vcuCanidArray.size(), getAllCanidMap().size());
        // std::unordered_map<int, int> a = getAllCanidMap();
        // for (auto it = a.begin(); it != a.end(); it++)
        // {
        //     LOG1("11 %s canid=%x route=%d", __func__, it->first, it->second);
        // }
    }

    // ECU
    Json::Value ecu_config = fileToJson(std::string(ECU_CONFIG_FILE_PATH));
    Json::Value domain_config = ecu_config["domain_config"];
    int ecu_count = domain_config.size();
    LOG1("VcuLogImpl::%s ecu_count=%d", __func__, ecu_count);
    if (ecu_count > 0)
    {
        for (int i = 0; i < ecu_count; i++)
        {
            Json::Value single_config = domain_config[i]["analysis_signal"];
            LOG3("VcuLogImpl::%s single_config = %s", __func__, single_config.toStyledString().c_str());
            for (int j = 0; j < single_config.size(); j++)
            {
                int canid = stoi(single_config[j]["canid"].asString(), nullptr, 16);
                int route = single_config[j]["route"].asInt();
                addEcuCanidMap(canid, route);
                if (canidSet.find(canid) == canidSet.end())
                {
                    canidSet.insert(canid);
                    canidArray.append(single_config[j]);
                } else
                {
                    LOG1("ecu_config %s canid=%x is repeat", __func__, canid);
                }
            }
            LOG1("VcuLogImpl::%s single_config size=%d getAllCanidMap size=%d", __func__, single_config.size(), getAllCanidMap().size());
            // std::unordered_map<int, int> a = getAllCanidMap();
            // for (auto it = a.begin(); it != a.end(); it++)
            // {
            //     LOG1("22 %s canid=%x route=%d", __func__, it->first, it->second);
            // }
        }
    }

    if (canidArray.size() > 0)
    {
        setConfigToMCU(canidArray);
    } else
    {
        clearConfigToMCU();
    }
}

void VcuLogImpl::setConfigToMCU(Json::Value canidArray) {
    int size = canidArray.size();
    LOG1("VcuLogImpl::%s canidArray size = %d", __func__, size);
    LOG3("VcuLogImpl::%s canidArray = %s", __func__, canidArray.toStyledString().c_str());
    if (size <= 0) { return; }

    BYDAutoManager& mgr(BYDAutoManager::getInstance());
    int ret=0, canid=0, route=0;
    char data[DIAG_WRITE_SIZE_MAX];
    if(size <= SET_CANID_COUNT_MAX) {
        data[0] = SET_CANID_FEATURE_ID >> 24 & 0xFF;
        data[1] = SET_CANID_FEATURE_ID >> 16 & 0xFF;
        data[2] = SET_CANID_FEATURE_ID >> 8 & 0xFF;
        data[3] = SET_CANID_FEATURE_ID & 0xFF;
        data[4] = (size*3 + 2) & 0xFF;
        data[5] = 1;
        data[6] = 1;
        for (int i = 0; i < size; i++)
        {
            Json::Value item = canidArray[i];
            LOG2("VcuLogImpl::%s: 1 item[%d] = %s",  __func__, i, item.toStyledString().c_str());
            canid = stoi(item["canid"].asString(), nullptr, 16);
            route = item["route"].asInt();
            data[i*3+7] = canid >> 8 & 0xFF;
            data[i*3+1+7] = canid & 0xFF;
            data[i*3+2+7] = route & 0xFF;
            LOG2("VcuLogImpl::%s: 1 canid = 0x%x route = %d",  __func__, canid, route);
        }
        mgr.setBuffer(0, COLLISION_VCU_CAN_SWITCH_SET, data+5, size*3 + 7 - 5);
        // ret = mSpiCommon->writeDevice(data, size*3 + 7);
    } else {
        int times = size/SET_CANID_COUNT_MAX;
        int left = size%SET_CANID_COUNT_MAX;

        for (int j = 1; j <= times; j++)
        {
            data[0] = SET_CANID_FEATURE_ID >> 24 & 0xFF;
            data[1] = SET_CANID_FEATURE_ID >> 16 & 0xFF;
            data[2] = SET_CANID_FEATURE_ID >> 8 & 0xFF;
            data[3] = SET_CANID_FEATURE_ID & 0xFF;
            data[4] = (SET_CANID_COUNT_MAX*3 + 2) & 0xFF;
            data[5] = left == 0 ? times : times+1;
            data[6] = j;
            for (int i = 0; i < SET_CANID_COUNT_MAX; i++)
            {
                Json::Value item = canidArray[(j-1)*SET_CANID_COUNT_MAX + i];
                LOG2("VcuLogImpl::%s: 2 item[%d] = %s",  __func__, (j-1)*SET_CANID_COUNT_MAX + i, item.toStyledString().c_str());
                canid = stoi(item["canid"].asString(), nullptr, 16);
                route = item["route"].asInt();
                data[i*3+7] = canid >> 8 & 0xFF;
                data[i*3+1+7] = canid & 0xFF;
                data[i*3+2+7] = route & 0xFF;
                LOG2("VcuLogImpl::%s: 1 canid = 0x%x route = %d",  __func__, canid, route);
            }
            mgr.setBuffer(0, COLLISION_VCU_CAN_SWITCH_SET, data+5, SET_CANID_COUNT_MAX * 3 + 7 - 5);
            // ret += mSpiCommon->writeDevice(data, SET_CANID_COUNT_MAX * 3 + 7);
            memset(data, 0, sizeof(data));
        }

        data[0] = SET_CANID_FEATURE_ID >> 24 & 0xFF;
        data[1] = SET_CANID_FEATURE_ID >> 16 & 0xFF;
        data[2] = SET_CANID_FEATURE_ID >> 8 & 0xFF;
        data[3] = SET_CANID_FEATURE_ID & 0xFF;
        data[4] = (left*3 + 2) & 0xFF;
        data[5] = data[6] = left == 0 ? times : times+1;
        for (int i = 0; i < left; i++)
        {
            Json::Value item = canidArray[times*SET_CANID_COUNT_MAX + i];
            LOG2("VcuLogImpl::%s: 3 item[%d] = %s",  __func__, times*SET_CANID_COUNT_MAX + i, item.toStyledString().c_str());
            canid = stoi(item["canid"].asString(), nullptr, 16);
            route = item["route"].asInt();
            data[i*3+7] = canid >> 8 & 0xFF;
            data[i*3+1+7] = canid & 0xFF;
            data[i*3+2+7] = route & 0xFF;
            LOG2("VcuLogImpl::%s: 3 canid = 0x%x route = %d",  __func__, canid, route);
        }
        mgr.setBuffer(0, COLLISION_VCU_CAN_SWITCH_SET, data+5, left * 3 + 7 - 5);
        // ret += mSpiCommon->writeDevice(data, left * 3 + 7);
        memset(data, 0, sizeof(data));
    }

    LOG1("VcuLogImpl::%s writeDevice ret = %d", __func__, ret);
}

void VcuLogImpl::clearConfigToMCU() {
    LOG1("VcuLogImpl::%s", __func__);
    BYDAutoManager& mgr(BYDAutoManager::getInstance());
    char data[7];
    data[0] = SET_CANID_FEATURE_ID >> 24 & 0xFF;
    data[1] = SET_CANID_FEATURE_ID >> 16 & 0xFF;
    data[2] = SET_CANID_FEATURE_ID >> 8 & 0xFF;
    data[3] = SET_CANID_FEATURE_ID & 0xFF;
    data[4] = 2;
    data[5] = data[6] = 1;
    int ret = mgr.setBuffer(0, COLLISION_VCU_CAN_SWITCH_SET, data+5, 7 - 5);
    // int ret = mSpiCommon->writeDevice(data, 7);
    LOG1("VcuLogImpl::%s ret=%d", __func__, ret);
}

void VcuLogImpl::setErrorBlacklist(Json::Value errorArray) {
    Mutex::Autolock _l(mErrorBlacklistLock);
    mErrorBlacklist.clear();
    int size = errorArray.size();
    LOG2("VcuLogImpl::%s errorArray size = %d errorArray=%s", __func__, size, errorArray.toStyledString().c_str());
    if (size <= 0) { return; }

    for (int i = 0; i < size; i++)
    {
        Json::Value item = errorArray[i];
        mErrorBlacklist.insert( item["error"].asString() );
    }
}

void VcuLogImpl::setLogLevel(int level) {
    LOG1("VcuLogImpl::%s level=%d", __func__, level);
    Diagnostic_setLogLevel(level);
}

}//namespace