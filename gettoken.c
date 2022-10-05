#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <termios.h>
#include "shell.h"


/*文字の削除*/
void delete_char(int *cur_pos, int *max_pos, int path_len, char *cmd)
{
    int i;

    (*cur_pos)--;

    for (i = *cur_pos - path_len; i < (*max_pos - path_len) && 0 <= i && i <= CMD_LEN - 1; i++) {
        cmd[i] = cmd[i+1];
    }

    (*max_pos)--;

}


/*画面の描画*/
void disp_clean(int *max_pos, int path_len, char *cmd, int *del_flag, int fact_cur)
{
    int i;

    /*カーソル位置の変更*/
    for (i = 0; i < fact_cur; i++) {
        printf("\x1b[1C");
    }

    if (*del_flag == 0) {
        /*文字削除が行われていない場合*/
	    if (fact_cur == *max_pos - path_len) {
            /*カーソル移動がされていた場合を考慮しfact_curまで*/
	    	for (i = 0; i < *max_pos - path_len; i++) {
	    		printf("\b \b");
	    	}
	    } else {
            for (i = 0; i < *max_pos-1 - path_len; i++) {
                printf("\b \b");
            }
       }
    } else {
        /*文字削除が行われていた場合*/
        for (i = 0; i < *max_pos+1 - path_len; i++) {
            printf("\b \b");
        }
        *del_flag = 0;
    }

    /*新しい文字配列を描画*/
    for (i = 0; i < *max_pos - path_len; i++) {
        putchar(cmd[i]);
    }

    /*元々のカーソル位置に戻す*/
    for (i = 0; i < fact_cur; i++) {
	    printf("\x1b[1D");
	}
} 


/*コマンド配列をずらす*/
void move_cmd_right(char *cmd, int n, int fact_cur)
{
    int i;
    
    for (i = 0; i < fact_cur; i++) {
        cmd[n + fact_cur - i] = cmd[n + fact_cur -i -1];
    }
}


/*非カノニカルモードで入力の変更をするための関数*/
int create_input(char *current_path, int *cur_pos, int *max_pos, char *cmd)
{
    int c, i, ctl_cnt, del_flag, fact_cur;

    /*初期化*/
    ctl_cnt = 0;
    del_flag = 0;
    fact_cur = 0;
    i = strlen(cmd);

    while(1) {
        /*入力の受付*/
        c  = getchar();
        /*空文字か否か*/
        if (isblank(c)) {
            if (cmd[i] != '\0') {
                /*実際のカーソル位置に空文字を入れ、以降を後ろにずらす*/
	            move_cmd_right(cmd, i, fact_cur);
	        }
	        cmd[i++] = ' ';
            (*cur_pos)++;
            (*max_pos)++;
        } else if (ctl_cnt != 0) {
            /*Escape入力が押下された次の入力の処理(矢印の処理)*/
            if (c == 'A') {
                /*上矢印*/
	            ctl_cnt = 0;
	            return TKN_UP_ARROW;
            } else if (c == 'B') {
                /*下矢印*/
	            ctl_cnt = 0;
	            return TKN_DOWN_ARROW;
            } else if (c == 'C') {
                /*右矢印*/
	            if (*max_pos > *cur_pos) {
                    /*画面のカーソルを一つ右へ*/
	                printf("\x1b[1C");
                    i++;
    	            (*cur_pos)++;
		            fact_cur--;
	            }
	            ctl_cnt = 0;
            } else if (c == 'D') {
                /*左矢印*/
	            if (*cur_pos > (int)strlen(current_path)) {
                    /*画面のカーソルを一つ左へ*/
	                printf("\x1b[1D");
                    i--;
	                (*cur_pos)--;
			        fact_cur++;
    	        }
	            ctl_cnt = 0;
            }
            continue;
        } else {
            /*入力の処理*/
            switch (c) {
                case 0x00:
                    break;
                case 0x1b:
                    /*Escapeの場合(矢印のとき)*/
                    ctl_cnt = 1;
                    continue;
                case 0x0a:
                    /*改行文字のとき*/
                    putchar('\n');
	        	    i += fact_cur;
                    cmd[i++] = c;
                    *cur_pos = strlen(current_path);
                    *max_pos = *cur_pos;
                    return TKN_EOL;
                case 0x7F:
                    /*Backspaceのとき*/
                    if (*cur_pos > (int)strlen(current_path)) {
                        /*カーソル位置の文字の削除*/
                        delete_char(cur_pos, max_pos, strlen(current_path), cmd);
                        i--;
                        del_flag = 1;
                    }
                    break;
                case 0x04:
                    /*ctrl Dのとき*/
                    return TKN_EOF;
                default:
                    /*矢印でカーソル移動し途中に追加する場合*/
                    if (cmd[i] != '\0') {
                        /*それ以降を一つ後ろにずらす*/
			            move_cmd_right(cmd, i, fact_cur);
		            }
		            cmd[i++] = c;
                    (*cur_pos)++;
                    (*max_pos)++;
                    break;
            }
        }
        /*画面出力をclearしてから新たな入力を反映し再表示*/
        disp_clean(max_pos, strlen(current_path), cmd, &del_flag, fact_cur);
    }

}

