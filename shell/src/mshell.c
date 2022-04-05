#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include "config.h"
#include "siparse.h"
#include "utils.h"
#include "builtins.h"

typedef struct 
{
    pid_t first;
    int second;
    int third;
} triple;

triple notes[MAX_NOTES];
pid_t foreground[FOREGROUND_SIZE];
int fgcount = 0;

int check_terminal()
{
    struct stat check;
    int terminal = fstat(STDIN, &check);
    if(terminal == -1)
    {
        exit(EXEC_FAILURE);
    }
    if(S_ISCHR(check.st_mode))
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

void handle_errors(char name[])
{
    switch(errno)
    {
        case ENOENT:
            fprintf(stderr, "%s: no such file or directory\n", name);
            break;
        case EACCES:
            fprintf(stderr, "%s: permission denied\n", name);
            break;
        default:
            fprintf(stderr, "%s: exec error\n", name);
    }
}

char **get_argv(command * com)
{
    int com_len = 0, idx = 0;
    argseq *argseq = com->args;
    do
    {
        argseq = argseq->next;
        com_len++;
    }
    while(argseq != com->args);
    char **argv = malloc((com_len+1) * sizeof(char *));
    argv[com_len] = NULL;
    do
    {
        argv[idx++] =  argseq->arg;
        argseq = argseq->next;
    }
    while(argseq != com->args);
    return argv;
}

int shell_command_check(char **argv)
{
    int i = 0, found = 0;
    builtin_pair pair = builtins_table[0];
    while(pair.name != NULL)
    {
        if(strcmp(pair.name, argv[0]) == 0)
        {
            found = 1;
            break;
        }
        pair = builtins_table[++i];   
    }
    if(found)
    {
        if(pair.fun(argv) == -1) // some error occured
        {
            fprintf(stderr, "Builtin %s error.\n", argv[0]);
        };
        return 1;
    }
    return 0;
}

void redirections(command *com)
{
    int flags;
    redirseq * redirs = com->redirs;
    if(redirs != NULL)
    {
        do
        {
            flags = redirs->r->flags;
            if(IS_RIN(flags))
            {
                if(close(STDIN) == -1)
                {
                    exit(EXEC_FAILURE);
                }
                if(open(redirs->r->filename, O_RDONLY) == -1)
                {
                    handle_errors(redirs->r->filename);
                    exit(EXEC_FAILURE);
                }
            }
            else if(IS_ROUT(flags) || IS_RAPPEND(flags))
            {
                if(close(STDOUT) == -1)
                {
                    exit(EXEC_FAILURE);
                }
                int new_flags = O_CREAT | O_WRONLY;
                if(IS_RAPPEND(flags))
                {
                    new_flags |= O_APPEND;
                }
                else
                {
                    new_flags |= O_TRUNC;
                }
                if(open(redirs->r->filename, new_flags, S_IRWXU) == -1)
                {
                    handle_errors(redirs->r->filename);
                    exit(EXEC_FAILURE);
                }
            }
            redirs = redirs->next;
        } 
        while(redirs != com->redirs);
    }
}

void execute(command *com, char **argv, int in, int out, int pfd0, int background)
{
    pid_t child_pid = fork();
    if(child_pid == -1) // fork fails
    {
        fprintf(stderr, "fork failed error %d\n", errno);
        exit(EXEC_FAILURE);
    }
    if(!child_pid)
    {
        if(background)
        {
            if(setsid() == -1) exit(EXEC_FAILURE);
        }
        struct sigaction act;
        act.sa_handler = SIG_DFL;
        act.sa_flags = 0;
        if(sigemptyset(&act.sa_mask) == -1) exit(EXEC_FAILURE);
        if(sigaction(SIGINT, &act, NULL) == -1) exit(EXEC_FAILURE);

        if(pfd0 != in && pfd0 != out && pfd0 != -1) // close pipefd[0] if not needed
        {
            if(close(pfd0) == -1) exit(EXEC_FAILURE);
        }
        if(in != STDIN) // change input if needed
        {
            if(dup2(in, STDIN) == -1)
            {
                exit(EXEC_FAILURE);
            }
        }
        if(out != STDOUT) // change output if needed
        {
            if(dup2(out, STDOUT) == -1)
            {
                exit(EXEC_FAILURE);
            }
        }
        if(in != STDIN) // close unnecessary descriptors
        {
            if(close(in) == -1) exit(EXEC_FAILURE);
        }
        if(out != STDOUT)
        {
            if(close(out) == -1) exit(EXEC_FAILURE);
        }
        redirections(com);
        if(execvp(argv[0], argv) == -1)
        {
            handle_errors(argv[0]);
            exit(EXEC_FAILURE);
        }
        else
        {
            exit(1);
        }
    }
    else
    {
        sigset_t mask;
        if(sigemptyset(&mask) == -1) exit(EXEC_FAILURE);
        if(sigaddset(&mask, SIGCHLD) == -1) exit(EXEC_FAILURE);
        if(sigprocmask(SIG_BLOCK, &mask, NULL) == -1) exit(EXEC_FAILURE);
        if(!background)
        {
            for(int i = 0; i < FOREGROUND_SIZE; i++)
            {
                if(foreground[i] == -1)
                {
                    foreground[i] = child_pid;
                    fgcount++;
                    break;
                }
            }
        }
        if(sigprocmask(SIG_UNBLOCK, &mask, NULL) == -1) exit(EXEC_FAILURE);
    } 
}

int pipeline_handler(pipeline *pl)
{
    commandseq *commands = pl->commands;
    command *com = pl->commands->com;
    int flag = pl->flags & INBACKGROUND; // flag = 1 means background
    int stat;
    if(commands == NULL)
    {
        return 0;
    }
    if(pl->commands == pl->commands->next) // we have just one command so we want to check builtins table
    {
        if(com == NULL)
        {
            return 0;
        }
        char **argv = get_argv(com);
        if(!flag)
        {
            if(shell_command_check(argv))
            {
                return 0;
            }
        }
        execute(com, argv, STDIN, STDOUT, -1, flag);
        sigset_t mask, old;
        if(sigemptyset(&mask) == -1) exit(EXEC_FAILURE);
        if(sigaddset(&mask, SIGCHLD) == -1) exit(EXEC_FAILURE);
        if(sigprocmask(SIG_BLOCK, &mask, &old) == -1) exit(EXEC_FAILURE);
        while(fgcount > 0)
        {
            sigsuspend(&old);
        }
        if(sigprocmask(SIG_UNBLOCK, &mask, NULL) == -1) exit(EXEC_FAILURE);
        return 0;
    }
    int last = STDIN, count = 0;
    int pipefd[2];
    do
    {
        com = commands->com;
        if(commands->next == pl->commands) // checking if this is the last command
        {
            pipefd[1] = STDOUT;
        }
        else
        {
            if(pipe(pipefd) == -1)
            {
                exit(EXEC_FAILURE);
            }
        }
        char **argv = get_argv(com);
        execute(com, argv, last, pipefd[1], pipefd[0], flag);
        count++;
        if(pipefd[1] != STDOUT) // closing unnecessary descriptors
        {
            if(close(pipefd[1]) == -1) exit(EXEC_FAILURE);
        }
        if(last != STDIN)
        {
            if(close(last) == -1) exit(EXEC_FAILURE);
        }
        last = pipefd[0];
        commands = commands->next;
    }
    while(commands != pl->commands);
    sigset_t mask, old;
    if(sigemptyset(&mask) == -1) exit(EXEC_FAILURE);
    if(sigaddset(&mask, SIGCHLD) == -1) exit(EXEC_FAILURE);
    if(sigprocmask(SIG_BLOCK, &mask, &old) == -1) exit(EXEC_FAILURE);
    while(fgcount > 0)
    {
        sigsuspend(&old); // what if it fails?
    }
    if(sigprocmask(SIG_UNBLOCK, &mask, NULL) == -1) exit(EXEC_FAILURE);
    return 0;
}

void handler(int sig_nb)
{
    pid_t child;
    int status;
    int err = errno;
    do
    {
        child = waitpid(-1, &status, WNOHANG);
        if(child > 0)
        {
            int i;
            for(i = 0; i < FOREGROUND_SIZE; i++)
            {
                if(child == foreground[i])
                {
                    break;
                }
            }
            if(i == FOREGROUND_SIZE) // background
            {
                triple note;
                note.first = child;
                note.second = status;
                if(WIFEXITED(status))
                {
                    note.third = 1;
                }
                else if(WIFSIGNALED(status))
                {
                    note.third = 2;
                }
                for(int i = 0; i < MAX_NOTES; i++)
                {
                    if(notes[i].first == -1)
                    {
                        notes[i] = note;
                        break;
                    }
                }
            }
            else
            {
                foreground[i] = -1;
                fgcount--;
            }   
        }
    }
    while(child > 0);
    errno = err;
}

void print_notes()
{
    triple empty_triple;
    empty_triple.first = empty_triple.second = empty_triple.third = -1;
    for(int i = 0; i < MAX_NOTES; i++)
    {
        if(notes[i].first != -1)
        {
            switch(notes[i].third)
            {
                case 1:
                    printf("Background process %d terminated. (exited with status %d)\n", notes[i].first, notes[i].second);
                    break;
                case 2:
                    printf("Background process %d terminated. (killed by signal %d)\n", notes[i].first, notes[i].second);
                    break;
                default:
                    printf("Background process %d terminated. (unknown reason)\n", notes[i].first);
            }
            notes[i] = empty_triple;
        }
    }
}

int main(int argc, char *argv[])
{
    pipelineseq *ln;
    command *com;
    char buf[2 * MAX_LINE_LENGTH];
    char *current = buf;
    char *end = buf;
    int read_more = 1, prompt = 1, too_long = 0, size = -2;
    int term = check_terminal();

    triple empty_triple;
    empty_triple.first = empty_triple.second = empty_triple.third = -1;
    for(int i = 0; i < FOREGROUND_SIZE; i++)
    {
        foreground[i] = -1;
    }
    for(int i = 0; i < MAX_NOTES; i++)
    {
        notes[i] = empty_triple;
    }

    struct sigaction act;
    act.sa_handler = handler;
    act.sa_flags = 0;
    if(sigemptyset(&act.sa_mask) == -1) exit(EXEC_FAILURE);
    if(sigaction(SIGCHLD, &act, NULL) == -1) exit(EXEC_FAILURE);

    act.sa_flags = 0;
    act.sa_handler = SIG_IGN; // PLS ADD \n
    if(sigaction(SIGINT, &act, NULL) == -1) exit(EXEC_FAILURE);

    while(1)
    {
        if(term && prompt) // checking if prompt is needed
        {
            sigset_t mask;
            if(sigemptyset(&mask) == -1) exit(EXEC_FAILURE);
            if(sigaddset(&mask, SIGCHLD) == -1) exit(EXEC_FAILURE);
            if(sigprocmask(SIG_BLOCK, &mask, NULL) == -1) exit(EXEC_FAILURE);
            print_notes();
            if(sigprocmask(SIG_UNBLOCK, &mask, NULL) == -1) exit(EXEC_FAILURE);
            fflush(stdout);
            printf("%s", PROMPT_STR);
            fflush(stdout);
        }
        if(read_more) // check if i want to read more
        {
            size = read(STDIN, end, 2*MAX_LINE_LENGTH-(current-buf));
            if(size == -1) // read fails
            {
                if(errno == EINTR)
                {
                    continue;
                }
                exit(EXEC_FAILURE);
            }
            end += size;
            *end = 0;
        }
        if(size == 0)
        {
            if(term)
            {
                printf("\n");
            }
            else
            {
                *end = 0;
            }
            break;
        }
        while(current < end && *current != '\n') //search for first '\n'
        {
            current++;
        }
        if(current - buf + 1 > MAX_LINE_LENGTH) //checking line length
        {
            too_long = 1;
        }
        if(current >= end) // we didn't find any line so we want to read more
        {
            if(too_long) // we want to ignore what we already read
            {
                current = buf;
                end = buf;
            }
            if(term) // we dont want 2 prompts in the next line
            {
                prompt = 0;
            }
            read_more = 1;
            continue;
        }
        prompt = 1;
        read_more = 0;
        *current = 0;
        if(too_long) // syntax error
        {
            ln = NULL;
            too_long = 0;
        }
        else
        {
            ln = parseline(buf);
        }
        memcpy(buf, current + 1, ((end - current) - 1) * sizeof(char));
        end -= (current - buf + 1); // subtructing line length
        current = buf;
        *end = 0;
        if(ln == NULL)
        {
            fprintf(stderr, "%s\n", SYNTAX_ERROR_STR);
            continue;
        }
        pipelineseq * pipelineseq = ln;
        do // iterate over pipelineseq
        {
            pipeline * pl = pipelineseq->pipeline;
            pipeline_handler(pl);
            pipelineseq = pipelineseq->next;
        }
        while(pipelineseq != ln);
    }
    return 0;
}