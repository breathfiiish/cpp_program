#include "EthRelayService.h"

using namespace byd_auto_hal;

int main()
{
    EthRelayService* eth_sender = new ZmqSender();
    int i = 0;
    while(true)
    {
        sleep(2);
        i++;
        std::string testStr = "SEND" + std::to_string(i) + " \n";
        int ret = eth_sender->dataSend(testStr.c_str(), testStr.size());
        if(ret == 0)
        {
            fprintf(stderr, "%s", testStr.c_str());
        }
        else {
            fprintf(stderr, "%s: %d\n","error",i);
        }
    }
}