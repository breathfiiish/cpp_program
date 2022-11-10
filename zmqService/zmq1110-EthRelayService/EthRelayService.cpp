
#define LOG_TAG "AutoHAL::EthRelayService"

#include "EthRelayService.h"

namespace byd_auto_hal {

const std::string PUB_IP = "tcp://*:6668";
const std::string FSE_IP = "192.168.195.17";
const std::string IVI_IP = "192.168.195.2";

// ZmqSender
ZmqSender::ZmqSender()
{
    int major, minor, patch;
    zmq_version(&major, &minor, &patch);
    fprintf(stderr,"Current ØMQ version is %d.%d.%d\n", major, minor, patch);
    initZmqSender();
}
int ZmqSender::initZmqSender()
{
    mCtx = zmq_ctx_new();
    mPub = zmq_socket(mCtx, ZMQ_PUB);
    zmq_setsockopt(mPub, ZMQ_LINGER, &zmq_linger, sizeof(zmq_linger));
    zmq_setsockopt(mPub, ZMQ_SNDTIMEO, &zmq_sndtimeo, sizeof(zmq_sndtimeo)); 
    if (0 == zmq_bind(mPub, PUB_IP.c_str())) // bind on port wait sub connect
    {
        fprintf(stderr, "end of initZmq \n");
        return 0;
    } else {
        fprintf(stderr, "end of initZmq\n");
        return -1;
    }
}
int ZmqSender::dataSend(const char* data, int len)
{
    int mode = 0;
    if (len == zmq_send(mPub, data, len, mode)) {
        return 0;
    } else {
        return -1;
    }
}
int ZmqSender::dataRecv(char* data)
{
    return -1;
}
// int ZmqSender::isConnectAlive()
// {
    
// }

// ZmqReceiver
ZmqReceiver::ZmqReceiver(std::string endpoint):
    mEndpoint(endpoint)
{
    int major, minor, patch;
    zmq_version(&major, &minor, &patch);
    fprintf(stderr,"Current ØMQ version is %d.%d.%d\n", major, minor, patch);
    mCtx = zmq_ctx_new();
    mSub = zmq_socket(mCtx, ZMQ_SUB);
    initZmqReceiver();
}
int ZmqReceiver::initZmqReceiver()
{
    usleep(2000); // wait for sender init
    std::string addr = "tcp://" + mEndpoint + ":6668";
    mStatus = zmq_connect(mSub, addr.c_str()); // connect to sender port
    if(mStatus != 0)
    {
        fprintf(stderr,"Can't Connect to Endpoint. FSE may not be running.");
        return mStatus;
    }
    // 
    zmq_setsockopt(mSub, ZMQ_SUBSCRIBE, "", 0);
    zmq_setsockopt(mSub, ZMQ_RCVTIMEO, &zmq_rcvtimeo, sizeof(zmq_rcvtimeo)); 
    //Turn on tcp_keepalive
    zmq_setsockopt(mSub, ZMQ_TCP_KEEPALIVE, &tcp_keep_alive, sizeof(tcp_keep_alive));
    zmq_setsockopt(mSub, ZMQ_TCP_KEEPALIVE_IDLE, &tcp_keep_idle, sizeof(tcp_keep_idle));
    fprintf(stderr, "end of initZmq\n");
    return mStatus;
}
int ZmqReceiver::dataSend(const char* data, int len)
{
    return -1;
}
int ZmqReceiver::dataRecv(char* data)
{
    int nbytes = zmq_recv(mSub, data, 1024, 0); // non block or block
    if(nbytes > 0) {
        return nbytes;
    }
    else return -1;
}
// int ZmqReceiver::keepConnectAlive()
// {
//     while(true)
//     {
//         usleep(50000); 
//         if(mStatus != 0)
//         {
//             mStatus = initZmqReceiver();
//         }
//     }
// }


} // namespace byd_auto_hal