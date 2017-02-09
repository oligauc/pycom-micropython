/*
 * Copyright (c) 2017, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#include "lwip/sockets.h"
#include "lwip/igmp.h"
#include "lwip/netif.h"

#include "aj_net.h"
#include "aj_bus.h"
#include "aj_disco.h"
#include "aj_debug.h"
#include "aj_config.h"
#include "aj_connect.h"

#define INVALID_SOCKET                (-1)
#define MSG_NOSIGNAL                  (0)
#define MDNS_UDP_PORT                 (5353)
#define AJ_UDP_PORT                   (9956)

typedef struct {
    int udpSock;
    int mDnsSock;
    int mDnsRecvSock;
    uint32_t mDnsRecvAddr;
    uint16_t mDnsRecvPort;
} MCastContext;

static const char MDNS_IPV4_MULTICAST_GROUP[] = "224.0.0.251";
static const char AJ_IPV4_MULTICAST_GROUP[] = "224.0.0.113";

static int netContext = INVALID_SOCKET;
static MCastContext mCastContext = { INVALID_SOCKET, INVALID_SOCKET, INVALID_SOCKET, INVALID_SOCKET };
static uint8_t blocked = FALSE;

static uint8_t rxData[AJ_RX_DATA_SIZE];
static uint8_t txData[AJ_TX_DATA_SIZE];

static AJ_Status AJ_TCP_Connect(AJ_BusAttachment* bus, const AJ_Service* service)
{
    
    printf("++++++++++ AJ_TCP_Connect\n");
    int ret;
    struct sockaddr_storage addrBuf;
    socklen_t addrSize;
    int tcpSock = INVALID_SOCKET;

    memset(&addrBuf, 0, sizeof(addrBuf));

    tcpSock = socket(AF_INET, SOCK_STREAM, 0);
    if (tcpSock == INVALID_SOCKET) {
        AJ_ErrPrintf(("AJ_TCP_Connect(): socket() failed.  status=AJ_ERR_CONNECT\n"));
        goto ConnectError;
    }
    if (service->addrTypes & AJ_ADDR_TCP4) {
        struct sockaddr_in* sa = (struct sockaddr_in*)&addrBuf;
        sa->sin_family = AF_INET;
        sa->sin_port = htons(service->ipv4port);
        sa->sin_addr.s_addr = service->ipv4;
        addrSize = sizeof(struct sockaddr_in);
        AJ_InfoPrintf(("AJ_TCP_Connect(): Connect to \"%s:%u\"\n", inet_ntoa(sa->sin_addr), service->ipv4port));;
    } else if (service->addrTypes & AJ_ADDR_TCP6) {
        struct sockaddr_in6* sa = (struct sockaddr_in6*)&addrBuf;
        sa->sin6_family = AF_INET6;
        sa->sin6_port = htons(service->ipv6port);
        memcpy(sa->sin6_addr.s6_addr, service->ipv6, sizeof(sa->sin6_addr.s6_addr));
        addrSize = sizeof(struct sockaddr_in6);
    } else {
        AJ_ErrPrintf(("AJ_TCP_Connect(): Invalid addrTypes %u, status=AJ_ERR_CONNECT\n", service->addrTypes));
        goto ConnectError;
    }

    ret = connect(tcpSock, (struct sockaddr*)&addrBuf, addrSize);
    if (ret < 0) {
        AJ_ErrPrintf(("AJ_TCP_Connect(): connect() failed. errno=\"%s\", status=AJ_ERR_CONNECT\n", strerror(errno)));
        goto ConnectError;
    } else {
        netContext = tcpSock;
        AJ_IOBufInit(&bus->sock.rx, rxData, sizeof(rxData), AJ_IO_BUF_RX, &netContext);
        bus->sock.rx.recv = AJ_Net_Recv;
        AJ_IOBufInit(&bus->sock.tx, txData, sizeof(txData), AJ_IO_BUF_TX, &netContext);
        bus->sock.tx.send = AJ_Net_Send;
        AJ_InfoPrintf(("AJ_TCP_Connect(): status=AJ_OK\n"));
    }

    return AJ_OK;

ConnectError:
    if (tcpSock != INVALID_SOCKET) {
        close(tcpSock);
    }

    return AJ_ERR_CONNECT;
}


AJ_Status AJ_Net_Connect(AJ_BusAttachment* bus, const AJ_Service* service)
{
    printf("++++++++++ AJ_Net_Connect\n");
    
    AJ_Status status = AJ_ERR_CONNECT;

    AJ_InfoPrintf(("AJ_Net_Connect(bus=0x%p, addrType=%d.)\n", bus, service->addrTypes));

    if (service->addrTypes & (AJ_ADDR_TCP4 | AJ_ADDR_TCP6)) {
        status = AJ_TCP_Connect(bus, service);
    }

    return status;
}

static AJ_Status CloseNetSock(AJ_NetSocket* netSock)
{
    printf("++++++++++ CloseNetSock\n");
    
    int *context = netSock->rx.context;
    
    if (*context) {
        if (*context != INVALID_SOCKET) {
            struct linger l;
            l.l_onoff = 1;
            l.l_linger = 0;
            setsockopt(*context, SOL_SOCKET, SO_LINGER, (void*)&l, sizeof(l));
            shutdown(*context, SHUT_RDWR);
            close(*context);
        }
        *context = INVALID_SOCKET;
        memset(netSock, 0, sizeof(AJ_NetSocket));
    }
    return AJ_OK;
}

void AJ_Net_Disconnect(AJ_NetSocket* netSock)
{
    printf("++++++++++ AJ_Net_Disconnect\n");
    
    if (netContext != INVALID_SOCKET) {
        CloseNetSock(netSock);
    }
}

AJ_Status AJ_Net_Recv(AJ_IOBuffer* buf, uint32_t len, uint32_t timeout)
{
    printf("++++++++++ AJ_Net_Recv\n");
    
    int rc = 0;
    fd_set fds;
    AJ_Status status = AJ_OK;
    int* context = buf->context;
    
    size_t rx = AJ_IO_BUF_SPACE(buf);
    struct timeval tv = { timeout / 1000, 1000 * (timeout % 1000) };
    
    assert(buf->direction == AJ_IO_BUF_RX);

    FD_ZERO(&fds);
    FD_SET(*context, &fds);
    blocked = TRUE;
    rc = select(*context, &fds, NULL, NULL, &tv);
    blocked = FALSE;
    
    if (rc == 0) {
        return AJ_ERR_TIMEOUT;
    }
    
    rx = min(rx, len);
    if (rx) {
        ssize_t ret = recv(*context, buf->writePtr, rx, 0);
        if ((ret == -1) || (ret == 0)) {
            AJ_ErrPrintf(("AJ_Net_Recv(): recv() failed. errno=\"%s\"\n", strerror(errno)));
            status = AJ_ERR_READ;
        } else {
            AJ_InfoPrintf(("AJ_Net_Recv(): recv'd %d from tcp\n", ret));
            buf->writePtr += ret;
        }
    }
    return status;
}

AJ_Status AJ_Net_Send(AJ_IOBuffer* buf)
{
    printf("++++++++++ AJ_Net_Send\n");
    
    int* context = buf->context;
    ssize_t ret;
    size_t tx = AJ_IO_BUF_AVAIL(buf);

    AJ_InfoPrintf(("AJ_Net_Send(buf=0x%p)\n", buf));

    assert(buf->direction == AJ_IO_BUF_TX);

    if (tx > 0) {
        ret = send(*context, buf->readPtr, tx, MSG_NOSIGNAL);
        if (ret == -1) {
            AJ_ErrPrintf(("AJ_Net_Send(): send() failed. errno=\"%s\", status=AJ_ERR_WRITE\n", strerror(errno)));
            return AJ_ERR_WRITE;
        }
        buf->readPtr += ret;
    }
    if (AJ_IO_BUF_AVAIL(buf) == 0) {
        AJ_IO_BUF_RESET(buf);
    }

    AJ_InfoPrintf(("AJ_Net_Send(): status=AJ_OK\n"));
    return AJ_OK;
}

static int MDnsRecvUp()
{
    printf("++++++++++ MDnsRecvUp\n");
    
    int ret;
    struct sockaddr_in sin;
    int reuse = 1;
    int recvSock;

    recvSock = socket(AF_INET, SOCK_DGRAM, 0);
    if (recvSock == INVALID_SOCKET) {
         printf("++++++++++ MDnsRecvUp - socket\n");
        AJ_ErrPrintf(("MDnsRecvUp(): socket() fails. status=AJ_ERR_READ\n"));
        goto ExitError;
    }

    ret = setsockopt(recvSock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    if (ret != 0) {
        printf("++++++++++ MDnsRecvUp - setsockopt\n");
        AJ_ErrPrintf(("MDnsRecvUp(): setsockopt(SO_REUSEADDR) failed. errno=\"%s\", status=AJ_ERR_READ\n", strerror(errno)));
        goto ExitError;
    }

    sin.sin_family = AF_INET;
    sin.sin_port = htons(0);
    sin.sin_addr.s_addr = INADDR_ANY;
    ret = bind(recvSock, (struct sockaddr*) &sin, sizeof(sin));
    if (ret < 0) {
        printf("++++++++++ MDnsRecvUp - bind\n");
        AJ_ErrPrintf(("MDnsRecvUp(): bind() failed. errno=\"%s\", status=AJ_ERR_READ\n", strerror(errno)));
        goto ExitError;
    }
    return recvSock;

ExitError:
    close(recvSock);
    return INVALID_SOCKET;
}

static uint8_t rxDataMCast[1454];
static uint8_t txDataMCast[475];

static int MCastUp4(const char group[], uint16_t port)
{
    printf("++++++++++ MCastUp4\n");
    
    int ret;
    struct ip_mreq mreq;
    struct sockaddr_in sin;
    int reuse = 1;
    int bcast = 1;
    int mcastSock;

    mcastSock = socket(AF_INET, SOCK_DGRAM, 0);
    if (mcastSock == INVALID_SOCKET) {
        AJ_ErrPrintf(("MCastUp4(): socket() fails. status=AJ_ERR_READ\n"));
        return INVALID_SOCKET;
    }

    ret = setsockopt(mcastSock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    if (ret != 0) {
        AJ_ErrPrintf(("MCastUp4(): setsockopt(SO_REUSEADDR) failed. errno=\"%s\", status=AJ_ERR_READ\n", strerror(errno)));
        goto ExitError;
    }

    // enable IP broadcast on this socket.
    // This is needed for bcast router discovery
    int r = setsockopt(mcastSock, SOL_SOCKET, SO_BROADCAST, (void*) &bcast, sizeof(bcast));
    if (r != 0) {
        AJ_ErrPrintf(("BcastUp4(): setsockopt(SOL_SOCKET, SO_BROADCAST) failed. errno=\"%s\"\n", strerror(errno)));
        goto ExitError;
    }

    /*
     * Bind supplied port
     */
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    sin.sin_addr.s_addr = INADDR_ANY;
    ret = bind(mcastSock, (struct sockaddr*) &sin, sizeof(sin));
    if (ret < 0) {
        AJ_ErrPrintf(("MCastUp4(): bind() failed. errno=\"%s\", status=AJ_ERR_READ\n", strerror(errno)));
        goto ExitError;
    }

    /*
     * Join our multicast group
     */
    memset(&mreq, 0, sizeof(mreq));
    inet_pton(AF_INET, group, &mreq.imr_multiaddr);
    mreq.imr_interface.s_addr = INADDR_ANY;
    ret = setsockopt(mcastSock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));
    if (ret < 0) {
        /*
         * Not all Linux based systems setup an IPv4 multicast route.
         * Since we were successful in setting up IPv4 broadcast for
         * this socket, we'll just use that and not use IPv4 multicast.
         */
        AJ_WarnPrintf(("MCastUp4(): setsockopt(IP_ADD_MEMBERSHIP) failed. errno=\"%s\", status=AJ_ERR_READ\n", strerror(errno)));
    }

    return mcastSock;

