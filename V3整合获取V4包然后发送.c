// 自己顺着逻辑将数据从ps单向传到pl
// 注意：各个结构体初始化顺序

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>             // Strlen function

#include <sys/mman.h>           // Mmap system call
#include <sys/ioctl.h>          // IOCTL system call
#include <unistd.h>             // Close() system call
#include <errno.h>              // Error codes

#include "libaxidma.h"          // Interface to the AXI DMA
#include "util.h"               // Miscellaneous utilities
#include "conversion.h"         // Miscellaneous conversion utilities

//------------------------------------------------------------------
//recv.c
// + change socket with ip4 to ipv6
// + add some structures to analyse the  Mac packet

#include <stdio.h>
#include <stdlib.h>          //perror
#include <string.h>          //strcpy,memset
#include <sys/socket.h>      //socket
#include <sys/ioctl.h>       //ioctl
#include <net/if.h>          //ifreq
#include <linux/if_packet.h> //sockaddr_sll
#include <linux/if_ether.h>  //ETH_P_ALL

#include <unistd.h>    //close
#include <arpa/inet.h> //htons

#include <linux/udp.h>
#include <linux/ip.h>
#include <arpa/inet.h>

#include <netinet/if_ether.h> //ether_header
#include <net/ethernet.h>

#define IFRNAME "eth0"
#define BUF_SIZE 2048
unsigned char buf[BUF_SIZE]; //接收v6包的buffer
unsigned char sendBuf[BUF_SIZE];//去v6头改mac地址处理后的发送buffer
int lenMac4 = 0;

// + add the tpv6 header structure
    typedef struct _ipv6_addr
    {
        union {
            uint8_t __u6_addr8[16];
            uint16_t __u6_addr16[8];
            uint32_t __u6_addr32[4];
        } __in6_u;
    } ipv6_addr;

    typedef struct _IP6Hdr
    {
        uint32_t ip6_version : 4;
        uint32_t ip6_priority : 8;
        uint32_t ip6_flow_lbl : 20;
        uint16_t ip6_payload_len; /* PayLoad Length */
        uint8_t ip6_nexthdr;      // Next Header
        uint8_t ip6_hop_limit;    // Hop Limit
        uint16_t ipv6_saddr[8];
        uint16_t ipv6_daddr[8];
        //ipv6_addr ipv6_Saddr;
        //ipv6_addr ipv6_Daddr;
    } IP6Hdr;

    // + add ip and mac structure
    typedef struct IpMacMap
    {
        uint8_t ip_addrT; // designed to a array just to easy to initialize
        uint8_t mac_addr[6];
    } ipMacMap;
//---------------------------------------------------------------------


