/* Используемая архитектура: Язык С, Linux */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#define NO_ERROR       0
#define ERROR_EOF      1
#define ERROR_LINE     2 
#define ERROR_CD       3 
#define ERROR_AMP      4
#define ERROR_REDIRECT 5

#define NO_QUOTES  0
#define IN_QUOTES  1

#define IN_FOREGROUND 0
#define IN_BACKGROUND 1

#define NO_APPEND 0
#define WITH_APPEND 1

#define NORMAL_STATUS 0

#define BUFFER_SIZE 10

#define OR   1
#define AND  2
#define PIPE 3

struct command
{
    char **argv;
    char *in, *out;
    int mode;
    int pid;
    int status;
    int fd_in;
    int fd_out;
};

struct pipeline_list
{
    struct command *cmd;
    struct pipeline_list *next;
    int background;
};

struct pid_list
{
    int pid;
    struct pid_list *next;
} *pipeline_pids = NULL;

int siginted = 0;

void error_message(int error)
{
    switch (error)
    {
        case ERROR_EOF: 
            printf("\n\nERROR: Unexpected end of file");
	    break;
        case ERROR_LINE: 
            printf("ERROR: Unexpected end of line\n\n");  
            break;
        case ERROR_CD:
            printf("ERROR: Invalid format of 'cd' command\n\n");
            break;
        case ERROR_AMP:
            printf("ERROR: '&' must be the last symbol in the line\n\n");
            break;
        case ERROR_REDIRECT:
            printf("ERROR: Invalid redirection syntax\n\n");
    }
}

char* copy_word(char *wrd, int length)
{
    if (length > 0)
    {
        char *tmp = malloc(length * (sizeof(char) + 1));
        int i;
        for (i = 0; i < length; i++)
            tmp[i] = wrd[i];
        tmp[length] = 0;
        return tmp;
    }
    else
        return NULL;
}

int finish_line(void)
{
    int c;
    while (((c = getchar()) != '\n') && (c != EOF)) { }
    if (c == '\n')
        return c;
    else
        return EOF;
}

char *increase_buffer(char *buff, int nbuff)
{
    int len, j;
    char *tmp;

    len = (nbuff) * BUFFER_SIZE;
    tmp = malloc((nbuff+1) * BUFFER_SIZE);
    for (j = 0; j != len; j++)
        tmp[j] = buff[j];
    free(buff);

    return tmp;
}

void add_to_buffer(char **buff, int c, int *i)
{
    if ((((*i)-BUFFER_SIZE) % BUFFER_SIZE) == 0)
        *buff = increase_buffer(*buff,((*i)/BUFFER_SIZE)+1);
    (*buff)[*i] = c;
    (*i)++;
}

void print_command(struct command *cmd)
{
    int i = 0;
    if (cmd != NULL)
    {
       while (cmd->argv[i] != NULL)
       {
           printf("[%s] ", cmd->argv[i]);
           i++;
       }
       printf("[%s] ", cmd->argv[i]);
       printf("; ");
       printf("in: <%s>, out: <%s>, mode=%d\n", 
              cmd->in == NULL ? "null" : cmd->in, 
              cmd->out == NULL ? "null" : cmd->out, 
              cmd->mode);
    }
    else
        printf("[empty command]\n");
}

struct pipeline_list* add_to_pipeline(struct pipeline_list* pipeline, 
                                      struct command *cmd)
{
    struct pipeline_list *tmp, *chain;
    if (pipeline == NULL)
    {
        pipeline = malloc(sizeof(*pipeline));
        pipeline->cmd = cmd;
        pipeline->next = NULL;
    }
    else
    {
        tmp = pipeline;
        while ((tmp->next) != NULL)
            tmp = tmp->next;
        chain = malloc(sizeof(*pipeline));
        chain->cmd = cmd;
        chain->next = NULL;
        tmp->next = chain;
    }
    return pipeline;
}
 
void print_pipeline(struct pipeline_list *pipeline)
{
    struct pipeline_list *tmp = pipeline;
    int i = 1;
    if (pipeline == NULL)
        printf("{ EMPTY PIPELINE }\n");
    else
    {
        printf("{ PIPELINE }\n");
        while (tmp != NULL)
        {
            printf("%d: ",i++);
            print_command(tmp->cmd);
            tmp = tmp->next;
        } 
    }
    printf("Background = ");
    printf("%s\n", 
           pipeline->background == IN_BACKGROUND ? "true" : "false");   
}

void clear_command(struct command *cmd)
{
    int i = 0;
    if (cmd != NULL)
    {
        if (cmd->argv != NULL)
        {
            while (cmd->argv[i] != NULL)
            {
                free(cmd->argv[i]);
                i++;
            }

            free(cmd->argv);
        }

        free(cmd);
    }
}

