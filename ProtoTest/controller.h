#pragma once
#ifndef PROGRESS_H
#define PROGRESS_H
#include <stdio.h>

#define MQTT_MAIN 1

#define HTTP_DOWNLOAD_MAIN 2
#define HTTP_UPLOAD_MAIN 3

// 切换入口。如果是 MQTT_MAIN，则使用 MQTT 入口函数；如果是 HTTP_MAIN，则使用 HTTP 入口函数。
#define ENTRY	MQTT_MAIN

#ifdef null
#undef null
#endif
#define null ((void*) 0)

#endif // !PROGRESS_H