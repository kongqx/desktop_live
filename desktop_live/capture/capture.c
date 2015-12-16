#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <Windows.h>
#include "capture.h"
#include "list.h"

#ifndef MIN
#  define MIN(a,b)  ((a) > (b) ? (b) : (a))
#endif

#ifndef MAX
#  define MAX(a,b)  ((a) < (b) ? (b) : (a))
#endif

#pragma comment(lib, "Winmm.lib")
#pragma comment(lib, "log.lib")

typedef struct node
{
	uint8_t *data;
	unsigned long size;
	struct list_head list;
}NODE;

typedef struct
{
	uint8_t *yuv;
	uint8_t *rgba;
	unsigned long rgba_len;
	unsigned long yuv_len;
	int fps;
}VIDEO, *PVIDEO;

typedef struct  
{
	//��Ļ���
	int width;
	int height;

	//��Ļλͼ�Ŀ�����ͨ����
	int bitmap_width;
	int bitmap_height;
	int bitmap_depth;
	int bitmap_channel;

	//һ֡λͼ�����ݳ��� h*w*d*c
	long len;
}SCREEN, *PSCREEN;

typedef struct 
{
	int channels;//2
	int bits_per_sample;//16
	int samples_per_sec;//48000
	int avg_bytes_per_sec;//48000

	unsigned long pcm_len;
	uint8_t *pcm;
}AUDIO, *PAUDIO;

typedef struct capture
{
	int initialized;
	int started;
#define ARRAY_LEN	2
#define VIDEO_INDEX 0
#define AUDIO_INDEX 1
	HANDLE handler[2];
	int stop;
	RTL_CRITICAL_SECTION cs[2];
	struct list_head head[2];
	int width;
	int height;

	AUDIO audio;
	VIDEO video;
	SCREEN screen;
}CAPTURE, *PCAPTURE;

static CAPTURE s_capture = {0};

//����:��ȡ��Ļ�Ŀ�ߣ���Ļλͼ�Ŀ�ߡ���ȡ�ͨ������һ֡λͼ�ĳ���
void GetScreenInfo(PSCREEN pScreen)
{
	HDC src = NULL;
	HDC mem = NULL;
	HBITMAP bitmap = NULL;
	HBITMAP old_bitmap = NULL;
	BITMAP bmp;

	int left = 0;
	int top = 0;
	int right = GetSystemMetrics(SM_CXSCREEN);
	int bottom = GetSystemMetrics(SM_CYSCREEN);

	PRINTLOG(LOG_DEBUG, ">>%s %s %d\n",__FILE__, __FUNCTION__, __LINE__);

	pScreen->width = right - left;
	pScreen->height = bottom - top;

	src = CreateDC("DISPLAY", NULL, NULL, NULL);
	mem = CreateCompatibleDC(src);

	bitmap = CreateCompatibleBitmap(src, pScreen->width, pScreen->height);
	old_bitmap = (HBITMAP)SelectObject(mem, bitmap);

	BitBlt(mem, 0, 0, pScreen->width, pScreen->height, src, 0, 0, SRCCOPY);
	bitmap = (HBITMAP)SelectObject(mem, old_bitmap);
	GetObject(bitmap, sizeof(BITMAP), &bmp);

	pScreen->bitmap_channel = bmp.bmBitsPixel == 1 ? 1 : bmp.bmBitsPixel/8 ;
	pScreen->bitmap_depth = bmp.bmBitsPixel == 1 ? 1 : 8;//IPL_DEPTH_1U : IPL_DEPTH_8U;
	pScreen->bitmap_width = bmp.bmWidth;
	pScreen->bitmap_height = bmp.bmHeight;
	pScreen->len = pScreen->bitmap_channel * (pScreen->bitmap_depth/8) * \
					pScreen->bitmap_width * pScreen->bitmap_height;

	PRINTLOG(LOG_INFO, "screen->width=%d screen->height=%d screen->bitmap_width=%d screen->bitmap_height=%d screen->len=%d\n",
					   pScreen->width, pScreen->height, pScreen->bitmap_width, pScreen->bitmap_height, pScreen->len);

	SelectObject(mem,old_bitmap);
	DeleteObject(old_bitmap);
	DeleteDC(mem);
	SelectObject(src,bitmap);
	DeleteDC(mem);
	DeleteObject(bitmap);

	PRINTLOG(LOG_DEBUG, "<<%s %s %d\n",__FILE__, __FUNCTION__, __LINE__);
}

