#ifndef __RTP_H__
#define __RTP_H__

#include <WinSock2.h>
#include "list.h"

enum stream_type
{
	video=0,
	audio=1,
	subtitle=2
};

typedef struct 
{
	/*  0                   1                   2                   3
	0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	|V=2|P|X|  CC   |M|     PT      |       sequence number         |
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	|                           timestamp                           |
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	|           synchronization source (SSRC) identifier            |
	+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
	|            contributing source (CSRC) identifiers             |
	|                             ....                              |
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    */
	//intel ��cpu ��intelΪС���ֽ��򣨵Ͷ˴浽�׵�ַ�� ��������Ϊ����ֽ��򣨸߶˴浽�͵�ַ��
	/*intel ��cpu �� �߶�->csrc_len:4 -> extension:1-> padding:1 -> version:2 ->�Ͷ�
	 ���ڴ��д洢 ��
	 ��->4001���ڴ��ַ��version:2
	     4002���ڴ��ַ��padding:1
		 4003���ڴ��ַ��extension:1
	 ��->4004���ڴ��ַ��csrc_len:4

     ���紫����� �� �߶�->version:2->padding:1->extension:1->csrc_len:4->�Ͷ�  (Ϊ��ȷ���ĵ�������ʽ)

	 ��������ڴ� ��
	 ��->4001���ڴ��ַ��version:2
	     4002���ڴ��ַ��padding:1
	     4003���ڴ��ַ��extension:1
	 ��->4004���ڴ��ַ��csrc_len:4
	 �����ڴ���� ���߶�->csrc_len:4 -> extension:1-> padding:1 -> version:2 ->�Ͷ� ��
	 ����
	 unsigned char csrc_len:4;        // expect 0 
	 unsigned char extension:1;       // expect 1
	 unsigned char padding:1;         // expect 0 
	 unsigned char version:2;         // expect 2 
	*/
	/* byte 0 */
	 unsigned char csrc_len:4;        /* expect 0 */
	 unsigned char extension:1;       /* expect 1, see RTP_OP below */
	 unsigned char padding:1;         /* expect 0 */
	 unsigned char version:2;         /* expect 2 */
	/* byte 1 */
	 unsigned char payloadtype:7;     /* RTP_PAYLOAD_RTSP */
	 unsigned char marker:1;          /* expect 1 */
	/* bytes 2,3 */
	 unsigned int seq_no;            
	/* bytes 4-7 */
	 unsigned int timestamp;        
	/* bytes 8-11 */
	 unsigned int ssrc;              /* stream number is used here. */
} RTP_HEADER;

typedef struct 
{
	SOCKET rtp_socket;
	SOCKET rtcp_socket;
	enum stream_type type;
	SOCKADDR_IN dest_addr;

	struct list_head list;
}RTP;

int init_rtp_socket(RTP *rtp);

#endif