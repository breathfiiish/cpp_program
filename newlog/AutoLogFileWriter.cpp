#include "AutoLogFileWriter.h"

#include <sys/time.h>

#include <dirent.h>
#include <stdlib.h>
#include <regex>
#include <sys/stat.h>

#if defined(__APPLE__)
#include <pthread.h>
#elif defined(__linux__) && !defined(__ANDROID__)
#include <syscall.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

namespace byd_auto_hal {

#define  LOG_BUF_SIZE 1024

const int kLogFileCountMin = 1;
const int kLogFileCountMax = 20;

const int kBufferSize = 500;

const uint64_t kFileSize = 1024 * 1024 * 5; // 5MB
const uint64_t kDurationTime = 1000 * 20 ; //20ms

const std::string kAutoHalLogFileName = "autohallog";
const std::string kAutoHalLogFilePath = "/data/logs/autohallogs"; // !注意路径是否正确

volatile int autoLogFlag = 1;
volatile int sysLogFlag = 0;
char strAutoLogFlag[PROPERTY_VALUE_MAX];
char strSysLogFlag[PROPERTY_VALUE_MAX];
std::mutex gMutex;

AutoLogFileWriter::AutoLogFileWriter():
    autoLogInitFlag(false)
{
}

void AutoLogFileWriter::addLog(const std::string & logline)
{
    if(m_buffer)
    {
        m_buffer->push(logline);
    }
}

void AutoLogFileWriter::logWriteThread()
{
    while(m_log_flag)
    {
        if(timestamp_now()-m_last_write_time > kDurationTime)
        {
            property_get("debug.autohallog.cus", strAutoLogFlag, "1");
            property_get("debug.autohallog.logd", strSysLogFlag, "0");
            autoLogFlag = atoi(strAutoLogFlag);
            sysLogFlag = atoi(strSysLogFlag);
            std::string logline;
            m_last_write_time = timestamp_now();
            while(m_buffer->tryPop(logline))
            {
                m_filewriter->writer(logline);
            }
        }
        std::this_thread::sleep_for(std::chrono::microseconds(50));
    }
}

AutoLogFileWriter::~AutoLogFileWriter()
{
    m_log_flag = false;
    std::this_thread::sleep_for(std::chrono::microseconds(50));
    m_write_thread.join();
    m_buffer.release();
    m_filewriter.release();
}

extern "C" {

uint64_t autohalGetThreadId() {
#if defined(__BIONIC__)
  return gettid();
#elif defined(__APPLE__)
  uint64_t tid;
  pthread_threadid_np(NULL, &tid);
  return tid;
#elif defined(__linux__)
  return syscall(__NR_gettid);
#elif defined(_WIN32)
  return GetCurrentThreadId();
#endif
}

int _custom_log_write(int prio, const char* tag, const char* fmt, ...)
{
    if(prio == -1) return -1; // 预留后续可能的扩展
    gMutex.lock();
    if(!gAutoLogWriter.autoLogInitFlag) {
        gAutoLogWriter.autoLogInitFlag = gAutoLogWriter.initAutoLogger();
    }
    gMutex.unlock();
    va_list ap;
    __attribute__((uninitialized)) char buf[LOG_BUF_SIZE];

    va_start(ap, fmt);
    vsnprintf(buf, LOG_BUF_SIZE, fmt, ap);
    va_end(ap);

    struct tm now;
    time_t t = time(nullptr);

    #if defined(_WIN32)
    localtime_s(&now, &t);
    #else
    localtime_r(&t, &now);
    #endif

    uint32_t milisec = (timestamp_now() % 1000000)/1000;
    char milisec_str[6];
    sprintf(milisec_str, ".%03d", milisec);

    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%m-%d %H:%M:%S", &now);
    strcat(timestamp, milisec_str);

    uint64_t tid = autohalGetThreadId();

    char priority_char = 'D';

    __attribute__((uninitialized)) char log_buf[LOG_BUF_SIZE+128];
    sprintf(log_buf, "%s %5d %5" PRIu64 " %c %s: %s\n",
        timestamp, getpid(), tid, priority_char, tag, buf);
    gAutoLogWriter.addLog(log_buf);
    return 0;
}

}

RingBuffer::RingBuffer(size_t const size) :
    m_size(size)
    , m_ring(new Item[m_size])
    , m_write_index(0)
    , m_read_index(0)
{

}

void RingBuffer::push(const std::string& logline)
{
    unsigned int write_index = m_write_index.fetch_add(1, std::memory_order_relaxed) % m_size;
    Item & item = m_ring[write_index];
    std::lock_guard<std::mutex> lg(item.flag);
    item.logline = logline;
    item.written = 1;

}

bool RingBuffer::tryPop(std::string& logline)
{
    Item & item = m_ring[m_read_index % m_size];
    std::lock_guard<std::mutex> lg(item.flag);
    if(item.written == 1)
    {
        logline = std::move(item.logline);
        item.written = 0;
        ++m_read_index;
        return true;
    }
    return false;
}

FileWriter::FileWriter(std::string path, std::string filename):
        m_file_path(path),
        m_file_name(filename)
{
    initFileContext();
    m_write_stream.reset(new std::ofstream());
    m_write_stream->open(m_file_path+"/"+kAutoHalLogFileName, std::ofstream::out | std::ofstream::app);
}

void FileWriter::initFileContext()
{
    DIR *dir = opendir(m_file_path.c_str());
    if (dir == NULL)
    {
        mkdir(m_file_path.c_str(), S_IRWXU);
        m_file_write_offset = -1;
        for(int i=0;i<=kLogFileCountMax;i++)
        {
            m_file_list.push_back("");
        }
    }
    else
    {
        struct dirent *ent;
        while ((ent=readdir(dir))!=NULL)
        {
            if(strcmp(ent->d_name,".")==0||strcmp(ent->d_name,"..")==0)
            {
                continue;
            }
            else
            {
                std::string logname = ent->d_name;
                if(isAutoHalLogFile(logname))
                {
                    int log_count = atoi(logname.substr(logname.length()-2, logname.length()-1).c_str());
                    if(log_count >= kLogFileCountMin && log_count <= kLogFileCountMax) {
                        m_file_list[log_count] = logname;
                    }
                    else {
                        remove((m_file_path + "/" +logname).c_str());
                    }
                }
                else if(logname == kAutoHalLogFileName){
                    // 读文件 确定文件大小
                    std::ifstream fileIn((m_file_path + "/" +logname), std::ios::binary|std::ios::in);
                    fileIn.seekg(0, std::ios::end);
                    size_t filesize = fileIn.tellg();
                    if(filesize < kFileSize && filesize > 0)
                        m_file_write_offset = filesize;
                    else {
                        remove((m_file_path + "/" +logname).c_str());
                        m_file_write_offset = -1;
                    }
                    fileIn.close();
                }
                else if(logname != kAutoHalLogFileName){
                    //remove((m_file_path + "/" +logname).c_str());
                }
            }
        }
        closedir(dir);

    }
}

void FileWriter::writer(const std::string & str)
{
    // 初始化时没有autohallog日志 或者 写入要大于文件最大值时 做一次文件切片滚动
    if(m_file_write_offset < 0 || m_file_write_offset + str.length() > kFileSize)
    {
        rollFile();
        m_file_write_offset = 0;
    }
    m_write_stream->write(str.c_str(), str.length());
    m_write_stream->flush();
    m_file_write_offset += str.length();
}

bool FileWriter::checkDir(const std::string & path)
{
    bool isDirExisted = false;
    DIR *dir = opendir(path.c_str());
    if (dir == NULL)
        isDirExisted = false;
    else
        isDirExisted = true;
    closedir(dir);
    return isDirExisted;
}

bool FileWriter::isAutoHalLogFile(const std::string & filename)
{
    std::string regstr = kAutoHalLogFileName + ".[0-9]{2}";
    std::regex log_reg(regstr);
    std::smatch result;
    return std::regex_match(filename, result, log_reg);
}

void FileWriter::rollFile()
{
    if(!checkDir(m_file_path))
        initFileContext();
    // 结束本次日志文件写入
    if(m_write_stream) {
        m_write_stream->flush();
        m_write_stream->close();
    }
    // 将文件重命名
    for(int i=kLogFileCountMax;i>=kLogFileCountMin;i--)
    {
        if(m_file_list[i] != "")
        {
            if(i+1 > kLogFileCountMax) {
                remove((m_file_path+"/"+m_file_list[i]).c_str());
            }
            else {
                char number[5];
                sprintf(number, "%02d", i+1);
                rename((m_file_path+"/"+m_file_list[i]).c_str(), (m_file_path+"/"+kAutoHalLogFileName+"."+number).c_str());
                m_file_list[i+1] = kAutoHalLogFileName+"."+number;
            }
        }
    }
    rename((m_file_path+"/"+kAutoHalLogFileName).c_str(), \
            (m_file_path+"/"+kAutoHalLogFileName+"."+"01").c_str());
    m_file_list[1] = kAutoHalLogFileName+".01";

    // 打开新的日志文件autohallog
    m_write_stream.reset(new std::ofstream());
    m_write_stream->open(m_file_path+"/"+kAutoHalLogFileName, std::ofstream::out | std::ofstream::app);
}

} // namespace byd_auto_hal