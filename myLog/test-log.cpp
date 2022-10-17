#include "AutoLogs.h"
#include <cutils/properties.h>
#include <stdio.h>
#include <unistd.h>

#include "android/logger_write.h"

#define LOG_TAG "AutoHAL::AutoHWI"

/*
#define LOG_LEVEL_ERROR     0
#define LOG_LEVEL_WARNING   1
#define LOG_LEVEL_DEBUG     2
*/

using namespace byd_auto_hal;
int main(){
    // fprintf(stderr, "-------------START_LOG();-----------\n");
    // START_LOG();
    // SET_LOG_LEVEL(LOG_LEVEL_ERROR);
    // property_set("sys.autohal.loglevel", "2");

    // ALOGD0("MY ALOGD0 TEST string %s, MY TEST int %d","hu.xiyu",10086);
    // ALOGD("MY ALOGD TEST string %s, MY TEST int %d","hu.xiyu",10086);
    // ALOGD1("MY ALOGD1 TEST string %s, MY TEST int %d","hu.xiyu",10086);
    // ALOGD2("MY ALOGD2 TEST string %s, MY TEST int %d","hu.xiyu",10086);
    // ALOGD3("MY ALOGD3 TEST string %s, MY TEST int %d","hu.xiyu",10086);

    // fprintf(stderr, "------------STOP_LOG();-------------\n");
    // STOP_LOG();
    // fprintf(stderr, "------------Test RingBuffer-------------\n");
    // RingBuffer* pBuffer = new RingBuffer(10);
    // pBuffer->push("This is the No.1\n");
    // pBuffer->push("This is the No.2\n");
    // std::string logstr;
    // pBuffer->tryPop(logstr);
    // fprintf(stderr,"%s",logstr.c_str());
    // fprintf(stderr, "------------Test FileWriter-------------\n");

    // FileWriter* pFileWriter = new FileWriter("/data/logs/autohallogs","autohallog");
    // pFileWriter->checkFile();

    // AutoHalLogWriter* log_writer = new AutoHalLogWriter();

    // int k = 1;
    // while(true)
    // {  
    //     if(getchar()=='+')
    //     {
    //         k++;
    //         char str[1024];
    //         scanf("%s",str);
    //         char buffer[1030];
    //         sprintf(buffer, "%d %s \n", k, str);
    //         fprintf(stderr, "%s", buffer);
    //         //pFileWriter->writer(buffer);
    //         if(log_writer != nullptr)
    //         {
    //             log_writer->add(buffer);
    //         }
    //         else{
    //              fprintf(stderr, "null log_writer\n");
    //         }
           
    //     }
    // }

    // return 0;



    
    //autohalLogWriterStart();
    START_LOG();
    SET_LOG_LEVEL(LOG_LEVEL_ERROR);
    property_set("sys.autohal.loglevel", "2");

    ALOGD0("MY ALOGD0 TEST string %s, MY TEST int %d","hu.xiyu",10086);
    ALOGD("MY ALOGD TEST string %s, MY TEST int %d","hu.xiyu",10086);
    ALOGD1("MY ALOGD1 TEST string %s, MY TEST int %d","hu.xiyu",10086);
    ALOGD2("MY ALOGD2 TEST string %s, MY TEST int %d","hu.xiyu",10086);
    ALOGD3("MY ALOGD3 TEST string %s, MY TEST int %d","hu.xiyu",10086);


    int k = 0;
    char flag = '0';
    while(true)
    {  
        flag = getchar();
        if(flag =='q')
        {
            ALOGD0("MY ALOGD0 TEST string %s, MY TEST int %d","hu.xiyu",10086);
            ALOGD("MY ALOGD TEST string %s, MY TEST int %d","hu.xiyu",10086);
            ALOGD1("MY ALOGD1 TEST string %s, MY TEST int %d","hu.xiyu",10086);
            ALOGD2("MY ALOGD2 TEST string %s, MY TEST int %d","hu.xiyu",10086);
            ALOGD3("MY ALOGD3 TEST string %s, MY TEST int %d","hu.xiyu",10086);
           
        }
        if(flag =='p') break;
    }


    
    //autohalLogWriterStop();

    ALOGD0("MY ALOGD0 TEST string %s, MY TEST int %d","hu.xiyu",10086);
    ALOGD("MY ALOGD TEST string %s, MY TEST int %d","hu.xiyu",10086);
    ALOGD1("MY ALOGD1 TEST string %s, MY TEST int %d","hu.xiyu",10086);
    ALOGD2("MY ALOGD2 TEST string %s, MY TEST int %d","hu.xiyu",10086);
    ALOGD3("MY ALOGD3 TEST string %s, MY TEST int %d","hu.xiyu",10086);

    sleep(1);
    return 0;


}