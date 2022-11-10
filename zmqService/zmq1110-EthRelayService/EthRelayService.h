
#include <zmq.h>
#include <string>
#include <unistd.h>

namespace byd_auto_hal {

class EthRelayService {
public:
    // EthRelayService();
    virtual ~EthRelayService(){}
    virtual int dataSend(const char* data, int len)=0;
    virtual int dataRecv(char* data)=0;
    int tcp_keep_alive = 1;
    int tcp_keep_idle = 10; // 60s
    int zmq_linger = 0; // 0ms,不等待，当使用zmq_close()关闭socket时，队列中还未发送的消息将被立即丢弃
    int zmq_rcvtimeo = 1000; //1s Maximum time before a recv operation returns with EAGAIN
    int zmq_sndtimeo = 1000; //1s Maximum time before a send operation returns with EAGAIN
};

class ZmqSender:public EthRelayService {
public:
    ZmqSender();
    int initZmqSender();
    int dataSend(const char* data, int len) override;
    int dataRecv(char* data) override;
    // int isConnectAlive();
private:
    void* mCtx;
    void* mPub;
    //int mStatus;
};

class ZmqReceiver:public EthRelayService {
public:
    ZmqReceiver(std::string endpoint);
    int initZmqReceiver();
    int dataSend(const char* data, int len) override;
    int dataRecv(char* data) override;
    //int keepConnectAlive();
private:
    void* mCtx;
    void* mSub;
    int mStatus;
    std::string mEndpoint;
};

} // namespace byd_auto_hal