ExitError:
    close(mcastSock);
    return INVALID_SOCKET;
}

static uint32_t chooseMDnsRecvAddr(){
    
    printf("++++++++++ chooseMDnsRecvAddr\n");
    
    uint32_t recvAddr = 0;
    struct netif *n = NULL;
    
    for (n = netif_list; n != NULL; n = n->next) {
        if ((n->flags & NETIF_FLAG_UP)){ 
            ip4_addr_t ip4Addr = n->ip_addr.u_addr.ip4;
            const char *charIP4Addr = ip4addr_ntoa(&ip4Addr);
            printf("++++ Our ip: %s", charIP4Addr);
            uint32_t uIP4Addr = ipaddr_addr(charIP4Addr);
            
            if (uIP4Addr != IPADDR_LOOPBACK){
                recvAddr = uIP4Addr;
            }
        }
    }
    
    return recvAddr;
}

/*static uint32_t chooseMDnsRecvAddr()
{
    uint32_t recvAddr = 0;
    struct ifaddrs* addrs;
    struct ifaddrs* addr;

    getifaddrs(&addrs);
    addr = addrs;
    while (addr != NULL) {
        // Choose first IPv4 address that is not LOOPBACK
        if (addr->ifa_addr != NULL && addr->ifa_addr->sa_family == AF_INET &&
            !(addr->ifa_flags & IFF_LOOPBACK)) {
            struct sockaddr_in* sin = (struct sockaddr_in*) addr->ifa_addr;
            recvAddr = sin->sin_addr.s_addr;
        }
        addr = addr->ifa_next;
    }
    freeifaddrs(addrs); 
    return recvAddr;
}*/