int InitCaptureVideoParam(PSCREEN pScreen, PVIDEO pVideo, PCAPTURECONFIG pCaptureConfig)
{
	PRINTLOG(LOG_DEBUG, ">>%s %s %d\n",__FILE__, __FUNCTION__, __LINE__);

	pVideo->fps = pCaptureConfig->fps;
	PRINTLOG(LOG_INFO,"pVideo->fps = %d\n", pVideo->fps);

	PRINTLOG(LOG_DEBUG, "<<%s %s %d\n",__FILE__, __FUNCTION__, __LINE__);
	return INIT_SECCESS;
}

//����:�������豸���ڴ�λͼ���Ƶ�video->rgba
void CopyScreenBitmap(PSCREEN pScreen, PVIDEO pVideo)
{
	HDC src = NULL;
	HDC mem = NULL;
	HBITMAP bitmap = NULL;
	HBITMAP old_bitmap = NULL;

	src = CreateDC("DISPLAY", NULL, NULL, NULL);
	mem = CreateCompatibleDC(src);
	bitmap = CreateCompatibleBitmap(src, pScreen->width, pScreen->height);
	old_bitmap = (HBITMAP)SelectObject(mem, bitmap);

	BitBlt(mem, 0, 0, pScreen->width, pScreen->height, src, 0, 0 , SRCCOPY);
	bitmap = (HBITMAP)SelectObject(mem, old_bitmap);
	GetBitmapBits(bitmap, pScreen->len, pVideo->rgba);
	
	SelectObject(mem,old_bitmap);
	DeleteObject(old_bitmap);
	DeleteDC(mem);
	SelectObject(src,bitmap);
	DeleteDC(mem);
	DeleteObject(bitmap);
}

//TRUE=�ɹ� FALSE=ʧ�ܣ��㷨���ᷴ��false��
//����:rgba��ʽ������ת��yuv420p��ʽ������
BOOL RGBA2YUV(LPBYTE RgbaBuf,UINT nWidth,UINT nHeight,LPBYTE yuvBuf,unsigned long *len, int widthStep)
{
	unsigned int i, j;
	unsigned char*bufY, *bufU, *bufV, *bufRGB;
	unsigned char y, u, v, r, g, b, a;
	unsigned int ylen = nWidth * nHeight;
	unsigned int ulen = (nWidth * nHeight)/4;
	unsigned int vlen = (nWidth * nHeight)/4; 
	memset(yuvBuf,0,(unsigned int )*len);
	bufY = yuvBuf;
	bufV = yuvBuf + nWidth * nHeight;
	bufU = bufV + (nWidth * nHeight* 1/4);
	*len = 0; 

	if (widthStep == 0)
		widthStep = nWidth*4;

	for (j = 0; j<nHeight;j++)
	{
		bufRGB = RgbaBuf + widthStep*j;
		for (i = 0;i<nWidth;i++)
		{
			b = *(bufRGB++);
			g = *(bufRGB++);
			r = *(bufRGB++);
			a = *(bufRGB++);//a������û��ʹ��

			y = (unsigned char)( ( 66 * r + 129 * g +  25 * b + 128) >> 8) + 16  ;          
			u = (unsigned char)( ( -38 * r -  74 * g + 112 * b + 128) >> 8) + 128 ;          
			v = (unsigned char)( ( 112 * r -  94 * g -  18 * b + 128) >> 8) + 128 ;
			*(bufY++) = MAX( 0, MIN(y, 255 ));
			if (j%2==0&&i%2 ==0)
			{
				if (u>255)
				{
					u=255;
				}
				if (u<0)
				{
					u = 0;
				}
				*(bufU++) =u;
				//��u����
			}
			else
			{
				//��v����
				if (i%2==0)
				{
					if (v>255)
					{
						v = 255;
					}
					if (v<0)
					{
						v = 0;
					}
					*(bufV++) =v;
				}
			}
		}
	}
	*len = nWidth * nHeight + (nWidth*nHeight)/2;
	return TRUE;
} 

