#include "zmq.h"
#include <string>
#include <unistd.h>
#include <thread>
#include <utils/Log.h>


namespace byd_auto_hal {

class EthRelayService {
public:
    virtual ~EthRelayService(){}
    virtual int ethSend(const unsigned char* buf, const unsigned int & len)=0;
    virtual int ethRecv(char* buf, unsigned int len)=0;
    virtual int onEthDataRecv(const unsigned char* buf, const unsigned int & len)=0;
};

// realized onEthDataRecv before instantiation
class ZmqRelayService:public EthRelayService {
public:
    ZmqRelayService();
    int initZmqSender();
    int initZmqReceiver();
    int ethSend(const unsigned char* buf, const unsigned int & len) override;
    int ethRecv(char* buf, unsigned int len) override;
    void recvThreadLoop();
private:
    void* mCtx;
    void* mPub;
    void* mSub;
    bool mRecvFlag = true;
    std::string mEndpoint;
    std::thread mRecvThread;

    // some const defination
    int tcp_keep_alive = 1;
    int tcp_keep_idle = 10; // 60s
    int zmq_linger = 0; // 0ms,不等待，当使用zmq_close()关闭socket时，队列中还未发送的消息将被立即丢弃
    int zmq_rcvtimeo = 1000; //1s Maximum time before a recv operation returns with EAGAIN
    int zmq_sndtimeo = 1000; //1s Maximum time before a send operation returns with EAGAIN
};
} // namespace byd_auto_hal
