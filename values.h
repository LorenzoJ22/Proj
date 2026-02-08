#ifndef CONFIG_H
#define CONFIG_H


#define GROUP_NAME "csap_group"
#define PATH_MAX        4096
#define BUFFER_SIZE 4096

#define COLOR_RED "\033[31m"
#define COLOR_RESET "\033[0m"
#define COLOR_GREEN "\033[32m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_BLUE "\033[34m"
#define COLOR_BLUE_BACK "\033[44m"
#define COLOR_CIANO "\033[36m"
#define COLOR_MAGENTA "\033[35m"



typedef enum {
    RESP_OK = 0,
    RESP_ERR_LOCKED,
    RESP_ERR_PATH,
    RESP_ERR_INVALID_NAME,
    RESP_ERR_OFFSET_UNDER,
    RESP_ERR_OFFSET_LONG,
    RESP_ERR_NO_OFFSET,
    RESP_USAGE,
    RESP_ERR_OPEN,
    RESP_ERR_VIO,
    RESP_ERR_GUEST,
    RESP_ERR_INVALID_FILE_N,
    RESP_ERR_INVALID_DOT,
    RESP_ERR_GENERIC
} ResponseStatus;




#endif