//����ֵ 0=�ɹ� ����=ʧ��
//����:�����ݴ�����У�һ������
//ע��:�̲߳���ȫ������ǰ�����������
static int enqueue(struct list_head *head, uint8_t *data, unsigned long size)
{
	int ret = 0;
	
	NODE *node = (NODE *)malloc(sizeof(NODE));
	if (NULL == node)
	{
		ret = -1;
		return ret;
	}

	memset(node, 0, sizeof(NODE));
	node->data = (uint8_t *)malloc(size);
	if (NULL == node->data)
	{
		free(node);
		ret = -2;
		return ret;
	}

	memcpy(node->data, data, size);
	node->size = size;
	list_add_tail(&node->list, head);

	ret = 0;
	return ret;
}

//����ֵ 0=�ɹ� ����=ʧ��
//����:�Ӷ��У�һ��������������
//ע��:�̲߳���ȫ������ǰ�����������
static int dequeue(struct list_head *head, uint8_t **data, unsigned long *size)
{
	int ret = 0;
	struct list_head *plist = NULL;

	if (0 != list_empty(head))
	{
		ret = -1;
		return ret;
	}

	list_for_each(plist, head) 
	{
		NODE *node = list_entry(plist, struct node, list);
		*data = (uint8_t *)malloc(node->size);
		if (NULL == *data)
		{
			ret = -2;
			return ret;
		}

		*size = node->size;
		memcpy(*data, node->data, node->size);

		list_del(plist);
		free(node->data);
		free(node);
		break;
	}

	ret = 0;
	return ret;
}

int MallocVideobuffer(PSCREEN pScreen, PVIDEO pVideo)
{
	PRINTLOG(LOG_DEBUG, ">>%s %s %d\n",__FILE__, __FUNCTION__, __LINE__);

	pVideo->yuv_len = pScreen->bitmap_width * \
		pScreen->bitmap_height*2;
	pVideo->yuv = (uint8_t *)malloc(pVideo->yuv_len);
	if (!pVideo->yuv)
	{
		return MALLOCFAILED;
	}

	PRINTLOG(LOG_DEBUG, "pVideo->yuv malloc %d\n", pVideo->yuv_len);

	pVideo->rgba_len =  pScreen->bitmap_width * pScreen->bitmap_height * \
		pScreen->bitmap_channel * (pScreen->bitmap_depth/8);
	pVideo->rgba = (uint8_t *)malloc(pVideo->rgba_len);
	if (!pVideo->rgba)
	{
		return MALLOCFAILED;
	}

	PRINTLOG(LOG_DEBUG, "pVideo->rgba malloc %d\n", pVideo->rgba_len);
	PRINTLOG(LOG_DEBUG, "<<%s %s %d\n",__FILE__, __FUNCTION__, __LINE__);

	return SECCESS;
}