static uint8_t sendToBroadcast(int sock, uint16_t port, void* ptr, size_t tx)
{
    ssize_t ret = -1;
    printf("++++++++++ sendToBroadcast\n");
    uint8_t sendSucceeded = FALSE;
    
    ip4_addr_t ip4bc = ip_addr_broadcast.u_addr.ip4;
    const char *charIP4bc = ip4addr_ntoa(&ip4bc);
    uint32_t uIP4bc = ipaddr_addr(charIP4bc);
    printf("++++ Broadcast ip: %s", charIP4bc);
    
    char buf[INET_ADDRSTRLEN];
    struct sockaddr_in sin_bcast;
    sin_bcast.sin_family = AF_INET;
    sin_bcast.sin_port = htons(port);
    sin_bcast.sin_addr.s_addr = uIP4bc;
    
    inet_ntop(AF_INET, &(sin_bcast.sin_addr), buf, sizeof(buf));
    AJ_InfoPrintf(("sendToBroadcast: sending to bcast addr %s\n", buf));
    ret = sendto(sock, ptr, tx, MSG_NOSIGNAL, (struct sockaddr*) &sin_bcast, sizeof(struct sockaddr_in));
    if (tx == ret) {
        sendSucceeded = TRUE;
        printf("+++++++++++++++ sendToBroadcast- Success\n");
    } else {
        printf("+++++++++++++++ sendToBroadcast- Fail\n");
        AJ_ErrPrintf(("sendToBroadcast(): sendto failed. errno=\"%s\"\n", strerror(errno)));
    }
    
    
   /* struct netif *n = NULL;
    
    for (n = netif_list; n != NULL; n = n->next) {
        if ((n->flags & NETIF_FLAG_UP) && (n->flags & NETIF_FLAG_BROADCAST)){ 
            printf("++++++++++++ Has broadcast ++++++++++++++++\n");
            ip4_addr_t ip4Addr = n->ip_addr.u_addr.ip4;
            const char *charIP4Addr = ip4addr_ntoa(&ip4Addr);
            uint32_t uIP4Addr = ipaddr_addr(charIP4Addr);
            //printf("++++ Broadcast ip: %s", charIP4Addr);
            //printf("++++ isBroadcast: %d\n",ip4_addr_isbroadcast_u32(uIP4Addr, n));
            //printf("++++loopback: %d\n",uIP4Addr != IPADDR_LOOPBACK);
            //uint32_t uIP4Addr = ipaddr_addr(charIP4Addr);
            
            //if ((uIP4Addr != IPADDR_LOOPBACK) && (ip4_addr_isbroadcast_u32(uIP4Addr, n))){
            //    printf("+++++++++++++++ Found broadcast++++++++\n");
            //}
        }
    }*/
    
    return sendSucceeded;
}

