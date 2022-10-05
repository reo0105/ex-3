#include <stdio.h>
#include "shell.h"

#define MAX_HIST 100

struct hist {
    char cmd[CMD_LEN];
    struct hist *fp;
    struct hist *bp;
};

int check_file_exists(char *);
void initialize_hist(FILE *, struct hist *, int *);
void insert_tail(struct hist *, struct hist *);
int chech_head(struct hist *, struct hist *);
void remove_head(struct hist *);
void write_hist(FILE *, struct hist *);
void create_strcmd(char *argv[MAX_ARGS], int tkn[MAX_TKN], char *, int);
void disp_history(struct hist *);