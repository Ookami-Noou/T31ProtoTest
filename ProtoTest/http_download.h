#ifndef PROTO_HTTP_DOWNLOAD_H
#define PROTO_HTTP_DOWNLOAD_H

#include "mongoose.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "util.h"

/** Main entry point */
extern int http_download_main(int argc, char* argv[]);

#endif // PROTO_HTTP_DOWNLOAD_H