//����:�ɼ���Ƶ�����߼���������Ļ��Ϣ��ȡ�������ļ�������ȡ���ɼ�ת������
unsigned int __stdcall VideoCaptureProc(void *p)
{
	unsigned int step_time = 0;
	DWORD start = 0;
	DWORD end = 0;

	PRINTLOG(LOG_DEBUG, ">>%s %s %d\n",__FILE__, __FUNCTION__, __LINE__);

	if (SECCESS != 
		MallocVideobuffer(&s_capture.screen, &s_capture.video))
	{
		return MALLOCFAILED;
	}

	step_time = 1000 / s_capture.video.fps;
	PRINTLOG(LOG_INFO, "capture step_time=%d\n", step_time);

	while (0 == s_capture.stop)
	{
		start = timeGetTime();
		CopyScreenBitmap(&s_capture.screen ,&s_capture.video);
		RGBA2YUV((LPBYTE)s_capture.video.rgba, 
					s_capture.screen.bitmap_width, 
					s_capture.screen.bitmap_height, 
					(LPBYTE)s_capture.video.yuv, 
					&s_capture.video.yuv_len, 
					s_capture.screen.bitmap_width*4);

		EnterCriticalSection(&s_capture.cs[VIDEO_INDEX]);
		enqueue(&s_capture.head[VIDEO_INDEX], s_capture.video.yuv, s_capture.video.yuv_len);
		LeaveCriticalSection(&s_capture.cs[VIDEO_INDEX]);

		while((((end = timeGetTime()) - start) < step_time))
			;
	}

	free(s_capture.video.yuv);
	PRINTLOG(LOG_DEBUG, "free video.yuv\n");

	free(s_capture.video.rgba);
	PRINTLOG(LOG_DEBUG, "free video.rgba\n");


	PRINTLOG(LOG_DEBUG, "<<%s %s %d\n",__FILE__, __FUNCTION__, __LINE__);
	return 0;
}

void InitCaptureAudioParam(PAUDIO pAudio, PCAPTURECONFIG pCaptureConfig)
{
	PRINTLOG(LOG_DEBUG, ">>%s %s %d\n",__FILE__, __FUNCTION__, __LINE__);

	pAudio->channels = pCaptureConfig->channels;
	pAudio->bits_per_sample = pCaptureConfig->bits_per_sample;
	pAudio->samples_per_sec = pCaptureConfig->samples_per_sec;
	pAudio->avg_bytes_per_sec = pCaptureConfig->avg_bytes_per_sec;
	PRINTLOG(LOG_INFO, "pAudio->channels=%d pAudio->bits_per_sample=%d pAudio->samples_per_sec=%d pAudio->avg_bytes_per_sec=%d\n",
		pAudio->channels, pAudio->bits_per_sample, pAudio->samples_per_sec, pAudio->avg_bytes_per_sec);

	PRINTLOG(LOG_DEBUG, "<<%s %s %d\n",__FILE__, __FUNCTION__, __LINE__);
}

