#include "EthRelayService.h"

using namespace byd_auto_hal;

int main(int argc, char* argv[])
{
    if(argc < 2)
    {
        return -1;
    }
    EthRelayService* eth_recv = new ZmqReceiver(argv[1]);
    int i = 0;
    while(true)
    {
        sleep(2);
        i++;
        char testStr[1024];
        int ret = eth_recv->dataRecv(testStr);
        if(ret > 0)
        {
            fprintf(stderr,"%s",testStr);
        }
        else{
            fprintf(stderr,"ERROR\n");
        }
    }
}