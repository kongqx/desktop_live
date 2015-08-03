#ifndef __RTSP_H__
#define __RTSP_H__

#include "list.h"
#include <WinSock2.h>



enum describe_mode
{
	none=0,
	sdp=1
};

typedef struct 
{
	SOCKET rtsp_socket;
	char cmd[20];
	char url[260];
	char version[20];
	char cseq[10];
	enum describe_mode mode;
	char time_buf[128];
	char *send_buf;
	int send_len;
	void *media;
	char *server_ip;
	SOCKADDR_IN dest_addr;

	//���������
	struct list_head list;

	//rtp������ͷ
	struct list_head rtp_head;
}RTSP;

int parse_recv_buffer(struct list_head *rtsp_head, RTSP *rtsp, char *recv_buf, int size);

#endif