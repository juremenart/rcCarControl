#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <vector>
#include <iostream>

#include "rcci_type.h"

const int cMaxFrameSize = (640*480*3);

typedef struct rcc_rx_frame_s
{
    int size_frame; // full frame size (taken from header.size)
    int cnt_frame; // frame count
    int num_all_msgs; // how many all messages
    int num_rx_msgs; // number of receiver messages
    int frame[cMaxFrameSize];

    rcc_rx_frame_s(void)
        : cnt_frame(-1), num_all_msgs(-1), num_rx_msgs(-1)
    {

    };
} rcc_rx_frame_t;

std::vector<rcc_rx_frame_t> rxFrames;

int main(int argc, char *argv[])
{
    struct sockaddr_in addr;
    int fd, nbytes,addrlen;
    struct ip_mreq mreq;
    rcci_msg_vframe_t videoFrame;

    int port;
    u_int yes=1;            /*** MODIFICATION TO ORIGINAL */

    if(argc != 3)
    {
        fprintf(stderr, "Usage: %s <hostname> <port>\n", argv[0]);
        return -1;
    }
    /* create what looks like an ordinary UDP socket */
    if ((fd=socket(AF_INET,SOCK_DGRAM,0)) < 0) {
        perror("socket");
        return -1;
    }

    port = (int)strtod(argv[2], NULL);

    /* allow multiple sockets to use the same PORT number */
    if (setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes)) < 0) {
        perror("Reusing ADDR failed");
        return -1;;
    }

    /* set up destination address */
    memset(&addr,0,sizeof(addr));
    addr.sin_family=PF_INET;
    addr.sin_addr.s_addr=htonl(INADDR_ANY); /* N.B.: differs from sender */
    addr.sin_port=htons(port);

    /* bind to receive address */
    if (bind(fd,(struct sockaddr *) &addr,sizeof(addr)) < 0) {
        perror("bind");
        return -1;;
    }

    /* use setsockopt() to request that the kernel join a multicast group */
    mreq.imr_multiaddr.s_addr=inet_addr("226.0.0.1");
    mreq.imr_interface.s_addr=htonl(INADDR_ANY);
    if (setsockopt(fd,IPPROTO_IP,IP_ADD_MEMBERSHIP,&mreq,sizeof(mreq)) < 0) {
        perror("setsockopt");
        return -1;;
    }

    /* now just enter a read-print loop */
    const int maxBufLen = rcci_msg_vframe_max_packet_size;
    int frameCnt = 0;
    printf("maxBufLen=%d\n", maxBufLen);
    rxFrames.resize(0);

    while (1) {
        while(true)
        {
            addrlen=sizeof(addr);

            if((nbytes=recvfrom(fd,&videoFrame, maxBufLen, 0,
                                (struct sockaddr *) &addr,
                                (socklen_t *)&addrlen)) < 0) {
                perror("recvfrom");
                return -1;;
            }

            if((videoFrame.header.magic == rcci_msg_init_magic))
            {
                // search if we have frame in the structure already
                std::vector<rcc_rx_frame_t>::iterator it;
                /* Valid frame check if we have already thig count */
                std::cout << "Reciving frame number " << (int)videoFrame.cnt_frame
                          << " ( " << (int)(videoFrame.cur_msg+1) << " / "
                          << (int)videoFrame.all_msgs << " )"
                          << " frame_size=" << videoFrame.header.size
                          << " cur_ptr=" << videoFrame.idx_frame
                          << " cur_size=" << videoFrame.size_frame << std::endl;

                for(it = rxFrames.begin(); it != rxFrames.end(); it++)
                {
                    if(it->cnt_frame == (int)videoFrame.cnt_frame)
                    {
                        if(it->size_frame == (int)videoFrame.header.size)
                        {
                            break;
                        }
                        else
                        {
                            // somethings wrong - cnt matches but not size
                            std::cerr
                                << "Counter matches but not size"
                                << ", deleting old frame!" << std::endl;
                            rxFrames.erase(it);
                            it = rxFrames.end();
                            break;
                        }
                    }
                }

                // not found so create new one
                if(it == rxFrames.end())
                {
                    rcc_rx_frame_t rxFrame;
                    rxFrame.size_frame = videoFrame.header.size;
                    rxFrame.cnt_frame = videoFrame.cnt_frame;
                    rxFrame.num_all_msgs = videoFrame.all_msgs;
                    rxFrame.num_rx_msgs = 0;
                    rxFrames.push_back(rxFrame);
                    it = --rxFrames.end();
                    std::cout << "New frame (cnt=" << rxFrame.cnt_frame
                              << " num_msgs=" << rxFrame.num_all_msgs
                              << " size=" << rxFrame.size_frame
                              << " )" << std::endl;
                }

                it->num_rx_msgs += 1;
//                std::cout << "Copying to=" << (int)videoFrame.idx_frame
//                          << " size=" << (int)videoFrame.size_frame
//                          << std::endl;

                memcpy((void *)&it->frame[videoFrame.idx_frame],
                       (void *)&videoFrame.frame[0],
                       videoFrame.size_frame);

                if(it->num_rx_msgs == it->num_all_msgs)
                {
                    char fout_str[64];
                    sprintf((char *)&fout_str[0], "/tmp/image%03d.jpg", frameCnt++);

                    printf("Dumping frame=%d (size=%d), dumping to %s\n",
                           it->cnt_frame,it->size_frame, fout_str);


                    int fout = open(fout_str, O_RDWR | O_CREAT);
                    if(fout < 0)
                    {
                        fprintf(stderr, "Failed to open %s for writing: %s\n",
                                fout_str, strerror(errno));
                    }
                    else
                    {
                        write(fout, it->frame, it->size_frame);
                        close(fout);
                    }

                    videoFrame.header.size = videoFrame.header.magic = 0;

                    // erase also all frames with older frame counters
                    int cur_frame_cnt = it->cnt_frame;
                    for(it = rxFrames.begin(); it != rxFrames.end(); it++)
                    {
                        if(it->cnt_frame <= cur_frame_cnt)
                        {
                            std::cout << "Removing frame " << it->cnt_frame
                                      << " from vector" << std::endl;
                            it = rxFrames.erase(it);
                            if(it == rxFrames.end())
                            {
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
    return 0;
}
