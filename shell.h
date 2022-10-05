#ifndef SHELLDEF
    #define SHELLDEF
#define TKN_NULL         0x00000000
#define TKN_STR          0x00000001
#define TKN_REDIR_IN     0x00000002
#define TKN_REDIR_OUT    0x00000004
#define TKN_REDIR_APPEND 0x00000008
#define TKN_PIPE         0x00000010
#define TKN_BG           0x00000020
#define TKN_EOL          0x00000040
#define TKN_EOF          0x00000080
#define TKN_UP_ARROW     0x00000100
#define TKN_DOWN_ARROW   0x00000200
#define TKN_LEFT_ARROW   0x00000400
#define TKN_RIGHT_ARROW  0x00000800
#define TKN_AND          0x00001000
#define TKN_OR           0x00002000 
#define TKN_BACK_SPACE   0x00004000

#define PARENTHESES 91


#define FILE_LEN 256
#define CMD_LEN 256
#define MAX_ARGS 32
#define CMD_UNIT_LEN 32
#define MAX_TKN 16


#define mem_alloc(ptr, type, size, errno)                                           \
do {                                                                                \
    if ((ptr = (type *)malloc(sizeof(type) * (size))) == NULL) {                    \
        fprintf(stderr, "Cannot allocate %ldbyte memory\n", sizeof(type) * size);   \
    }                                                                               \
} while(0)                                                                          \

#define file_open(fd, fname, mode, permission, errno) \
do {                                                  \
    if ((fd = open(fname, mode, permission)) < 0) {   \
        perror("open()");                             \
        errno = -1;                                   \
    }                                                 \
} while(0)                                            \

#define file_fopen(fp, fname, mode, errno)     \
do {                                           \
    if ((fp = fopen(fname, mode)) == NULL) {   \
        perror("fopen()");                     \
    }                                          \
} while(0)                                     \

#endif