/*static uint8_t sendToBroadcast(int sock, uint16_t port, void* ptr, size_t tx)
{
    printf("++++++++++ sendToBroadcast\n");
    
    //ssize_t ret = -1;
    uint8_t sendSucceeded = FALSE;
    struct ifaddrs* addrs;
    struct ifaddrs* addr;

    getifaddrs(&addrs);
    addr = addrs;

    while (addr != NULL) {
        // only care about IPV4
        if (addr->ifa_addr != NULL && addr->ifa_addr->sa_family == AF_INET) {
            char buf[INET_ADDRSTRLEN];
            struct sockaddr_in* sin_bcast = (struct sockaddr_in*) addr->ifa_ifu.ifu_broadaddr;
            sin_bcast->sin_port = htons(port);
            inet_ntop(AF_INET, &(sin_bcast->sin_addr), buf, sizeof(buf));
            AJ_InfoPrintf(("sendToBroadcast: sending to bcast addr %s\n", buf));
            ret = sendto(sock, ptr, tx, MSG_NOSIGNAL, (struct sockaddr*) sin_bcast, sizeof(struct sockaddr_in));
            if (tx == ret) {
                sendSucceebroaded = TRUE;
            } else {
                AJ_ErrPrintf(("sendToBroadcast(): sendto failed. errno=\"%s\"\n", strerror(errno)));
            }
        }

        addr = addr->ifa_next;
    }
    freeifaddrs(addrs); 
    return sendSucceeded;
}*/