void clear_pipeline(struct pipeline_list *pipeline)
{
    if (pipeline != NULL)
    {
        clear_pipeline(pipeline->next);
        clear_command(pipeline->cmd);
        free(pipeline);
    }
}    

void clear_address(char** in, char** out)
{
    free(*in);
    free(*out);
}

int word_equal_cd(char* wrd)
{
    if (wrd[0] == 'c' && wrd[1] == 'd' && wrd[2] == 0)
        return 1;
    else 
        return 0;
}

int word_length(char** wrd)
{
    int i = 0;
    while (wrd[i] != NULL)
        i++; 
    return i;
}
int skip_spaces(void)
{
    int c;
    while (((c = getchar()) == ' ') || (c == '\t')) { }
    if (c == EOF)
        return EOF;
    else
    {
        ungetc(c,stdin);
        return NORMAL_STATUS;
    }
}    

char* read_word(int *status, int *err)
{
    int c, i = 0, quotes = NO_QUOTES;
    char *wrd;
    char *buff = malloc(BUFFER_SIZE);
    buff[0] = '\0';

    *err = NO_ERROR;
    if (((*status) = skip_spaces()) == EOF)
        return NULL;
    while ((c = getchar()) != EOF)
    {
        if (quotes == IN_QUOTES)
        {
            if (c == '\n')
            {
                *status = c;
                *err = ERROR_LINE; 
                free(buff);
                return NULL;
            }
            else 
            if (c == '"')
                quotes = NO_QUOTES;
            else
                add_to_buffer(&buff,c,&i); 
        }
        else
        {             
            if ((c == ' ') || (c == '\t')) 
            {
                *status = NORMAL_STATUS;
                wrd = copy_word(buff,i);  
                free(buff);
                return wrd;
            } 
            else
            if ((c == '<') || (c == '>') 
                || (c == '&') || (c == '\n') || (c == '|'))
            {
                *status = c;
                wrd = copy_word(buff,i);
                free(buff);
                return wrd;
            }
            if (c == '"')
                quotes = IN_QUOTES;
            else
                add_to_buffer(&buff,c,&i);
        }
    }
    if (quotes == IN_QUOTES)
        *err = ERROR_EOF;
    *status = EOF;
    return NULL;   
}   

char** add_word_to_cmd(char** cmd, char* wrd, int* num)
{
    char** tmp;
    int i = 0;
    if (wrd != NULL)
    {
        (*num)++;
        tmp = malloc((sizeof(*tmp)) * ((*num)+1));
        while (i < ((*num)-1))
        {
            tmp[i] = cmd[i];
            i++;
        }
        tmp[i] = wrd;
        tmp[(*num)] = NULL;
        free(cmd);
        return tmp;
    }
    else
    {
        return cmd;
    }
}

int read_address(int* status, int* status_prev, char** wrd, 
                    char** in, char** out, int* outmode)
{
    int error = NO_ERROR;
    if (*status_prev == '<')
    {
        if ((*wrd) == NULL)
            error = ERROR_REDIRECT;
        else
        {
            free(*in);
            *in = *wrd;
        }
    }  
    else 
    if (*status_prev == '>')
    {
        if (*wrd == NULL)
        {
            if (*status == '>')
            {
                *wrd = read_word(status, &error);
                if (error == NO_ERROR)
                {
                    if (*wrd == NULL)
                        error = ERROR_REDIRECT;
                    else
                    {
                        free(*out);
                        *out = *wrd;
                        *outmode = WITH_APPEND;
                    } 
                }
            }
            else 
                error = ERROR_REDIRECT;
        }
        else
        {
            free(*out);
            *out = *wrd;
            *outmode = NO_APPEND;
        }
    }
    return error;
}

struct command *read_command(int* st, int *err, int* background)
{
    int status = NORMAL_STATUS, status_prev = NORMAL_STATUS, 
        error = NO_ERROR, num = 0;
    char *wrd;
    char **argv = NULL;

    char *in = NULL, *out = NULL;
    int outmode;

    int mode = PIPE;

    *st = NORMAL_STATUS;
    *err = NO_ERROR;
    *background = IN_FOREGROUND;
    while ((status != '\n') && (status != '|') && (status != EOF)
           && (status != '&') && (error == NO_ERROR))
    {
        status_prev = status;

        wrd = read_word(&status, &error);
        if (status == '&')
        {
            int cn = getchar();
            if ((cn == EOF) || (cn == '\n'))
            {
                *background = IN_BACKGROUND;
                status = cn;
            }
            else if (cn == '&')
            {
                mode = AND;
                status  = '|';
            }
            else
            {
                error = ERROR_AMP;
            }
        }
        else if (status == '|')
        {
            char cn = getchar();
            if (cn == '|')
            {
                mode = OR;
            }
            else
            {
                mode = PIPE;
                ungetc(cn, stdin);
            }
        }
        
        if (error == NO_ERROR)
        {
            if ((status_prev == '<') || (status_prev == '>'))
                error = read_address(&status, &status_prev, &wrd, 
                                     &in, &out, &outmode);    
            else 
                argv = add_word_to_cmd(argv, wrd, &num); 
        }
    }

