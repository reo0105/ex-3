/*61910902 須永一輝*/
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>
#include <string.h>
#include "shell.h"
#include "history.h"

int shell(char *, struct hist *, int *, FILE *);

 
int main()
{
    int token, line_file = 0;
    char exec_path[PATH_MAX], current_path[PATH_MAX], *hist_file = "myshell_history";
    struct hist *tail;
    FILE *history;
    
    /*カレントディレクトリを取得*/
    if (getcwd(current_path, PATH_MAX) == NULL) {
        perror("getcwd()");
        exit(EXIT_FAILURE);
    }
    strcpy(exec_path, current_path);

    /*histroyファイルが存在しなければ作成*/
    if (check_file_exists(hist_file)) {
        file_fopen(history, hist_file, "r", EXIT_FAILURE);
        /*tailをマロック*/
        mem_alloc(tail, struct hist, 1, EXIT_FAILURE);
        tail->fp = tail->bp = tail;
        /*この中で履歴分のマロック*/
        initialize_hist(history, tail, &line_file);
    } else {
        mem_alloc(tail, struct hist, 1, EXIT_FAILURE);
        tail->fp = tail->bp = tail;
    }
    
    /*プロンプト表示*/
    printf("Welcome to myshell!\n");
    strcat(current_path, "$ ");

    /*コマンドを受付*/
    while(1) {
        printf("%s", current_path);
        token = shell(current_path, tail, &line_file, history);
        if (token == TKN_EOF) {
            break;
        } else if (token == TKN_EOL) {
            continue;
        }
    }

    /*historyファイルの保存*/
    strcat(exec_path, "/");
    strcat(exec_path, hist_file);
    file_fopen(history, exec_path, "w", EXIT_FAILURE);
    /*ここで履歴の書き込みとhistory関連のfree*/
    write_hist(history, tail);

    return 0;
}