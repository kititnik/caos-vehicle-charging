#include <stdio.h>
#include <string.h>

#include "config_file.h"

static const char* config_error_message(ErrorCode error) {
    switch (error) {
        case ERR_FILE_OPEN:
            return "cannot open file";
        case ERR_FILE_READ:
            return "cannot read file";
        case ERR_OUT_OF_MEMORY:
            return "out of memory";
        case ERR_CONFIG_SYNTAX:
            return "syntax error";
        case ERR_CONFIG_VALUE:
            return "invalid value";
        case ERR_CONFIG_MISSING:
            return "missing required parameters";
        default:
            return "unknown error";
    }
}

int main(int argc, char** argv) {
    if (argc != 3 || strcmp(argv[1], "--config") != 0) {
        fprintf(stderr, "Usage: %s --config <file>\n", argv[0]);
        return 1;
    }
    const char* config_path = argv[2];

    Config config;
    int error_line;
    ErrorCode error = config_file_load(&config, config_path, &error_line);
    if (error != ERR_OK) {
        if (error_line > 0) {
            fprintf(stderr, "%s:%d: %s\n", config_path, error_line, config_error_message(error));
        } else {
            fprintf(stderr, "%s: %s\n", config_path, config_error_message(error));
        }
        return 1;
    }

    config_destroy(&config);
    return 0;
}