static AJ_Status RewriteSenderInfo(AJ_IOBuffer* buf, uint32_t addr, uint16_t port)
{
    printf("++++++++++ RewriteSenderInfo\n");
    
    uint16_t sidVal;
    const char snd[4] = { 'd', 'n', 'e', 's' };
    const char sid[] = { 's', 'i', 'd', '=' };
    const char ipv4[] = { 'i', 'p', 'v', '4', '=' };
    const char upcv4[] = { 'u', 'p', 'c', 'v', '4', '=' };
    char sidStr[6];
    char ipv4Str[17];
    char upcv4Str[6];
    uint8_t* pkt;
    uint16_t dataLength;
    int match;
    AJ_Status status;

    // first, pluck the search ID from the mDNS header
    sidVal = *(buf->readPtr) << 8;
    sidVal += *(buf->readPtr + 1);

    // convert to strings
    status = AJ_IntToString((int32_t) sidVal, sidStr, sizeof(sidStr));
    if (status != AJ_OK) {
        return AJ_ERR_WRITE;
    }
    status = AJ_IntToString((int32_t) port, upcv4Str, sizeof(upcv4Str));
    if (status != AJ_OK) {
        return AJ_ERR_WRITE;
    }
    status = AJ_InetToString(addr, ipv4Str, sizeof(ipv4Str));
    if (status != AJ_OK) {
        return AJ_ERR_WRITE;
    }

    // ASSUMPTIONS: sender-info resource record is the final resource record in the packet.
    // sid, ipv4, and upcv4 key value pairs are the final three key/value pairs in the record.
    // The length of the other fields in the record are static.
    //
    // search backwards through packet to find the start of "sender-info"
    pkt = buf->writePtr;
    match = 0;
    do {
        if (*(pkt--) == snd[match]) {
            match++;
        } else {
            match = 0;
        }
    } while (pkt != buf->readPtr && match != 4);
    if (match != 4) {
        return AJ_ERR_WRITE;
    }

    // move forward to the Data Length field
    pkt += 22;

    // actual data length is the length of the static values already in the buffer plus
    // the three dynamic key-value pairs to re-write
    dataLength = 23 + 1 + sizeof(sid) + strlen(sidStr) + 1 + sizeof(ipv4) + strlen(ipv4Str) + 1 + sizeof(upcv4) + strlen(upcv4Str);
    *pkt++ = (dataLength >> 8) & 0xFF;
    *pkt++ = dataLength & 0xFF;

    // move forward past the static key-value pairs
    pkt += 23;

    // ASSERT: must be at the start of "sid="
    assert(*(pkt + 1) == 's');

    // re-write new values
    *pkt++ = sizeof(sid) + strlen(sidStr);
    memcpy(pkt, sid, sizeof(sid));
    pkt += sizeof(sid);
    memcpy(pkt, sidStr, strlen(sidStr));
    pkt += strlen(sidStr);

    *pkt++ = sizeof(ipv4) + strlen(ipv4Str);
    memcpy(pkt, ipv4, sizeof(ipv4));
    pkt += sizeof(ipv4);
    memcpy(pkt, ipv4Str, strlen(ipv4Str));
    pkt += strlen(ipv4Str);

    *pkt++ = sizeof(upcv4) + strlen(upcv4Str);
    memcpy(pkt, upcv4, sizeof(upcv4));
    pkt += sizeof(upcv4);
    memcpy(pkt, upcv4Str, strlen(upcv4Str));
    pkt += strlen(upcv4Str);

    buf->writePtr = pkt;

    return AJ_OK;
}

