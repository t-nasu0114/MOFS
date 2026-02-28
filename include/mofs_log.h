#ifndef __MOFS_LOG__
#define __MOFS_LOG__

#include <stdio.h>

/* Ubuntu Only */
#define CLR_RESET  "\033[0m"
#define CLR_RED    "\033[31m"
#define CLR_YELLOW "\033[33m"
#define CLR_CYAN   "\033[36m"

/* Ubuntu */
#define MOFS_DBG(fmt, ...) printf(CLR_CYAN "[DBG] " fmt CLR_RESET "", ##__VA_ARGS__)
#define MOFS_INF(fmt, ...) printf("[INF] " fmt "", ##__VA_ARGS__)
#define MOFS_WRN(fmt, ...) fprintf(stderr, CLR_YELLOW "[WRN] " fmt CLR_RESET "", ##__VA_ARGS__)
#define MOFS_ERR(fmt, ...) fprintf(stderr, CLR_RED "[ERR] " fmt CLR_RESET "", ##__VA_ARGS__)

#endif /* __MOFS_LOG__ */
