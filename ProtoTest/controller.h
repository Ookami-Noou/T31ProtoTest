#pragma once
#ifndef PROGRESS_H
#define PROGRESS_H
#include <stdio.h>

#define MQTT_MAIN 1

#define HTTP_DOWNLOAD_MAIN 2
#define HTTP_UPLOAD_MAIN 3

// �л���ڡ������ MQTT_MAIN����ʹ�� MQTT ��ں���������� HTTP_MAIN����ʹ�� HTTP ��ں�����
#define ENTRY	MQTT_MAIN

#ifdef null
#undef null
#endif
#define null ((void*) 0)

#endif // !PROGRESS_H