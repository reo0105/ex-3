#include "history.h"
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>


/*ファイルの存在確認*/
int check_file_exists(char *filename)
{
    struct stat buffer;
    int exist = stat(filename, &buffer);
    
    if (exist == 0) {
        return 1;
    } else {
        return 0;
    }
}


/*historyファイルから履歴の読み込み*/
void initialize_hist(FILE *history, struct hist *tail, int *line)
{
  int i = 0, j = 0;
    char pre_cmd[CMD_LEN];
    struct hist *hist[MAX_HIST];

    while (fgets(pre_cmd, CMD_LEN, history) != NULL) {
        if ((hist[i] = (struct hist *)malloc(sizeof(struct hist))) == NULL) {
            fprintf(stderr, "Cannot allocate memory\n");
            fclose(history);
            exit(EXIT_FAILURE);
        }
        (*line)++;
	    for (j = 0; pre_cmd[j] != '\n'; j++);
	    pre_cmd[j] = '\0';
        strcpy(hist[i]->cmd, pre_cmd);
        insert_tail(tail, hist[i++]);
    }

    fclose(history);
}


void insert_tail(struct hist *tail, struct hist *p)
{
    p->bp = tail->bp;
    p->fp = tail;
    tail->bp->fp = p;
    tail->bp = p;
}


void remove_head(struct hist *tail)
{
    struct hist *p;

    p = tail->fp;
    tail->fp = p->fp;
    p->fp->bp = tail;
    free(p);
    p = NULL;
}


/*historyファイルへの書き込み*/
void write_hist(FILE *history, struct hist *tail)
{
    int len;
    struct hist *p;

    printf("write hist\n");
    for (p = tail->fp; p != tail; p = p->fp) {
        len = strlen(p->cmd);
        if (fwrite(p->cmd, 1, len, history) < (size_t)len) {
            perror("fwrite()");
        }
        fwrite("\n", 1, 1, history);
        free(p);
    }

    free(tail);
    fclose(history);
}


/*historyファイルへの書き込み用コマンドの作成*/
void create_strcmd(char *argv[MAX_ARGS], int tkn[MAX_TKN], char *strcmd, int argc)
{
    int i, j, count = 0;

    for(i = 0, j = 0; i < argc; i++) {
        if (count++ == 0) {
            strcpy(strcmd, argv[i]);
        } else {
            if (argv[i] == NULL) {
                switch (tkn[j++]) {
                    case TKN_REDIR_IN:
                        strcat(strcmd, " <");
                        break;
                    case TKN_REDIR_OUT:
                        strcat(strcmd, " >");
                        break;
                    case TKN_REDIR_APPEND:
                        strcat(strcmd, " >>");
                        break;
                    case TKN_PIPE:
                        strcat(strcmd, " |");
                        break;
                    case TKN_BG:
                        strcat(strcmd, " &");
                        break;
                    case TKN_AND:
                        strcat(strcmd, " &&");
                        break;
                    case TKN_OR:
                        strcat(strcmd, " ||");
                        break;
                }
            } else {
                strcat(strcmd, " ");
                strcat(strcmd, argv[i]);
            }
        }
    }
}


void disp_history(struct hist *tail)
{
    int i;
    struct hist *p;

    for (i = 0, p = tail->fp; p != tail; i++, p = p->fp) {
        printf("%3d: %s\n", i, p->cmd);
    }
}