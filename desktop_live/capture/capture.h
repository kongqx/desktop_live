#ifndef __CAPTURE_H__
#define __CAPTURE_H__

#include "log.h"

#define BUILDING_DLL 1

#if BUILDING_DLL
# define DLLIMPORT __declspec (dllexport)
#else
# define DLLIMPORT __declspec (dllimport)
#endif

typedef unsigned char uint8_t;

#ifdef __cplusplus
extern "C"
{
#endif

//������־ָ�롢�����ļ�
//����0=�ɹ� ����ʧ��
//��ʼ�ɼ�
DLLIMPORT int start_capture(LOG *log, char *config_file);

//�ĸ�������������������������ݵ�ָ��ĵ�ַ���������ݳ��ȵĵ�ַ����ȵĵ�ַ���߶ȵĵ�ַ
//0=�ɹ� ����=ʧ��,����ִ�гɹ���Ҫ�ͷ�data
//��ȡһ֡video
DLLIMPORT int get_video_frame(void **data, unsigned long *size, int *width, int *hetgit);

//�������ݵ�ָ��ĵ�ַ���������ݳ��ȵĵ�ַ
//0=�ɹ� ����=ʧ��
//��ȡһ֡audio
DLLIMPORT int get_audio_frame(void **data, unsigned long *size);

//ֹͣ�ɼ�
DLLIMPORT int stop_capture();

//�ͷŲɼ�����Ҫ����Դ
DLLIMPORT int free_capture();

#ifdef __cplusplus
};
#endif

#endif //__CAPTURE_H__