    if (error == NO_ERROR)
        *st = status;
    else
    {
        free(wrd);
        *err = error;
        if ((status != '\n') && (status != EOF))
            *st = finish_line();
        else
            *st = status;
    }   
    
    if (argv != NULL)
    {
        struct command *cmd = malloc(sizeof(*cmd));
        cmd->argv = argv;
        cmd->in = in;
        cmd->out = out;
        cmd->mode = mode;
        cmd->pid = -1;
        cmd->status = -1;     
        cmd->fd_in = -1;
        cmd->fd_out = -1;
        return cmd;
    }
    else
        return NULL;            
}

int command_length(char **cmd)
{
    int i = 0;
    if (cmd == NULL)
        return 0;
    else
    {
        while (cmd[i] != NULL)
            i++;
        return i;
    }
}    

struct pipeline_list *read_pipeline(int* st, int* err, int* background)
{
    struct pipeline_list *pipeline = NULL;
    struct command *cmd;
    int status = NORMAL_STATUS, error = NO_ERROR;
    
    while ((status != '\n') && (status != EOF) && (error == NO_ERROR))
    {
        cmd = read_command(&status, &error, background);
        if ((error == NO_ERROR) && (cmd != NULL))
            pipeline = add_to_pipeline(pipeline, cmd);
    }
    
    *err = error;
    *st = status;
    
    if (pipeline == NULL)
        return NULL; 

    pipeline->background = *background;  
    return pipeline;
}

int pipeline_length(struct pipeline_list *pipeline)
{
    struct pipeline_list *tmp = pipeline;
    int i = 0;
    while (tmp != NULL)
    {
        i++;
        tmp = tmp->next;
    }
    return i;
}

int redirect_output(char* address, int outmode)
{
    int fd;
    if (address != NULL)
    {
        if (outmode == NO_APPEND)
            fd = open(address,O_WRONLY|O_CREAT|O_TRUNC,0666);
        else
            fd = open(address,O_WRONLY|O_CREAT|O_APPEND,0666);
    
        if (fd != -1)
        {
            dup2(fd,1);
            close(fd);
        }
        else
            perror(address);
        return fd;    
    }
    else
        return 1;
}

void delete_pid_from_list(int zombie)
{
    struct pid_list *tmp, *prev;
    if (pipeline_pids != NULL)
    {
        if (pipeline_pids->pid == zombie)
        {
            tmp = pipeline_pids;
            pipeline_pids = pipeline_pids->next;
            free(tmp);
        }
        else
        {
            tmp = pipeline_pids;
            while ((tmp->pid != zombie) && (tmp->next != NULL))
            {
                prev = tmp;
                tmp = tmp->next;
            }
            if (tmp->pid == zombie)
            {
                prev->next = tmp->next;
                free(tmp);
            }
        }
    }
}

void clear_zombies(int background)
{
    int zombie, status;
/*    if (background == IN_BACKGROUND)
    {   
        while((zombie = wait4(-1,&status,WNOHANG,NULL)) > 0)
        {
            if (zombie > 0)
                fprintf(stderr, "Process %d exited: %d\n", 
                        zombie, WEXITSTATUS(status));
        }
    }
    else
    {*/ 
        while ((pipeline_pids != NULL) && (zombie !=-1))
        {
            zombie = wait(&status);
            if (zombie <= 0)
                break;

            delete_pid_from_list(zombie);
            fprintf(stderr, "Process %d exited: %d\n", 
                    zombie, WEXITSTATUS(status));
        }
  //  }
}

void add_pid_to_list(int pid)
{
    struct pid_list *tmp, *chain;

    chain = malloc(sizeof(*chain));
    chain->pid = pid;
    chain->next = NULL;

    if (pipeline_pids == NULL)
        pipeline_pids = chain;
    else
    {
        tmp = pipeline_pids;
        while (tmp->next != NULL)
            tmp = tmp->next;
        tmp->next = chain;
    }
}