AJ_Status AJ_Net_SendTo(AJ_IOBuffer* buf)
{
    printf("++++++++++ AJ_Net_SendTo\n");
    
    ssize_t ret = -1;
    uint8_t sendSucceeded = FALSE;
    size_t tx = AJ_IO_BUF_AVAIL(buf);
    MCastContext* context = (MCastContext*) buf->context;
    AJ_InfoPrintf(("AJ_Net_SendTo(buf=0x%p)\n", buf));
    assert(buf->direction == AJ_IO_BUF_TX);

    if (tx > 0) {
        if ((context->udpSock != INVALID_SOCKET) && (buf->flags & AJ_IO_BUF_AJ)) {
            struct sockaddr_in sin;
            sin.sin_family = AF_INET;
            sin.sin_port = htons(AJ_UDP_PORT);

            if (inet_pton(AF_INET, AJ_IPV4_MULTICAST_GROUP, &sin.sin_addr) == 1) {
                ret = sendto(context->udpSock, buf->readPtr, tx, MSG_NOSIGNAL, (struct sockaddr*)&sin, sizeof(sin));
                if (tx == ret) {
                    sendSucceeded = TRUE;
                } else {
                    AJ_ErrPrintf(("AJ_Net_SendTo(): sendto AJ IPv4 failed. errno=\"%s\"\n", strerror(errno)));
                }
            } else {
                AJ_ErrPrintf(("AJ_Net_SendTo(): Invalid AJ IP address. errno=\"%s\"\n", strerror(errno)));
            }

            if (sendToBroadcast(context->udpSock, AJ_UDP_PORT, buf->readPtr, tx) == TRUE) {
                sendSucceeded = TRUE;
            } // leave sendSucceeded unchanged if FALSE
        }
    }

    if (buf->flags & AJ_IO_BUF_MDNS) {
        if (RewriteSenderInfo(buf, context->mDnsRecvAddr, context->mDnsRecvPort) != AJ_OK) {
            AJ_WarnPrintf(("AJ_Net_SendTo(): RewriteSenderInfo failed.\n"));
            tx = 0;
        } else {
            tx = AJ_IO_BUF_AVAIL(buf);
        }
    }

    if (tx > 0) {
        if ((context->mDnsSock != INVALID_SOCKET) && (buf->flags & AJ_IO_BUF_MDNS)) {
            struct sockaddr_in sin;
            sin.sin_family = AF_INET;
            sin.sin_port = htons(MDNS_UDP_PORT);

            if (inet_pton(AF_INET, MDNS_IPV4_MULTICAST_GROUP, &sin.sin_addr) == 1) {
                ret = sendto(context->mDnsSock, buf->readPtr, tx, MSG_NOSIGNAL, (struct sockaddr*)&sin, sizeof(sin));
                if (tx == ret) {
                    sendSucceeded = TRUE;
                } else {
                    AJ_ErrPrintf(("AJ_Net_SendTo(): sendto mDNS IPv4 failed. errno=\"%s\"\n", strerror(errno)));
                }
            } else {
                AJ_ErrPrintf(("AJ_Net_SendTo(): Invalid mDNS IP address. errno=\"%s\"\n", strerror(errno)));
            }

            if (sendToBroadcast(context->mDnsSock, MDNS_UDP_PORT, buf->readPtr, tx) == TRUE) {
                sendSucceeded = TRUE;
            } // leave sendSucceeded unchanged if FALSE
        }

        if (!sendSucceeded) {
            /* Not a single send succeeded, return an error */
            AJ_ErrPrintf(("AJ_Net_SendTo(): sendto() failed. errno=\"%s\", status=AJ_ERR_WRITE\n", strerror(errno)));
            return AJ_ERR_WRITE;
        }
        buf->readPtr += ret;
    }
    AJ_IO_BUF_RESET(buf);
    AJ_InfoPrintf(("AJ_Net_SendTo(): status=AJ_OK\n"));
    return AJ_OK;
}