//����:��ʼ�����ҿ���wave�ɼ�
int StartWave(PAUDIO pAudio, PWAVEFORMATEX pWaveFormat, 
				PWAVEHDR *wavehdr, HWAVEIN *wavein, const int HDRCOUNT)
{
	int ret = 0;
	int i = 0;
	int size = 1024*24;

	PRINTLOG(LOG_DEBUG, ">>%s %s %d\n",__FILE__, __FUNCTION__, __LINE__);
	PRINTLOG(LOG_INFO, "wavein header buffer size=%d header count=%d\n",size, HDRCOUNT);

	pAudio->pcm_len = 0;
	pAudio->pcm = (uint8_t *)malloc(size * HDRCOUNT);
	if (NULL == pAudio->pcm)
	{
		return MALLOCFAILED;
	}
	PRINTLOG(LOG_DEBUG, "audio->pcm = malloc %d\n", size*HDRCOUNT);

	pWaveFormat->wFormatTag = WAVE_FORMAT_PCM;								//������ʽ
	pWaveFormat->nChannels = pAudio->channels;								//������
	pWaveFormat->nSamplesPerSec = pAudio->samples_per_sec;					//������
	pWaveFormat->nAvgBytesPerSec = pAudio->avg_bytes_per_sec;					//ƽ������
	pWaveFormat->nBlockAlign = pAudio->channels * pAudio->bits_per_sample / 8;	//������*ÿ������ռ���ֽ���
	pWaveFormat->wBitsPerSample = pAudio->bits_per_sample;					//����ռ��λ��
	pWaveFormat->cbSize = 0;													//�ر���Ϣ����

	ret = waveInOpen(wavein, WAVE_MAPPER, pWaveFormat, (DWORD)NULL, 0L, CALLBACK_NULL);
	if (ret != MMSYSERR_NOERROR)
	{
		return WAVEINOPENFAILED;
	}

	for (i=0; i<HDRCOUNT; i++)
	{
		wavehdr[i] = (WAVEHDR*)malloc(size + sizeof(WAVEHDR));
		if (NULL == wavehdr[i])
		{
			return WAVEINOPENFAILED;
		}
		PRINTLOG(LOG_DEBUG, "wavehdr[%d] = malloc %d\n", i, size*HDRCOUNT);

		memset(wavehdr[i], 0, size + sizeof(WAVEHDR));
		wavehdr[i]->dwBufferLength = size;
		wavehdr[i]->lpData = (LPSTR)(wavehdr[i] + 1);//(LPSTR)((char *)wavehdr[i]+sizeof(WAVEHDR));

		ret = waveInPrepareHeader(*wavein, wavehdr[i], sizeof(WAVEHDR));
		if (ret != MMSYSERR_NOERROR)
		{
			return WAVEINPREPAREHEADERFAILED;
		}

		ret = waveInAddBuffer(*wavein, wavehdr[i], sizeof(WAVEHDR));
		if (ret != MMSYSERR_NOERROR)
		{
			return WAVEINADDBUFFERFAILED;
		}
	}

	ret = waveInStart(*wavein);
	if (ret != MMSYSERR_NOERROR)
	{
		return WAVEINSTARTFAILED;
	}

	PRINTLOG(LOG_DEBUG, "<<%s %s %d\n",__FILE__, __FUNCTION__, __LINE__);
	return SECCESS;
}

//����:��Ƶ�ɼ����߼�
unsigned int __stdcall AudioCaptureProc(void *p)
{
	int i = 0;
	int ret = 0;
	const int HDRCOUNT = 10;
	int hdr = 0;
	WAVEHDR *wavehdr[10];
	WAVEFORMATEX waveformat = {0};
	HWAVEIN wavein = {0};
	char log_str[1024] = {0};

	PRINTLOG(LOG_DEBUG, ">>%s %s %d\n",__FILE__, __FUNCTION__, __LINE__);

	ret = StartWave(&s_capture.audio, &waveformat, &wavehdr[0], &wavein, HDRCOUNT);
	if (SECCESS != ret)
	{
		PRINTLOG(LOG_ERROR, ">>%s %s %d StartWave failed ret = %d\n",
			__FILE__, __FUNCTION__, __LINE__, ret);
		return ret;
	}


	while (0 == s_capture.stop)
	{
		Sleep(100);
		while (wavehdr[hdr]->dwFlags & WHDR_DONE)
		{
			memcpy(s_capture.audio.pcm + s_capture.audio.pcm_len,
				wavehdr[hdr]->lpData, wavehdr[hdr]->dwBytesRecorded);
			s_capture.audio.pcm_len += wavehdr[hdr]->dwBytesRecorded;
			ret = waveInAddBuffer(wavein, wavehdr[hdr], sizeof(WAVEHDR));            
			if (ret != MMSYSERR_NOERROR)
			{
				PRINTLOG(LOG_ERROR, ">>%s %s %d waveInAddBuffer failed ret = %d\n",
					__FILE__, __FUNCTION__, __LINE__, ret);
				return WAVEINADDBUFFERFAILED;
			}
			hdr = (hdr+1)%HDRCOUNT;
		}

		EnterCriticalSection(&s_capture.cs[AUDIO_INDEX]);
		enqueue(&s_capture.head[AUDIO_INDEX], s_capture.audio.pcm, s_capture.audio.pcm_len);
		s_capture.audio.pcm_len = 0;
		LeaveCriticalSection(&s_capture.cs[AUDIO_INDEX]);
	}

	ret = waveInStop(wavein);
	if (ret != MMSYSERR_NOERROR) 
	{
		return WAVEINSTOPFAILED;
	}

	ret = waveInReset(wavein);
	if (ret != MMSYSERR_NOERROR)
	{
		return WAVEINRESETFAILED;
	}

	for (i = 0; i < HDRCOUNT; i++)
	{
		ret = waveInUnprepareHeader(wavein, wavehdr[i], sizeof(WAVEHDR));
		if (ret != MMSYSERR_NOERROR)
		{
			ret = -8;
			return ret;
		}
		free(wavehdr[i]);
		PRINTLOG(LOG_DEBUG, "wavehdr[%d] free\n", i);
	}

	free(s_capture.audio.pcm);
	PRINTLOG(LOG_DEBUG, "audio.pcm free\n");

	ret = waveInClose(wavein);
	if (ret != MMSYSERR_NOERROR)
	{
		return WAVEINCLOSEFAILED;
	}
	PRINTLOG(LOG_DEBUG, "<<%s %s %d\n",__FILE__, __FUNCTION__, __LINE__);
	return SECCESS;
}