void execute_command(struct command *cmd, 
                     int background,
                     int last, int mode)
{

    int fd_in;
    if (cmd->fd_in != -1)
        fd_in = cmd->fd_in;
    else if (cmd->in != NULL)
        fd_in = open(cmd->in, O_RDONLY);
    else
        fd_in = 0;    

    if (last == 0)
    {
        int fd[2];
        pipe(fd);
        cmd->pid = fork();
        if (cmd->pid == 0)
        {
            if (fd_in != 0)
            {
                dup2(fd_in,0);
                close(fd_in);
            }

            int fd_out = redirect_output(cmd->out, NO_APPEND);

            if (mode == PIPE)
                dup2(fd[1], fd_out);
            
            close(fd[1]);
            close(fd[0]);
            execvp(cmd->argv[0], cmd->argv);
            perror(cmd->argv[0]);
            exit(1);
        }
        else
        {
            if (cmd->pid == -1)
            {
                perror("fork");
                exit(1);
            }
        } 
        close(fd[1]);
        if (fd_in != 0)
           close(fd_in);
        cmd->fd_out = fd[0];

        //if (background == IN_FOREGROUND)
            add_pid_to_list(cmd->pid);
    }

    else // last
    {
        cmd->pid = fork();
        if (cmd->pid == 0)
        {
            if (fd_in != 0)
            {
                dup2(fd_in,0);
                close(fd_in);
            }
            redirect_output(cmd->out, NO_APPEND);
            execvp(cmd->argv[0],cmd->argv);
            perror(cmd->argv[0]);
            exit(1);
        }
        else
        {
            if (cmd->pid == -1)
            {
                perror("fork");
                exit(1);
            }
            if (fd_in != 0)
                close(fd_in);
          
            //if (background == IN_FOREGROUND)
                add_pid_to_list(cmd->pid);
   
        }
    }
}
void pipeline_execution(struct pipeline_list *pipeline)
{
    struct pipeline_list *tmp = pipeline;
    struct command *prev_cmd = NULL;

    pipeline_pids = NULL;

    int last;
    if (tmp != NULL)
    {
        if (tmp->next == NULL)
            last = 1;
        else
            last = 0;

        execute_command(tmp->cmd, 
                        pipeline->background, last, tmp->cmd->mode);
        prev_cmd = tmp->cmd;
        tmp = tmp->next;
    }
    else
    {
        return;
    }

    while (tmp != NULL)
    {
        if (tmp->next == NULL)
            last = 1;
        else
            last = 0;


        if (siginted)
            break;

        if (prev_cmd->mode != PIPE)
        {
            int status;

            wait4(prev_cmd->pid, &status, 0, NULL);
            fprintf(stderr, "Process %d exited: %d\n", 
                    prev_cmd->pid, WEXITSTATUS(status));
            

            if (siginted)
                break;
        
            if (prev_cmd->mode == AND && WIFEXITED(status) 
                && WEXITSTATUS(status) == 0)
            {
                execute_command(tmp->cmd,
                                pipeline->background,
                                last,
                                tmp->cmd->mode);
            }
            else if (prev_cmd->mode == OR && WIFEXITED(status) &&
                    WEXITSTATUS(status) != 0)
            {
                execute_command(tmp->cmd,
                                pipeline->background,
                                last,
                                tmp->cmd->mode);
            }
            else
            {
                prev_cmd->mode = tmp->cmd->mode;
                tmp = tmp->next;
                continue;
            }
        }
        else // mode == PIPE
        {
            if (siginted)
                break;

            tmp->cmd->fd_in = prev_cmd->fd_out;
            execute_command(tmp->cmd, 
                            pipeline->background,
                            last,
                            tmp->cmd->mode);
        }

        prev_cmd = tmp->cmd;
        tmp = tmp->next;
    }

    clear_zombies(pipeline->background);
    siginted = 0;  
}

void pipeline_execution1(struct pipeline_list *pipeline)
{
    if (pipeline == NULL)
        return; 

    if (pipeline->background == IN_FOREGROUND)
    {
        pipeline_execution(pipeline);
    }
    else
    {
        int bpid = fork();
        if (bpid == 0)
        {
            pipeline_execution(pipeline);
            exit(0);
        }
    }
}

void kill_all_processes()
{
    struct pid_list *tmp = pipeline_pids;

    while (tmp != NULL)
    {
        kill(tmp->pid, SIGINT);
        tmp = tmp->next;
    }
}

void handler(int a)
{
    signal(SIGINT, handler);
    siginted = 1;
    
    kill_all_processes();
}

int main(void)
{

    signal(SIGINT, handler);

    int status = NORMAL_STATUS, error = NO_ERROR, background;
    struct pipeline_list *pipeline;
    //printf("\n========== SHELL LAUNCHED ==========\n");
    while (status != EOF)
    {
        //printf(">: "); 
        pipeline = read_pipeline(&status,&error,&background);   
        if ((error == NO_ERROR) && (status != EOF))
        {
        //    print_pipeline(pipeline);
            pipeline_execution1(pipeline);
        }
    
        error_message(error); 
        clear_pipeline(pipeline);
    } 
    //printf("\n========== SHELL FINISHED ==========\n");
    return 0;
}