int gettoken(char *token, char *current_path, int *cur_pos, int *max_pos, int *flag, char *cmd, int *cmd_i)
{
/*
    1文字ずつ入力を解析
    getargvから呼ばれる
*/
    int c, len = 0, ret_token;

    /*1回目の実行か否か*/
    if (*flag) {
	    *flag = 0;
        if (*cmd_i == 0) {
            /*通常の入力の場合*/
            memset(cmd, 0, CMD_LEN);
                /*非カノニカルモードのためコマンドを一度配列で管理*/
                if ((ret_token = create_input(current_path, cur_pos, max_pos, cmd)) == TKN_UP_ARROW){
                    return TKN_UP_ARROW;
                } else if (ret_token == TKN_DOWN_ARROW) {
                    return TKN_DOWN_ARROW;
                } else if (ret_token == TKN_EOF) {
                    return TKN_EOF;
                }
        } else {
            /*矢印入力(履歴の実行・変更の場合)*/
            if ((c = getchar()) == '\n') {
                /*Enterが押下されたらコマンドの解析へ*/
                cmd[*cmd_i] = '\n';
                putchar('\n');
            } else {
                /*それ以外なら履歴が入った配列の変更へ*/
                ungetc(c, stdin);
                if ((ret_token = create_input(current_path, cur_pos, max_pos, cmd)) == TKN_UP_ARROW) {
                    return TKN_UP_ARROW;
                }  else if (ret_token == TKN_DOWN_ARROW) {
                    return TKN_DOWN_ARROW;
                } else if (ret_token == TKN_EOF) {
                    return TKN_EOF;
                }
            }
            *cmd_i = 0;
        }
    }
    
    /*コマンドの配列を解析*/
    while(cmd[*cmd_i] != '\0') {

        while(isblank(c = cmd[(*cmd_i)++]));

        /*cが英数字以外のTOKEN*/
        switch (c) {
            case '|':
                if ((c = cmd[(*cmd_i)++]) == '|') {
                    return TKN_OR;
                } else {
                    (*cmd_i)--;
                    return TKN_PIPE;
                }
            case '&':
                if ((c = cmd[(*cmd_i)++]) == '&') {
                    return TKN_AND;
                } else {
                    (*cmd_i)--;
                    return TKN_BG;
                }
            case '\n': return TKN_EOL;
            case '<': return TKN_REDIR_IN;
            case '>':
                if ((c = cmd[(*cmd_i)++]) == '>') {
            return TKN_REDIR_APPEND;
                } else {
                    (*cmd_i)--;
                    return TKN_REDIR_OUT;
                }
            case '\0': return TKN_NULL;
            case 0x7f: return TKN_BACK_SPACE;
            default: break;
        }

        /*cが英数字のTOKEN*/
        (*cmd_i)--;

        while(1) {
            c = cmd[(*cmd_i)++];
            if (c != '|' && c != '&' && c != '\n' && c != EOF && c != '<' && c != '>' && c != '\0' && !isblank(c) && c != 0x7f && c != 0x1b) {
                /*cが文字の間*/
                token[len++] = c;
                (*cur_pos)++;
                (*max_pos)++;
            } else {
                break;
            }
        }

        (*cmd_i)--;
        /*一つのコマンドor引数の完成*/
        token[len] = '\0';
        if (len > 0) {
            return TKN_STR;
        } else {
            return TKN_NULL;
        }
    }

    return TKN_NULL;
}


int getargv(int *ac, char *av[], int tkn[], int *cnt, int *bg, char *current_path, int *cur_pos, int *max_pos, char *cmd)
{

/*
    パイプか改行、EOFでreturn
    １コマンドずつ解析
    shellから呼ばれる
*/
    int token, flag = 1, cmd_i =  0;
    char *str;

    /*strをマロック*/
    mem_alloc(str, char, CMD_UNIT_LEN, EXIT_FAILURE);

    cmd_i = strlen(cmd);

    /*コマンドの解析*/
    while ((token = gettoken(str, current_path, cur_pos, max_pos, &flag, cmd, &cmd_i))) {
        switch (token) {
            case TKN_BG:
                av[(*ac)] = NULL;
	            *bg = 1;
                break;
            case TKN_AND:
                av[(*ac)++] = NULL;
                tkn[(*cnt)++] = TKN_AND;
                break;
            case TKN_OR:
                av[(*ac)++] = NULL;
                tkn[(*cnt)++] = TKN_OR;
                break;
            case TKN_REDIR_IN:
                av[(*ac)++] = NULL;
                tkn[(*cnt)++] = TKN_REDIR_IN;
                break;
            case TKN_REDIR_OUT:
                av[(*ac)++] = NULL;
                tkn[(*cnt)++] = TKN_REDIR_OUT;
                break;
            case TKN_REDIR_APPEND:
                av[(*ac)++] = NULL;
                tkn[(*cnt)++] = TKN_REDIR_APPEND;
                break;
            case TKN_STR:
                strcpy(av[(*ac)++], str);
                break;
            case TKN_PIPE:
                av[(*ac)++] = NULL;
                tkn[(*cnt)++] = TKN_PIPE;
                break;
            case TKN_EOL:
                free(str);
                av[(*ac)++] = NULL;
                tkn[(*cnt)++] = TKN_EOL;
                return TKN_EOL;
            case TKN_EOF:
                free(str);
                tkn[(*cnt)++] = TKN_EOF;
                return TKN_EOF;
            case TKN_UP_ARROW:
                free(str);
                return TKN_UP_ARROW;
            case TKN_DOWN_ARROW:
                free(str);
                return TKN_DOWN_ARROW;
	        case TKN_NULL: 
                free(str); 
                av[(*ac)++] = NULL; 
                tkn[(*cnt)++] = TKN_EOL; 
                return TKN_NULL;
	        default: break;
        }
    }

    return TKN_NULL;
}