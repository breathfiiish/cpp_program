
#define LOG_TAG "AutoHAL::EthRelayService"

#include "EthRelayService.h"
#include "ConfigList.h"

namespace byd_auto_hal {

const std::string PUB_IP = "tcp://*:6668";
const std::string FSE_IP = "192.168.195.17";
const std::string IVI_IP = "192.168.195.2";
const unsigned int ETH_RELAY_MAX_BUF_SIZE = 1024;


// ZmqSender
ZmqRelayService::ZmqRelayService()
{
    int major, minor, patch;
    zmq_version(&major, &minor, &patch);
    ALOGE("Current ØMQ version is %d.%d.%d\n", major, minor, patch);
    mCtx = zmq_ctx_new();
    ////////////////////////
    mEndpoint = IVI_IP; // fse is sender, receiver will connect to fse endpoint
    //if(isFSE) 
        initZmqReceiver();
    //if(!isFSE && !isRSE)
        initZmqSender();
    ////////////////////////
}

int ZmqRelayService::initZmqSender()
{
    mPub = zmq_socket(mCtx, ZMQ_PUB);
    zmq_setsockopt(mPub, ZMQ_LINGER, &zmq_linger, sizeof(zmq_linger));
    zmq_setsockopt(mPub, ZMQ_SNDTIMEO, &zmq_sndtimeo, sizeof(zmq_sndtimeo)); 
    if (0 == zmq_bind(mPub, PUB_IP.c_str())) // bind on port wait sub connect
    {
        ALOGE( "end of initZmq \n");
        return 0;
    } else {
        ALOGE( "end of initZmq\n");
        return -1;
    }
}

int ZmqRelayService::initZmqReceiver()
{
    usleep(1000);
    mSub = zmq_socket(mCtx, ZMQ_SUB);
    std::string addr = "tcp://" + mEndpoint + ":6668";
    if(0 != zmq_connect(mSub, addr.c_str())) // connect to sender port
    {
        ALOGE("Can't Connect to Endpoint. FSE may not be running.");
        return -1;
    }
    zmq_setsockopt(mSub, ZMQ_SUBSCRIBE, "", 0);
    zmq_setsockopt(mSub, ZMQ_RCVTIMEO, &zmq_rcvtimeo, sizeof(zmq_rcvtimeo)); 
    //Turn on tcp_keepalive
    zmq_setsockopt(mSub, ZMQ_TCP_KEEPALIVE, &tcp_keep_alive, sizeof(tcp_keep_alive));
    zmq_setsockopt(mSub, ZMQ_TCP_KEEPALIVE_IDLE, &tcp_keep_idle, sizeof(tcp_keep_idle));
    ALOGE( "end of initZmq\n");
    mRecvThread = std::thread(&ZmqRelayService::recvThreadLoop,this);
    return 0;
}

int ZmqRelayService::ethSend(const unsigned char* buf, const unsigned int & len)
{
    int mode = 0;
    int size = len;
    if (size == zmq_send(mPub, buf, len, mode)) {
        ALOGE("zmq send msg : %s \n", buf);
        return len;
    } else {
        // ALOGE("zmq send msg failed: %s", buf);
        return -1;
    }
}

int ZmqRelayService::ethRecv(char* buf, unsigned int len)
{
    int nbytes = zmq_recv(mSub, buf, len, 0); // block 1s until msg
    if(nbytes > 0) {
        ALOGE("zmq recv msg : %s \n", buf);
        return nbytes;
    }
    else return -1;
}

void ZmqRelayService::recvThreadLoop()
{
    while(mRecvFlag)
    {
        char data[ETH_RELAY_MAX_BUF_SIZE];
        int nbytes = ethRecv(data, ETH_RELAY_MAX_BUF_SIZE);
        if(0 < nbytes)
        {
            onEthDataRecv((const unsigned char*)data, (unsigned int)nbytes);
        }
        usleep(1000);
    }
}
} // namespace byd_auto_hal