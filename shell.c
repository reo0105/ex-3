#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <sysexits.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <termios.h>
#include "shell.h"
#include "history.h"
#include "gettoken.h"


extern char **environ;


/*sig_childハンドラ*/
void sig_child_handler(int sig)             
{
    (void) sig;

    pid_t pid;
    while ((pid = waitpid(-1, NULL, WNOHANG)) != 0) {
        if (pid == -1) {
            if (errno == ECHILD) {
                break;
            } else if (errno == EINTR) {
                continue;
            }
            perror("waitpid()");
            break;
        }
    }
}


/*シグナルのセット*/
void signal_set()
{
    struct sigaction sigact;

    sigemptyset(&sigact.sa_mask);

    sigact.sa_handler = SIG_IGN;
    sigact.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sigact, NULL);

    sigact.sa_handler = sig_child_handler;
    sigact.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &sigact, NULL);

    sigact.sa_handler = SIG_IGN;
    sigact.sa_flags = SA_RESTART;
    sigaction(SIGTTOU, &sigact, NULL);

    sigact.sa_handler = SIG_IGN;
    sigact.sa_flags = SA_RESTART;
    sigaction(SIGTSTP, &sigact, NULL);
}


/*シグナルのリセット*/
void reset_signal_handlers()
{
    struct sigaction sigact;

    memset(&sigact, 0, sizeof(sigact));

    sigemptyset(&sigact.sa_mask);
    sigact.sa_handler = SIG_DFL;
    sigact.sa_flags = 0;

    sigaction(SIGINT, &sigact, NULL);
    sigaction(SIGTTOU, &sigact, NULL);
    sigaction(SIGCHLD, &sigact, NULL);
    sigaction(SIGTSTP, &sigact, NULL);
}


/*execve用のパスの作成*/
void search_path(char *path, char *cmd)
{
    int i, exist;
    char search[PATH_MAX], *env_path;
    struct stat buf;
    
    /*PATH以外の実行ファイル*/
    if (cmd[0] == '.') {
        if (getcwd(path, PATH_MAX) == NULL) {
            perror("getcwd() in search_path()");
        } else {
            strcat(path, "/");
            strcat(path, cmd);
        }
        return;
    }

    /*PATHを取得*/
    env_path = getenv("PATH");

    /*PATHを探索*/
    for (; *env_path != '\0';) {
        for (i = 0; *env_path != ':' && *env_path != '\0'; i++, env_path++) {
            search[i] = *env_path;
        }

        strcat(search, "/");
        strcat(search, cmd);

        exist = stat(search, &buf);

        /*コマンドが存在し通常ファイルならpathに設定*/
        if  (exist == 0 && S_ISREG(buf.st_mode)) {
            strcpy(path, search);
            //free(env_path);
            return;
        }

        if (*env_path == ':') {
            memset(search, '\0', PATH_MAX);
            env_path++;
        }
    }
}


/*dup2*/
int duplicate(int olfd, int newfd)
{
    if (olfd == newfd) {
        return 0;
    }

    if (dup2(olfd, newfd) == -1) {
        perror("dup2()");
        close(olfd);
        return -1;       
    } 
    
    if (close(olfd) == -1) {
        perror("close()");
        return -1;
    }

    return 0;
}


/*ビルトインコマンド(cd, pwd, exit)の判定 (history)は除く*/
int is_builtin(char *cmd)
{
    if (strcmp(cmd, "cd") == 0 || strcmp(cmd, "pwd") == 0 || strcmp(cmd, "exit") == 0) {
        return 1;
    } else {
        return 0;
    }
}


/*上下矢印を入力した際の現在表示されているコマンドの削除*/
void remove_cmd_disp(int cur_pos, int max_pos, int path_len)
{
    int i;

    for (i = 0; i < max_pos - cur_pos; i++) {
        printf("\x1b[1C");
    }

    for (i = 0; i < max_pos - path_len; i++) {
        printf("\b \b");
    }
} 


