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

#include "rcci_type.h";

rcci_msg_vframe_t videoFrame;

main(int argc, char *argv[])
{
    struct sockaddr_in addr;
    int fd, nbytes,addrlen;
    struct ip_mreq mreq;
    uint8_t *rcvBuf;
    int      rcvCount;

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

/**** MODIFICATION TO ORIGINAL */
    /* allow multiple sockets to use the same PORT number */
    if (setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes)) < 0) {
        perror("Reusing ADDR failed");
        return -1;;
    }
/*** END OF MODIFICATION TO ORIGINAL */

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
    rcvBuf = (uint8_t *)&videoFrame;
    const int maxBufLen = (1<<16)-40;
    int frameCnt = 0;
    printf("maxBufLen=%d\n", maxBufLen);
    rcvCount = 0;
    while (1) {
        while(true)
        {
            addrlen=sizeof(addr);

            if((nbytes=recvfrom(fd,&rcvBuf[rcvCount], maxBufLen, 0,
                                (struct sockaddr *) &addr,
                                (socklen_t *)&addrlen)) < 0) {
                perror("recvfrom");
                return -1;;
            }

            if((rcvCount == 0) &&
               (videoFrame.header.magic == rcci_msg_init_magic))
            {
                // Start of frame
                // something to do?
            }
            if((videoFrame.header.magic == rcci_msg_init_magic))
            {
                rcvCount += nbytes;
            }

            if((videoFrame.header.size == rcvCount) &&
               (videoFrame.header.magic == rcci_msg_init_magic))
            {
                // Frame received

                char fout_str[64];
                sprintf((char *)&fout_str[0], "/tmp/image%03d.jpg", frameCnt);

                printf("Dumping frame=%d (size=%d), dumping to %s\n",
                       frameCnt++,rcvCount, fout_str);


                int fout = open(fout_str, O_RDWR | O_CREAT);
                if(fout < 0)
                {
                    fprintf(stderr, "Failed to open %s for writing: %s\n",
                            fout_str, strerror(errno));
                }
                else
                {
                    write(fout, videoFrame.frame,
                          videoFrame.header.size - sizeof(rcci_msg_header_t));

                    close(fout);
                }

                rcvCount = 0;
                videoFrame.header.size = videoFrame.header.magic = 0;
            }
        }
    }
}