AJ_Status AJ_Net_RecvFrom(AJ_IOBuffer* buf, uint32_t len, uint32_t timeout)
{
    printf("++++++++++ AJ_Net_RecvFrom\n");
    
    MCastContext* context = (MCastContext*) buf->context;
    AJ_Status status = AJ_OK;
    ssize_t ret;
    size_t rx;
    fd_set fds;
    int maxFd = INVALID_SOCKET;
    int rc = 0;
    struct timeval tv = { timeout / 1000, 1000 * (timeout % 1000) };

    // AJ_InfoPrintf(("AJ_Net_RecvFrom(buf=0x%p, len=%d, timeout=%d)\n", buf, len, timeout));

    assert(buf->direction == AJ_IO_BUF_RX);
    assert(context->mDnsRecvSock != INVALID_SOCKET);

    FD_ZERO(&fds);
    FD_SET(context->mDnsRecvSock, &fds);
    maxFd = context->mDnsRecvSock;

    if (context->udpSock != INVALID_SOCKET) {
        FD_SET(context->udpSock, &fds);
        maxFd = max(maxFd, context->udpSock);
    }

    rc = select(maxFd + 1, &fds, NULL, NULL, &tv);
    if (rc == 0) {
        AJ_InfoPrintf(("AJ_Net_RecvFrom(): select() timed out. status=AJ_ERR_TIMEOUT\n"));
        return AJ_ERR_TIMEOUT;
    }

    // we need to read from the first socket that has data available.

    rx = AJ_IO_BUF_SPACE(buf);
    if (context->mDnsRecvSock != INVALID_SOCKET && FD_ISSET(context->mDnsRecvSock, &fds)) {
        rx = min(rx, len);
        if (rx) {
            ret = recvfrom(context->mDnsRecvSock, buf->writePtr, rx, 0, NULL, 0);
            if (ret == -1) {
                AJ_ErrPrintf(("AJ_Net_RecvFrom(): mDnsRecvSock recvfrom() failed. errno=\"%s\"\n", strerror(errno)));
                status = AJ_ERR_READ;
            } else {
                AJ_InfoPrintf(("AJ_Net_RecvFrom(): recv'd %d from mDNS\n", (int) ret));
                buf->flags |= AJ_IO_BUF_MDNS;
                buf->writePtr += ret;
                status = AJ_OK;
                goto Finished;
            }
        }
    }

    rx = AJ_IO_BUF_SPACE(buf);
    if (context->udpSock != INVALID_SOCKET && FD_ISSET(context->udpSock, &fds)) {
        rx = min(rx, len);
        if (rx) {
            ret = recvfrom(context->udpSock, buf->writePtr, rx, 0, NULL, 0);
            if (ret == -1) {
                AJ_ErrPrintf(("AJ_Net_RecvFrom(): recvfrom() failed. errno=\"%s\"\n", strerror(errno)));
                status = AJ_ERR_READ;
            } else {
                AJ_InfoPrintf(("AJ_Net_RecvFrom(): recv'd %d from udp\n", (int) ret));
                buf->flags |= AJ_IO_BUF_AJ;
                buf->writePtr += ret;
                status = AJ_OK;
                goto Finished;
            }
        }
    }

Finished:
    if (status != AJ_OK) {
        AJ_InfoPrintf(("AJ_Net_RecvFrom(): status=%s\n", AJ_StatusText(status)));
    }
    return status;
}

static AJ_Status CloseMCastSock(AJ_MCastSocket* mcastSock)
{
    printf("++++++++++ CloseMCastSock\n");
    
    MCastContext* context = (MCastContext*)mcastSock->rx.context;
    if (context) {
        if (context->udpSock != INVALID_SOCKET) {
            close(context->udpSock);
        }
        
        if (context->mDnsSock != INVALID_SOCKET) {
            close(context->mDnsSock);
        }
        
        if (context->mDnsRecvSock != INVALID_SOCKET) {
            close(context->mDnsRecvSock);
        }
        context->udpSock = context->mDnsSock = context->mDnsRecvSock = INVALID_SOCKET;
        memset(mcastSock, 0, sizeof(AJ_MCastSocket));
    }
    return AJ_OK;
}