int shell(char *current_path, struct hist *tail, int *line, FILE *history)
{
    int argc, bg_flag, token, tkn[MAX_TKN], status, i, j, cnt, loop, err, builtin, quit, hi, c, cmd_len, path_len;
    int fd_infile, fd_outfile, pfd[2], fd_tty, fd_stdin, fd_stdout, cur_pos, max_pos;
    int change_mode, files_free, chang_fg;
    char *argv[MAX_ARGS], *infile, *outfile, *path, **cmd, strcmd[CMD_LEN], hist_cmd[CMD_LEN];
    pid_t cpid, cgrppid, wait_pid, pfgpgid;
    struct hist *p, *hist_arrow;
    struct termios io_conf;
    
    /*変数の初期化*/
    argc = 0;
    bg_flag = 0;
    token = 0;
    status = 0;
    cnt = 0;
    err = 0;
    hi = 0;
    quit = -1;
    cpid = -1;
    files_free = 0;
    change_mode = 0;
    chang_fg = 0;
    cur_pos = strlen(current_path);
    max_pos = cur_pos;
    path_len = cur_pos;
    memset(hist_cmd, 0, CMD_LEN);

    /*シグナルのセット*/
    signal_set();

    /*標準入力と標準出力の獲得*/
    if ((fd_stdin = dup(STDIN_FILENO)) == -1) {
        perror("dup()");
        exit(EXIT_FAILURE);
    }
    if ((fd_stdout = dup(STDOUT_FILENO)) == -1) {
        perror("dup()");
        exit(EXIT_FAILURE);
    }

    /*制御端末のフォアグランドプロセスIDの獲得*/
    if ((fd_tty = open("/dev/tty", O_RDWR)) == -1) {
        perror("fd_tty");
        exit(EXIT_FAILURE);
    }

    if((pfgpgid = tcgetpgrp(fd_tty)) == -1) {
        perror("tcgetgrp");
        exit(EXIT_FAILURE);
    }
    
    /*argvをマロック*/
    for (i = 0; i < MAX_ARGS; i++) {
        mem_alloc(argv[i], char, CMD_UNIT_LEN, EXIT_FAILURE);
    }

    /*struct histの設定*/
    hist_arrow = tail;
    hist_arrow->cmd[0] = '\0';
    
    /*非カノニカルモードの設定*/
    memset(&io_conf, 0x00, sizeof(struct termios));
    tcgetattr(0, &io_conf);
    io_conf.c_lflag &= ~(ECHO);
    io_conf.c_lflag &= ~(ICANON);
    io_conf.c_cc[VMIN] = 1;
    io_conf.c_cc[VTIME] = 0;
    tcsetattr(0, TCSANOW, &io_conf);
    change_mode = 1;

    /*構文解析*/
    while ((token = getargv(&argc, argv, tkn, &cnt, &bg_flag, current_path, &cur_pos, &max_pos, hist_cmd))) {
        if (token == TKN_EOL) {
            if (argc == 1) {
                return TKN_EOL;
            } else {
                break;
            }
        } else if (token == TKN_EOF) {
	        printf("Quit\n");
            /*カノニカルモードの設定*/
	        memset(&io_conf, 0x00, sizeof(struct termios));
	        tcgetattr(0, &io_conf);
	        io_conf.c_lflag |= ECHO;
	        io_conf.c_lflag |= ICANON;
	        io_conf.c_cc[VMIN] = 1;
	        io_conf.c_cc[VTIME] = 0;
	        tcsetattr(0, TCSANOW, &io_conf);
            
            /*argvの開放*/
            for (i = 0; i < MAX_ARGS; i++) {
                free(argv[i]);
            }
	        return TKN_EOF;
        } else if (token == TKN_NULL) {
            break;
        } else if (token == TKN_UP_ARROW || token == TKN_DOWN_ARROW) {
            /*画面に表示してcontinue*/
            if (max_pos != path_len) {
                remove_cmd_disp(cur_pos, max_pos, path_len);
            } 
            
            if (token == TKN_UP_ARROW && hist_arrow->bp != tail) {
                hist_arrow = hist_arrow->bp;
            } else if (token == TKN_DOWN_ARROW && hist_arrow->fp != tail) {
                hist_arrow = hist_arrow->fp;
            } 

            strcpy(hist_cmd, hist_arrow->cmd);
            printf("%s", hist_arrow->cmd);
            cmd_len = strlen(hist_arrow->cmd);
            cur_pos = cmd_len + path_len;
            max_pos = cur_pos;
	        if ((c = getchar()) == '\n') {
	            if (pipe(pfd) == -1){
	                perror("pipe()");
                    err = -1;
                    goto END;
	            }
    	        if (duplicate(pfd[1], 1) == -1) {
                    err = -1;
                    goto END;
                }
	            printf("\n");
    	        if (duplicate(pfd[0], 0) == -1) {
                    err = -1;
                    goto END;
                }
	            hi = 1;
	            if (duplicate(fd_stdout, STDOUT_FILENO) == -1) {
                    err = -1;
                    goto END;
                }
	        }
	        ungetc(c, stdin);
        }
    }

    /*矢印で履歴を表示した際の後処理*/
    if (hi) {
        duplicate(fd_stdin, STDIN_FILENO);
        if ((fd_stdin = dup(STDIN_FILENO)) == -1) {
            perror("dup()");
            err = -1;
            goto END;
        }

        if ((fd_stdout = dup(STDOUT_FILENO)) == -1) {
            perror("dup()");
            err = -1;
            goto END;
        }
    }
    
    /*カノニカルモードの設定*/
    memset(&io_conf, 0x00, sizeof(struct termios));
    tcgetattr(0, &io_conf);
    io_conf.c_lflag |= ECHO;
    io_conf.c_lflag |= ICANON;
    io_conf.c_cc[VMIN] = 1;
    io_conf.c_cc[VTIME] = 0;
    tcsetattr(0, TCSANOW, &io_conf);
    change_mode = 0;

    /*入力からhist用のコマンド作成*/
    mem_alloc(p, struct hist, 1, EXIT_FAILURE);
    create_strcmd(argv, tkn, strcmd, argc);
    strcpy(p->cmd, strcmd);
    if (*line == MAX_HIST) {
        remove_head(tail);
        insert_tail(tail, p);
    } else {
        line++;
        insert_tail(tail, p);
    }

    /*execの引数用のポインタの初期化*/
    cmd = &argv[0];
    // printf("argc = %d\n", argc);
    // for (hi = 0; hi < 32; hi++) {
    //   printf("argv[%d] = %s\n", hi, argv[hi]);
    // }

    /*リダイレクト用ファイルとコマンドパスのマロック*/
    mem_alloc(infile, char, FILE_LEN, EXIT_FAILURE);
    mem_alloc(outfile, char, FILE_LEN, EXIT_FAILURE);
    mem_alloc(path, char, PATH_MAX, EXIT_FAILURE);
    files_free = 1;

    /*初期化*/
    infile[0] = '\0';
    outfile[0] = '\0';

    /*コマンドを解析していく*/
    for (i = 0, j = 0, loop = 0; i < argc; i++, j++, cmd = &argv[i]) {

        while (argv[i] != NULL){
            i++;
            continue;
        }
        /*入出力ファイルがあれば*/
        if (tkn[j] & (TKN_REDIR_IN | TKN_REDIR_OUT | TKN_REDIR_APPEND)) {
            /*tokenがパイプかEOLまで実行*/
            while ((tkn[j] & (TKN_PIPE | TKN_EOL | TKN_AND | TKN_OR)) == 0) {
                while (argv[i] != NULL) {
                    i++;
                }
                /*token応じてリダイレクトの設定*/
                switch (tkn[j++]) {
                    case TKN_REDIR_IN:
                        file_open(fd_infile, argv[++i], O_RDONLY, 0644, err);
                        if (err == -1) {
                            break;
                        } else if (duplicate(fd_infile, 0) == -1) {
                            err = -1;
                        }
                        break;
                    case TKN_REDIR_OUT:
                        file_open(fd_outfile, argv[++i], O_WRONLY | O_CREAT | O_TRUNC, 0644, err);
                        if (err == -1) {
                            break;
                        } else if (duplicate(fd_outfile, 1) == -1) {
                            err = -1;
                        }
                        break;
                    case TKN_REDIR_APPEND:
                        file_open(fd_outfile, argv[++i], O_WRONLY | O_APPEND | O_CREAT, 0644, err);
                        if (err == -1) {
                            break;
                        } else if (duplicate(fd_infile, 1) == -1) {
                            err = -1;
                        }
                        break;
                    default: break;
                }
            }
        }

        /*openかdupでエラー。perrorでどちらによるものかは判明*/
        if (err == -1) {
            goto END;
        }

        while (argv[i] != NULL) {
            i++;
        }

        /*パイプ生成*/
        if (tkn[j] == TKN_PIPE) {
            if (pipe(pfd) == -1) {
                perror("pipe()");
                err = -1;
                goto END;
            }
        }

        /*fork*/
        if (((builtin = is_builtin(cmd[0])) == 0) && ((cpid = fork()) < 0)) {
            perror("fork");
            err = -1;
            goto END;
        } else if (cpid == 0) {
            /*子プロセス*/
            /*tokenがパイプのとき標準出力を変更*/
            if (tkn[j] == TKN_PIPE) {
                /*stdoutの変更*/
                if (duplicate(pfd[1], 1) == -1) {
                    close(pfd[0]);
                    break;
                }
                close(pfd[0]);
            }

            /*シグナルハンドラのリセット*/
            reset_signal_handlers();
        
            /*history*/
            if (strcmp(cmd[0], "history") == 0) {
                disp_history(tail);
                exit(EXIT_SUCCESS);
            } else {
                /*コマンドのパスの取得*/
                search_path(path, cmd[0]);
                /*コマンドの実行*/
                printf("%s", path);
                execve(path, cmd, environ);
            }
            
            /*execが成功するとプロセスが実行しているプログラムを変更*/
            /*execがエラーの場合*/
            fprintf(stderr, "%s: コマンドが見つかりません\n", cmd[0]);
            exit(EX_USAGE);
        } else {
            /*親プロセスの処理*/
            /*ビルトインコマンドのチェックと実行*/
            if (builtin) {
                /*cdコマンド*/
                if (strcmp(cmd[0], "cd") == 0) {
                    if (cmd[1] == NULL) {
                        chdir(getenv("HOME"));
                    } else if (chdir(cmd[1]) == -1) {
                        perror("chidir()");
                        err = -1;
                        goto END;
                    }

                    if (getcwd(current_path, PATH_MAX) == NULL) {
                        perror("getcwd() in cdcmd");
                        err = -1;
                        goto END;
                    }

                    strcat(current_path, "$ ");

                /*pwdコマンド*/
                } else if (strcmp(cmd[0], "pwd") == 0) {
                    if (getcwd(current_path, PATH_MAX) == NULL) {
                        perror("getcwd()");
                        err = -1;
                        goto END;
                    } else {
                        printf("%s\n", current_path);
                    }

                /*exit*/
                }else if ((quit = strcmp(cmd[0], "exit")) == 0) {
                    break;
                }
                continue;
            }

            /*子プロセスのプロセスグループIDの獲得*/
            if (bg_flag == 0) {
                cgrppid = cpid;
            } else {
                cgrppid = (loop++ == 0) ? cpid : cgrppid;
            }

            /*子プロセスグループIDの設定*/
            if (setpgid(cpid, cgrppid) == -1) {
                perror("setpgid");
                err = -1;
                goto END;
            }
        
            /*バックグラウンド実行でない場合、フォアグランドプロセスグループIDの設定*/
            if (bg_flag == 0) {
                if (tcsetpgrp(fd_tty, cgrppid) == -1) {
                    perror("tcsetpgrp()");
                    err = -1;
                    goto END;
                }
                chang_fg = 1;
            }

            /*tokenがパイプなら標準入力の変更*/
            if (tkn[j] == TKN_PIPE) {
                if (duplicate(pfd[0], 0) == -1) {
                    close(pfd[1]);
                    err = -1;
                    goto END;
                }
                close(pfd[1]);
            } 

            /*バックグラウンド実行でなければ子プロセスを待つ*/
            if (bg_flag == 0) {    
                wait_pid = wait(&status);
                if (wait_pid == (pid_t) -1) {
                    perror("wait()");
                    err = -1;
                    goto END;
                }
	        }
        } 

        /*フォアグランドプロセスグループIDの設定*/
        if(tcsetpgrp(fd_tty, pfgpgid) == -1) {
            perror("tcsetpgrp");
            err = -1;
            goto END;
        }
        chang_fg = 0;

        /*終了状態によってコマンドを中断*/
	    if (tkn[j] == TKN_AND && WIFEXITED(status) && WEXITSTATUS(status) == EX_USAGE) {
            goto END;
        }

        if (tkn[j] == TKN_OR && WIFEXITED(status) && WEXITSTATUS(status) == EXIT_SUCCESS) {
            goto END;
        }
    }


END:
    /*終了処理*/
    /*argvの開放*/
    for (i = 0; i < MAX_ARGS; i++) {
        free(argv[i]);
    }

    if (change_mode) {
        /*カノニカルモードの設定*/
        memset(&io_conf, 0x00, sizeof(struct termios));
        tcgetattr(0, &io_conf);
        io_conf.c_lflag |= ECHO;
        io_conf.c_lflag |= ICANON;
        io_conf.c_cc[VMIN] = 1;
        io_conf.c_cc[VTIME] = 0;
        tcsetattr(0, TCSANOW, &io_conf);
    }

    if (files_free) {
        free(infile);
        free(outfile);
        free(path);   
    }
    /*pはwrite_histで開放*/

    /*フォアグランドプロセスグループIDを復旧*/
    if (chang_fg) {
        if(tcsetpgrp(fd_tty, pfgpgid) == -1) {
            perror("tcsetpgrp");
        }
    }

    /*制御端末のファイル記述子を閉じる*/
    if (fd_tty != -1) {
        if (close(fd_tty) == -1) {
            perror("close");
        }
    }

    /*標準入出力をもとに戻す*/
    duplicate(fd_stdin, STDIN_FILENO);
    duplicate(fd_stdout, STDOUT_FILENO);

    if (err == -1) {
        write_hist(history, tail);
        exit(EXIT_FAILURE);
    }

    if (quit == 0) {
        return TKN_EOF;
    }

    return TKN_EOL;
}