int InitCapture(PCAPTURECONFIG pCaptureConfig)
{
	int ret;
	PRINTLOG(LOG_DEBUG, ">>%s %s %d\n",__FILE__, __FUNCTION__, __LINE__);

	if (1 == s_capture.initialized)
	{
		return INITED;
	}

	if (0 >= pCaptureConfig->fps ||
		0 >= pCaptureConfig->channels ||
		0 >= pCaptureConfig->bits_per_sample ||
		0 >= pCaptureConfig->samples_per_sec ||
		0 >= pCaptureConfig->avg_bytes_per_sec)
	{
		return WRONG_PARAM;
	}

	GetScreenInfo(&s_capture.screen);
	ret = InitCaptureVideoParam(&s_capture.screen, &s_capture.video, pCaptureConfig);
	if (ret != INIT_SECCESS)
	{
		return ret;
	}

	InitCaptureAudioParam(&s_capture.audio, pCaptureConfig);

	s_capture.initialized = 1;
	PRINTLOG(LOG_DEBUG, "<<%s %s %d\n",__FILE__, __FUNCTION__, __LINE__);
	return SECCESS;
}

int StartCapture(PCAPTURECONFIG pCaptureConfig)
{
	int ret = 0;
	int i = 0;

	PRINTLOG(LOG_DEBUG, ">>%s %s %d\n",__FILE__, __FUNCTION__, __LINE__);

	if (s_capture.initialized != 1)
	{
		PRINTLOG(LOG_ERROR, "capture have no init\n");
		return NOINIT;
	}

	s_capture.stop = 0;


	for (i=0; i<ARRAY_LEN; i++)
	{
		INIT_LIST_HEAD(&s_capture.head[i]);
		InitializeCriticalSection(&s_capture.cs[i]);
		if (VIDEO_INDEX == i)
			s_capture.handler[i] = (HANDLE)_beginthreadex(NULL, 0, VideoCaptureProc, NULL, 0, NULL);
		else if (AUDIO_INDEX == i)
			s_capture.handler[i] = (HANDLE)_beginthreadex(NULL, 0, AudioCaptureProc, NULL, 0, NULL);

		if (NULL == s_capture.handler[i])
		{
			PRINTLOG(LOG_ERROR, "start thread failed\n");
			return STARTTHREADFAILED;
		}
	}

	s_capture.started = 1;
	PRINTLOG(LOG_DEBUG, "<<%s %s %d\n",__FILE__, __FUNCTION__, __LINE__);

	return SECCESS;
}

