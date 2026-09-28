#ifndef CONFIG_FILE_H
#define CONFIG_FILE_H

#include "config.h"
#include "error_code.h"

ErrorCode config_file_load(Config* config, const char* path, int* error_line);

#endif //CONFIG_FILE_H
