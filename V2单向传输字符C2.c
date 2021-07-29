// 自己顺着逻辑将数据从ps单向传到pl
// 注意：各个结构体初始化顺序

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>             // Strlen function

// #include <fcntl.h>              // Flags for open()
// #include <sys/stat.h>           // Open() system call
// #include <sys/types.h>          // Types for open()
#include <sys/mman.h>           // Mmap system call
#include <sys/ioctl.h>          // IOCTL system call
#include <unistd.h>             // Close() system call
// #include <sys/time.h>           // Timing functions and definitions
// #include <getopt.h>             // Option parsing
#include <errno.h>              // Error codes

#include "libaxidma.h"          // Interface to the AXI DMA
#include "util.h"               // Miscellaneous utilities
#include "conversion.h"         // Miscellaneous conversion utilities

#define DEFAULT_TRANSFER_SIZE       60
int main()
{
    int tx_channel,rx_channel;
    const array_t *tx_chans,*rx_chans;
    size_t tx_size,rx_size; //image？???就是一般数据的缓冲区大小
    char * tx_buf, *rx_buf;
    axidma_dev_t axidma_dev;

    // 原本的parse_args工作
    // 赋予默认值
    tx_channel = -1;
    rx_channel = -1;
    tx_size = DEFAULT_TRANSFER_SIZE;
    rx_size = DEFAULT_TRANSFER_SIZE;

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
    char *txb_p, *rxb_p;
    txb_p = tx_buf;
    rxb_p = rx_buf;
    for (size_t i = 0; i < DEFAULT_TRANSFER_SIZE; i++)
    {
        txb_p[i] = 'A'+i;
        rxb_p[i] = 'a'+i;
    }
    //传输
    int rt = -1;
    rt = axidma_oneway_transfer(axidma_dev,tx_channel,tx_buf,tx_size,true);
    if (rt<0)
    {
        printf("传输失败！\n");
        return -1;
    }
    
    //通过比对数据有没有变化验证是否传输成功
    printf("接收缓存里的数据传输后为：\n");
    rxb_p = rx_buf;
    for (size_t i = 0; i < DEFAULT_TRANSFER_SIZE; i++)
    {
        printf(" %c ",rxb_p[i]);
    }
    printf("\n");
    
    int i = 0;
    for (i = 0; i < DEFAULT_TRANSFER_SIZE; i++)
    {
        if (rx_buf[i] != 'a'+i)
        {
            break;
        }
        
    }
    if (i < DEFAULT_TRANSFER_SIZE)
    {
        printf("数据传输成功啦！！！！\n");
    }
    else
    {
        printf("接收缓存里数据没有变化哟！\n");
    }
    
    return 0;
}