AJ_Status AJ_Net_MCastUp(AJ_MCastSocket* mcastSock)
{
    printf("++++++++++ AJ_Net_MCastUp\n");
    
    struct sockaddr_storage addrBuf;
    socklen_t addrLen = sizeof(addrBuf);
    struct sockaddr_in* sin;
    AJ_Status status = AJ_ERR_READ;

    mCastContext.mDnsRecvSock = MDnsRecvUp();
    if (mCastContext.mDnsRecvSock == INVALID_SOCKET) {
        printf("++++++++++ AJ_Net_MCastUp - failed 1\n");
        AJ_ErrPrintf(("AJ_Net_MCastUp(): MDnsRecvUp for mDnsRecvPort failed"));
        return status;
    }
    if (getsockname(mCastContext.mDnsRecvSock, (struct sockaddr*) &addrBuf, &addrLen)) {
        AJ_ErrPrintf(("AJ_Net_MCastUp(): getsockname for mDnsRecvPort failed"));
        printf("++++++++++ AJ_Net_MCastUp - getsockname\n");
        goto ExitError;
    }
    sin = (struct sockaddr_in*) &addrBuf;
    mCastContext.mDnsRecvPort = ntohs(sin->sin_port);

    mCastContext.mDnsRecvAddr = ntohl(chooseMDnsRecvAddr());
    if (mCastContext.mDnsRecvAddr == 0) {
        AJ_ErrPrintf(("AJ_Net_MCastUp(): no mDNS recv address"));
        printf("++++++++++ AJ_Net_MCastUp - mDnsRecvAddr\n");
        goto ExitError;
    }
    AJ_InfoPrintf(("AJ_Net_MCastUp(): mDNS recv on %d.%d.%d.%d:%d\n", ((mCastContext.mDnsRecvAddr >> 24) & 0xFF), ((mCastContext.mDnsRecvAddr >> 16) & 0xFF), ((mCastContext.mDnsRecvAddr >> 8) & 0xFF), (mCastContext.mDnsRecvAddr & 0xFF), mCastContext.mDnsRecvPort));

    mCastContext.mDnsSock = MCastUp4(MDNS_IPV4_MULTICAST_GROUP, MDNS_UDP_PORT);
    if (AJ_GetMinProtoVersion() < 10) {
        mCastContext.udpSock = MCastUp4(AJ_IPV4_MULTICAST_GROUP, 0);
    }

    if (mCastContext.udpSock != INVALID_SOCKET || mCastContext.mDnsSock != INVALID_SOCKET) {
        AJ_IOBufInit(&mcastSock->rx, rxDataMCast, sizeof(rxDataMCast), AJ_IO_BUF_RX, &mCastContext);
        mcastSock->rx.recv = AJ_Net_RecvFrom;
        AJ_IOBufInit(&mcastSock->tx, txDataMCast, sizeof(txDataMCast), AJ_IO_BUF_TX, &mCastContext);
        mcastSock->tx.send = AJ_Net_SendTo;
        printf("++++ We found the correct socket in AJ_Net_MCastUp\n");
        status = AJ_OK;
    }
    return status;

ExitError:
    close(mCastContext.mDnsRecvSock);
    return status;
}

void AJ_Net_MCastDown(AJ_MCastSocket* mcastSock)
{
    printf("++++++++++ AJ_Net_MCastDown\n");
    
    MCastContext* context = (MCastContext*) mcastSock->rx.context;
    AJ_InfoPrintf(("AJ_Net_MCastDown(mcastSock=0x%p)\n", mcastSock));

    if (context->udpSock != INVALID_SOCKET) {
        struct ip_mreq mreq;
        inet_pton(AF_INET, AJ_IPV4_MULTICAST_GROUP, &mreq.imr_multiaddr);
        mreq.imr_interface.s_addr = INADDR_ANY;
        setsockopt(context->udpSock, IPPROTO_IP, IP_DROP_MEMBERSHIP, (char*) &mreq, sizeof(mreq));
    }

    if (context->mDnsSock != INVALID_SOCKET) {
        struct ip_mreq mreq;
        inet_pton(AF_INET, MDNS_IPV4_MULTICAST_GROUP, &mreq.imr_multiaddr);
        mreq.imr_interface.s_addr = INADDR_ANY;
        setsockopt(context->udpSock, IPPROTO_IP, IP_DROP_MEMBERSHIP, (char*) &mreq, sizeof(mreq));
    }

    CloseMCastSock(mcastSock);
}