//����:��ȡһ֡�ɼ���Ƶ����
int GetVideoFrame(void **data, unsigned long *size, int *width, int *hetgit)
{
	int ret = 0;
	if (NULL == data || NULL == size)
	{
		return WRONG_PARAM;
	}

	if (1 != s_capture.initialized)
	{
		return NOINIT;
	}

	if (1 != s_capture.started)
	{
		return NOSTART;
	}

	EnterCriticalSection(&s_capture.cs[VIDEO_INDEX]);
	ret = dequeue(&s_capture.head[VIDEO_INDEX], (uint8_t **)data, size);
	LeaveCriticalSection(&s_capture.cs[VIDEO_INDEX]);
	if (ret != 0)
		return DEQUEUEFAILED;
	*width = s_capture.screen.width;
	*hetgit = s_capture.screen.height;

	return SECCESS;
}

//����:��ȡһ֡�ɼ���Ƶ����
int GetAudioFrame(void **data, unsigned long *size)
{
	int ret = 0;
	if (NULL == data || NULL == size )
	{
		return WRONG_PARAM;
	}

	if (1 != s_capture.initialized)
	{
		return NOINIT;
	}

	if (1 != s_capture.started)
	{
		return NOSTART;
	}

	EnterCriticalSection(&s_capture.cs[AUDIO_INDEX]);
	ret = dequeue(&s_capture.head[AUDIO_INDEX], (uint8_t **)data, size);
	LeaveCriticalSection(&s_capture.cs[AUDIO_INDEX]);
	if (ret != 0)
		return DEQUEUEFAILED;
	return SECCESS;
}

//����:ֹͣ�ɼ�
int StopCapture()
{
	int i = 0;

	PRINTLOG(LOG_DEBUG, ">>%s %s %d\n",__FILE__, __FUNCTION__, __LINE__);

	if (s_capture.initialized != 1)
	{
		PRINTLOG(LOG_ERROR, "capture have no init\n");
		return NOINIT;
	}

	if (1 != s_capture.started)
	{
		PRINTLOG(LOG_ERROR, "capture have no start\n");
		return NOSTART;
	}

	s_capture.stop = 1;

	for (i=0; i<ARRAY_LEN; i++)
	{
		WaitForSingleObject(s_capture.handler[i],INFINITE);
		s_capture.handler[i] = NULL;
	}

	s_capture.started = 0;
	PRINTLOG(LOG_DEBUG, "<<%s %s %d\n",__FILE__, __FUNCTION__, __LINE__);

	return SECCESS;
}

//�ͷŲɼ�����Ҫ����Դ
int FreeCapture()
{
	int ret = 0, i = 0;
	PRINTLOG(LOG_DEBUG, ">>%s %s %d\n",__FILE__, __FUNCTION__, __LINE__);

	if (s_capture.initialized != 1)
	{
		PRINTLOG(LOG_ERROR, "capture have no init\n");
		return NOINIT;
	}


	for (i=0; i<ARRAY_LEN; i++)
	{
		struct list_head *plist = NULL, *n = NULL;
		if (NULL != s_capture.handler[i])
		{
			return NOSTOP;
		}

		EnterCriticalSection(&s_capture.cs[i]);

		list_for_each_safe(plist, n, &s_capture.head[i]) 
		{
			NODE *node = list_entry(plist, struct node, list);
			list_del(plist);
			free(node->data);
			free(node);
			break;
		}

		LeaveCriticalSection(&s_capture.cs[i]);
		DeleteCriticalSection(&s_capture.cs[i]);
	}

	s_capture.initialized = 0;
	PRINTLOG(LOG_DEBUG, "<<%s %s %d\n",__FILE__, __FUNCTION__, __LINE__);

	return SECCESS;
}
