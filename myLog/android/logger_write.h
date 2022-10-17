/*
 * Copyright (C) 2020 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//#pragma once

#ifndef _LOGGER_WRITER_H_
#define _LOGGER_WRITER_H_

#include <string>
#include <mutex>
#include <fstream>
#include <dirent.h>
#include <stdlib.h>
//#include <direct.h>
#include <sys/stat.h>
#include <regex>
#include <map>
#include <memory>
#include <thread>
#include <chrono>


std::string& GetDefaultTag();

namespace byd_auto_hal {

const int kLogFileCountMin = 1;
const int kLogFileCountMax = 20;
const std::string kAutoHalLogFileName = "autohallog";
//const size_t kFileSize = 1024 * 1024 * 5; // 5MB
const size_t kFileSize = 1024 * 5; 
const uint64_t kDurationTime = 1000 * 200 ; //200ms

class RingBuffer {
public:
    struct  Item
    {
      Item() : written(0)
      , logline("")
      { }
      mutable std::mutex flag;
      char written;
      std::string logline;
    };

    RingBuffer(size_t const size) :
      m_size(size)
    , m_ring(new Item[m_size])
    , m_write_index(0)
    , m_read_index(0)
    {
    }

    ~RingBuffer()
    {
      delete[] m_ring;
    }

    void push(const std::string & logline)
    {
      m_rw_mutex.lock();
      size_t write_index = m_write_index % m_size;
      if((m_read_index % m_size == m_write_index % m_size) && (m_ring[write_index].written==1)) {
        m_read_index++;
      }
      m_write_index++;
      m_rw_mutex.unlock();
      //fprintf(stderr, "RingBuffer push : %s", logline.c_str());
      Item & item = m_ring[write_index];
      std::lock_guard<std::mutex> lg(item.flag);
      item.logline = logline;
      item.written = 1;
      fprintf(stderr, "RingBuffer push : %s", logline.c_str());
    }

    bool tryPop(std::string& logline) 
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

private:
    size_t const m_size;
    Item * m_ring;
    mutable std::mutex m_rw_mutex;
    unsigned int m_write_index;
    unsigned int m_read_index;
};


class FileWriter {
public:
    FileWriter(std::string path, std::string filename):
    m_file_path(path),
    m_file_name(filename)
    {
        m_file_write_offset = -1;
        for(int i=0;i<=kLogFileCountMax;i++)
        {
            m_file_list.push_back("");
        }
        checkFile();
        m_write_stream.reset(new std::ofstream());
        m_write_stream->open(m_file_path+"/"+kAutoHalLogFileName, std::ofstream::out | std::ofstream::app);

    }

    // 新开日志文件
    void rollFile()
    {
        fprintf(stderr, "-------------------Roll File---------------- \n");
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
                    fprintf(stderr, "rename File from: %s to : %s \n", (m_file_path+"/"+m_file_list[i]).c_str(), (m_file_path+"/"+kAutoHalLogFileName+"."+number).c_str());
                    m_file_list[i+1] = kAutoHalLogFileName+"."+number;
                }
            }
        }
        rename((m_file_path+"/"+kAutoHalLogFileName).c_str(), \
                (m_file_path+"/"+kAutoHalLogFileName+"."+"01").c_str());

        DIR *dir = opendir(m_file_path.c_str());
        if (dir == NULL)
        {
        }
        else
        {
            struct dirent *ent;
            while ((ent=readdir(dir))!=NULL)
            {
                printf(">>>>>>>>>>>All File : %s\n",ent->d_name);
            }
        }
        
        // 打开新的日志文件autohallog
        m_write_stream.reset(new std::ofstream());
        m_write_stream->open(m_file_path+"/"+kAutoHalLogFileName, std::ofstream::out | std::ofstream::app);
    }

    void writer(const std::string & str)
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

    bool isAutoHalLogFile(const std::string & filename)
    {
        std::string regstr = kAutoHalLogFileName + "\.[0-9]{2}";
        //std::regex log_reg("autohallog\.[0-9]{2}");
        std::regex log_reg(regstr);
        std::smatch result;
        return std::regex_match(filename, result, log_reg);
    }

    void checkFile()
    {
        // 查看是否有文件夹
        // 如果没有则创建
        // 如果有则检查文件夹下有什么文件
        
        DIR *dir = opendir(m_file_path.c_str());
        if (dir == NULL)
        {
            // std::string command = "mkdir -p " + m_file_path;
            // system(command);   
            // status = mkdir("/home/cnd/mod1", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)

            mkdir(m_file_path.c_str(), S_IRWXU); 
            fprintf(stderr, "创建文件夹%s",m_file_path.c_str());
        }
        else
        {
            //_chdir(m_file_path.c_str()); 		//进入到当前读取目录eg：/tmp/sdcard/hle_camera
            struct dirent *ent;
            while ((ent=readdir(dir))!=NULL)
            {
                if(strcmp(ent->d_name,".")==0||strcmp(ent->d_name,"..")==0)
                {
                    continue;
                }
                
                // struct stat st;
                // stat(ent->d_name,&st);
                // if(S_ISDIR(st.st_mode)) //递归调用解析其子目录下的文件夹
                // {
                //     //getFileName(ent->d_name);
                // }
                else 
                {
                    printf(">>>>>>>>>>>All File : %s\n",ent->d_name);
                    std::string logname = ent->d_name;
                    if(isAutoHalLogFile(logname))
                    {
                        fprintf(stderr, "####### %s", logname.substr(logname.length()-2, logname.length()-1).c_str());

                        int log_count = atoi(logname.substr(logname.length()-2, logname.length()-1).c_str());
                        
                        if(log_count >= kLogFileCountMin && log_count <= kLogFileCountMax)
                        {
                            //m_file_map.insert(std::pair<int, std::string>(log_count, logname));
                            m_file_list[log_count] = logname;
                            fprintf(stderr, "+++++++++++ Log File: %s \n", logname.c_str());
                        }
                        else {
                            remove((m_file_path + "/" +logname).c_str());
                            fprintf(stderr, "--------- Delete Log File: %s \n", logname.c_str());
                        }
                        
                    }
                    else if(logname != kAutoHalLogFileName){
                        remove((m_file_path + "/" +logname).c_str());
                        fprintf(stderr, "********** Delete Other File: %s \n", logname.c_str());
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
                    // else {
                    //     remove((m_file_path + "/" +logname).c_str());
                    //     fprintf(stderr, "********** Delete Other File: %s \n", logname.c_str());
                    // }
                    
                    
                }
            }
            closedir(dir);


        }
    }


private:
    size_t m_file_write_offset = -1;
    int m_file_index;
    std::string m_file_path;
    std::string m_file_name;
    std::unique_ptr < std::ofstream > m_write_stream;
    std::vector<std::string> m_file_list;

};



/* Returns microseconds since epoch */
static uint64_t timestamp_now()
{
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

class AutoHalLogWriter {
public:
    AutoHalLogWriter() : m_pBuffer(new RingBuffer(1000))
    , m_pFileWriter(new FileWriter("/data/logs/autohallogs","autohallog"))
    , m_last_write_time(timestamp_now())
    {
        logFlag = true;
        // m_pBuffer.reset(new RingBuffer(1000));
        // m_pFileWriter.reset(new FileWriter("/data/logs/autohallogs","autohallog"));
        // m_pBuffer = new RingBuffer(100);
        // m_pFileWriter = new FileWriter("/data/logs/autohallogs","autohallog");
        m_write_thread = std::thread(&AutoHalLogWriter::logWriteThread, this);
    }
    ~AutoHalLogWriter()
    {
        logFlag = false;
        std::this_thread::sleep_for(std::chrono::microseconds(50));
        m_write_thread.join();
        m_pBuffer.release();
        m_pFileWriter.release();
        // if(m_pBuffer != nullptr)
        // {
        //     delete m_pBuffer;
        //     m_pBuffer = nullptr;
        // }
        // if(m_pFileWriter != nullptr)
        // {
        //     delete m_pFileWriter;
        //     m_pFileWriter = nullptr;
        // }
        
    }

    void add(std::string logline)
    {
        fprintf(stderr, "%s", "add log\n");
        if(m_pBuffer == nullptr)
        {
            fprintf(stderr, "%s", "Error Buffer\n");
        }
        m_pBuffer->push(logline);
    }

    void logWriteThread()
    {
        while(logFlag)
        {
            if(timestamp_now()-m_last_write_time > kDurationTime)
            {
                m_last_write_time = timestamp_now();
                std::string logline;
                while(m_pBuffer->tryPop(logline))
                {
                    fprintf(stderr, "Log Write start\n");
                    m_pFileWriter->writer(logline);
                    fprintf(stderr, "Log Write end\n");
                }
                //fprintf(stderr, "Log Write time: %lu", m_last_write_time);
            }
            std::this_thread::sleep_for(std::chrono::microseconds(50));
        }
    }
private:
    bool logFlag;
    std::thread m_write_thread;
    // RingBuffer* m_pBuffer = nullptr;
    // FileWriter* m_pFileWriter = nullptr;
    std::unique_ptr < RingBuffer >  m_pBuffer;
    std::unique_ptr < FileWriter > m_pFileWriter;
    uint64_t m_last_write_time;
    
};


static AutoHalLogWriter g_autohalLogWriter = AutoHalLogWriter();

// void autohalLogWriterStart()
// {
//     g_autohalLogWriter = new AutoHalLogWriter();
//     fprintf(stderr, "-------------autohalLogWriter初始化-----------\n");
// }

// void autohalLogWriterStop()
// {
//     if(g_autohalLogWriter != nullptr)
//     {
//         delete g_autohalLogWriter;
//         g_autohalLogWriter = nullptr;
        
//     }
//     fprintf(stderr, "------------autohalLogWriter停止-------------\n");
// }

} // namesapce byd_auto_hal

#endif