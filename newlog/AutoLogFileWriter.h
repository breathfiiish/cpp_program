#ifndef _AUTO_LOG_FILE_WRITER_H_
#define _AUTO_LOG_FILE_WRITER_H_

#include <inttypes.h>
#include <utils/Log.h>
#include <mutex>
#include <memory>
#include <thread>
#include <vector>
#include <fstream>
#include <atomic>
#include <cutils/properties.h>


namespace byd_auto_hal {

extern "C" {
    uint64_t autohalGetThreadId();
    int _custom_log_write(int prio, const char* tag, const char* fmt, ...);
}
static uint64_t timestamp_now()
{
    return std::chrono::duration_cast<std::chrono::microseconds> \
(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

extern const int kBufferSize;
extern const std::string kAutoHalLogFileName;
extern const std::string kAutoHalLogFilePath;

#define AUTO_CONDITION(cond)     (__builtin_expect((cond)!=0, 0))

#ifndef AUTO_LOG
#define AUTO_LOG(priority, tag, ...) \
    _custom_log_write(priority, tag, __VA_ARGS__)
#endif

#ifndef AUTO_LOGD
#define AUTO_LOGD(...) ((void)AUTO_LOG(0, LOG_TAG, __VA_ARGS__))
#endif

#ifndef AUTO_LOGD_IF
#define AUTO_LOGD_IF(cond, ...) \
    ( (AUTO_CONDITION(cond)) \
    ? ((void)AUTO_LOG(0, LOG_TAG, __VA_ARGS__)) \
    : (void)0 )
#endif

#ifndef CUSTOM_LOG_IF
#define CUSTOM_LOG_IF(mode, cond, ...) \
    ( (AUTO_CONDITION(mode)) \
    ? ((void)AUTO_LOGD_IF(cond, __VA_ARGS__)) \
    : (void)0 )
#endif

#ifndef SYSTEM_LOG_IF
#define SYSTEM_LOG_IF(mode, cond, ...) \
    ( (AUTO_CONDITION(mode)) \
    ? ((void)ALOGD_IF(cond, __VA_ARGS__)) \
    : (void)0 )
#endif

static int loglevel = 2;
static bool logprint = true;
extern volatile int autoLogFlag;
extern volatile int sysLogFlag;
extern char strAutoLogFlag[PROPERTY_VALUE_MAX];
extern char strSysLogFlag[PROPERTY_VALUE_MAX];

#ifndef ALOGD0
#define ALOGD0(...) CUSTOM_LOG_IF((autoLogFlag > 0), (loglevel >= 0), __VA_ARGS__); \
                    SYSTEM_LOG_IF((sysLogFlag > 0), (loglevel >= 0), __VA_ARGS__);
#endif

#ifndef ALOGD1
#define ALOGD1(...) CUSTOM_LOG_IF((autoLogFlag > 0), (logprint && loglevel >= 1), __VA_ARGS__); \
                    SYSTEM_LOG_IF((sysLogFlag > 0), (logprint && loglevel >= 1), __VA_ARGS__);
#endif

#ifndef ALOGD2
#define ALOGD2(...) CUSTOM_LOG_IF((autoLogFlag > 0), (logprint && loglevel >= 2), __VA_ARGS__); \
                    SYSTEM_LOG_IF((sysLogFlag > 0), (logprint && loglevel >= 2), __VA_ARGS__);
#endif

#ifndef ALOGD3
#define ALOGD3(...) CUSTOM_LOG_IF((autoLogFlag > 0), (logprint && loglevel >= 3), __VA_ARGS__); \
                    SYSTEM_LOG_IF((sysLogFlag > 0), (logprint && loglevel >= 3), __VA_ARGS__);
#endif


class RingBuffer {
public:
    struct  Item
    {
      Item() : written(0)
      , logline("")
      {
      }
      mutable std::mutex flag;
      char written;
      std::string logline;
    };

    RingBuffer(size_t const size);
    ~RingBuffer()
    {
      delete[] m_ring;
    }

    void push(const std::string & logline);
    bool tryPop(std::string& logline);

private:
    size_t const m_size;
    Item * m_ring;
    std::atomic < unsigned int > m_write_index;
    unsigned int m_read_index;
};


class FileWriter {
public:
    FileWriter(std::string path, std::string filename);
    void rollFile();
    void writer(const std::string & str);
private:
    bool isAutoHalLogFile(const std::string & filename);
    void initFileContext();
    bool checkDir(const std::string& path);
private:
    size_t m_file_write_offset = -1;
    std::string m_file_path;
    std::string m_file_name;
    std::unique_ptr < std::ofstream > m_write_stream;
    std::vector<std::string> m_file_list;
};


class AutoLogFileWriter
{
public:
    AutoLogFileWriter();
    ~AutoLogFileWriter();
    bool autoLogInitFlag = false;
    bool initAutoLogger()
    {
        m_log_flag = true;
        sprintf(strAutoLogFlag, "%d", autoLogFlag);
        sprintf(strSysLogFlag, "%d", sysLogFlag);
        property_set("debug.autohallog.cus", strAutoLogFlag);
        property_set("debug.autohallog.logd", strSysLogFlag);
        m_last_write_time = timestamp_now();
        m_buffer.reset(new RingBuffer(kBufferSize));
        m_filewriter.reset(new FileWriter(kAutoHalLogFilePath, kAutoHalLogFileName));
        m_write_thread = std::thread(&AutoLogFileWriter::logWriteThread, this);
        if(m_buffer && m_filewriter)
            return true;
        else
            return false;
    }
    void logWriteThread();
    void addLog(const std::string& logline);
private:
    bool m_log_flag = false;
    std::thread m_write_thread;
    uint64_t m_last_write_time;
    std::unique_ptr< RingBuffer > m_buffer;
    std::unique_ptr < FileWriter > m_filewriter;
};

static AutoLogFileWriter gAutoLogWriter;
}

#endif