int main()
{
    //------------------------------------------------------------------
    ipMacMap ipMacs[] = {
        {232, {0x4a, 0x59, 0x30, 0x66, 0x54, 0x54}},
        {233, {0x4a, 0x59, 0x71, 0x44, 0x36, 0x47}},
        {114, {0xdc, 0xa6, 0x32, 0xf5, 0x76, 0x54}},
        {64, {0x2c, 0xa5, 0x9c, 0x3b, 0xd7, 0xfc}}};

    IP6Hdr *ip6h;
    unsigned char *p;

    int sfd, len;
    struct sockaddr_ll sll;
    struct ifreq ifr;

    //if ((sfd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL))) == -1)
    if ((sfd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_IPV6))) == -1)
    {
        perror("socket");
        printf("create socket error");
        return 0;
    }
    memset(&ifr, 0, sizeof(ifr));
    strcpy(ifr.ifr_name, IFRNAME);
    if ((ioctl(sfd, SIOCGIFINDEX, &ifr)) == -1)
    {
        perror("ioctl 1");
        close(sfd);
        return 0;
    }
    memset(&sll, 0, sizeof(sll));
    sll.sll_family = PF_PACKET;
    sll.sll_ifindex = ifr.ifr_ifindex;
    //sll.sll_protocol = htons(ETH_P_ALL);
    sll.sll_protocol = htons(ETH_P_IPV6);

    int length = sizeof(sll);
    if ((bind(sfd, (struct sockaddr *)&sll, sizeof(sll))) == -1)
    {
        perror("bind");
        close(sfd);
        return 0;
    }
    while (1)
    {
        len = recvfrom(sfd, buf, sizeof(buf), 0, (struct sockaddr *)&sll, (socklen_t *)&length);
        //the MAC layer packet is the same level of MAC as wiresahrk

        // --------------------anylyse------------------
        //++++++++mac header++++++++
        struct udphdr *udph;
        struct ether_header *ether;
        ether = (struct ether_header *)buf; //若数据是从数据链路曾抓取的，那么就有这个以太网帧头

        printf("shu ju bao lei xing xie yi:0x%x\n", ntohs(ether->ether_type)); //指出上层使用的协议（ip协议--0x0800，ipv6--0x86dd）
        printf("v6 packet de mac_daddr:");
        for (size_t i = 0; i < 6; i++)
        {
            printf("%x ", ether->ether_dhost[i]);
        }

        if (ntohs(ether->ether_type) == 0x86dd)
        {
            printf("This is a V6 Packet!\n");

            //+++++++++++++++ip+++++++++++

            ip6h = (IP6Hdr *)(buf + sizeof(struct ether_header));           //put the buffer point forward until arrive ip header
            printf("\nip6_payload_len:%x\n", ntohs(ip6h->ip6_payload_len)); // 002c --> 2c00 --> 002c ->2c
            printf("IP6_saddr=");
            for (size_t i = 0; i < 8; i++)
            {
                printf("%x ", ip6h->ipv6_saddr[i]);
            }
            printf("\nip6_daddr:");
            for (size_t i = 0; i < 8; i++)
            {
                printf("%x ", ip6h->ipv6_daddr[i]);
            }

            printf("\n");

            if (ip6h->ip6_nexthdr == 0x11) // emmm, I forget why I write the if here!!!!!!!!!!!But if without the if, there would have error.
            {
                //+++++++transport layer+++++++++++++++++
                //udph = (struct udphdr *)(buf + sizeof(ip6h));
                udph = (struct udphdr *)(buf + 54);
                printf("Source Port::%d\n", ntohs(udph->source));
                printf("Dest Port::%d\n", ntohs(udph->dest));
                u_int16_t lenv4 = (unsigned short)(ntohs(udph->len)) - sizeof(struct udphdr);
                printf("udp len(udp Header + data)::%d\n\n\n", lenv4);

                //++++++++++DATA(with IPv4 header)++++++++++++++++++++++
                //rebuild a packet ipv4 version---change the MAC
                ///* want to have the ipv4 header
                struct iphdr *iph = (struct iphdr *)(buf + 54 + sizeof(struct udphdr));
                uint8_t ipaddr[4] = {ntohl(iph->daddr)};

                printf("ipv4_saddr:");
                printf("%x   ", ntohl(iph->saddr));
                uint8_t ipv4_saddr = ntohl(iph->saddr);
                printf("ipv4_saddr[3]=%x\n", ipv4_saddr);

                printf("ipv4_daddr:%x   ", ntohl(iph->daddr));
                uint8_t ipv4_daddr = ntohl(iph->daddr);
                printf("ipv4_daddr[3]=%x\n", ipv4_daddr);

                uint8_t mac_addr[6];
                for (size_t i = 0; i < 4; i++)
                {
                    if (ipv4_daddr == ipMacs[i].ip_addrT)
                    {
                        printf("\naccording dip the dmac should be:\n");

                        printf("ip:%x find mac:", ipv4_daddr);
                        for (size_t j = 0; j < 6; j++)
                        {
                            mac_addr[j] = ipMacs[i].mac_addr[j];
                            printf("%x ", mac_addr[j]);
                        }

                        break;
                    }
                }
                //*/

                printf("\nipv4 len::%d\n", lenv4);
                p = buf + 54 + sizeof(struct udphdr);
                printf("p:%x\n", *p);

                int i = 0;
                for (i = 0; i < 6; i++)
                {
                    sendBuf[i] = mac_addr[i];
                }

                for (; i < sizeof(struct ether_header); i++)
                {
                    sendBuf[i] = buf[i];
                }
                int j = 0;
                for (; i < lenv4 + sizeof(struct ether_header); i++)
                {
                    sendBuf[i] = p[j];
                    j++;
                }

                printf("the rebuilt packet:\n");
                lenMac4 = sizeof(struct ether_header) + lenv4;
                for (i = 0; i < lenMac4; i++)
                {
                    printf("%.2x ", (unsigned char *)sendBuf[i]);
                    if ((i + 1) % 16 == 0)
                    {
                        printf("\n");
                    }
                }
                if (len > 0)
                {
                    printf("\nrecv buf: %d\n", len);
                    for (i = 0; i < len; i++)
                    {

                        printf("%.2x ", (unsigned char *)buf[i]);
                        //printf("%c", buf[i]);
                        if ((i + 1) % 16 == 0)
                        {
                            printf("\n");
                        }
                    }
                }
                printf("\n----------------------------------------------------------------\n\n");
            }
        }
    }
    close(sfd);
    //------------------------------------------------------------------

    int tx_channel,rx_channel;
    const array_t *tx_chans,*rx_chans;
    size_t tx_size,rx_size; //image？???就是一般数据的缓冲区大小
    unsigned char * tx_buf, *rx_buf;
    axidma_dev_t axidma_dev;

    // 原本的parse_args工作
    // 赋予默认值
    tx_channel = -1;
    rx_channel = -1;
    tx_size = lenMac4;
    rx_size = lenMac4;

    printf("AXI DMA Parameters:\n");
    printf("\tTransmit Buffer Size:%0.2f MiB\n",BYTE_TO_MIB(tx_size));
    printf("\tRecive Buffer Size:%0.2f MiB\n",BYTE_TO_MIB(rx_size));

    // 初始化AXI DMA device
    axidma_dev = axidma_init();
    if (axidma_dev == NULL)
    {
        printf("Failed to initialized the AXI DMA device.\n");
        return -1;
    }

    // 为发送和接收分配内存
    tx_buf = axidma_malloc(axidma_dev,tx_size);
    if (tx_buf == NULL)
    {
        printf("Unable to allocate transmit buffer from the AXI DMA device.");
        return -1;
    }
    rx_buf = axidma_malloc(axidma_dev,rx_size);
    if (rx_buf == NULL)
    {
        printf("Unable to allocate receive buffer from the AXI DMA device.");
        return -1;
    }

    // 获取所有发送和接收channel
    tx_chans = axidma_get_dma_tx(axidma_dev);
    rx_chans = axidma_get_dma_rx(axidma_dev);
    if (tx_chans->len < 1)
    {
        printf("Error: No transmit channels were found.\n");
        return -1;
    }

    if (rx_chans->len < 1)
    {
        printf("Error: No receive channels were found.\n");
        return -1;
    }
    
    tx_channel = tx_chans->data[0];
    rx_channel = rx_chans->data[0];
    printf("Uisng transmit channel %d and receive channel %d.\n",tx_channel,rx_channel);


    //-------------进行传输---------------
    //初始化缓冲区
    unsigned char *txb_p, *rxb_p;
    txb_p = tx_buf;
    rxb_p = rx_buf;
    for (size_t i = 0; i < lenMac4; i++)
    {
        txb_p[i] = sendBuf[i];
    }
    //传输
    int rt = -1;
    rt = axidma_oneway_transfer(axidma_dev,tx_channel,tx_buf,tx_size,true);
    if (rt<0)
    {
        printf("传输失败！\n");
        return -1;
    }
    
    return 0;
}
