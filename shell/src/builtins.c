#include <string.h>
#include <stdio.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <dirent.h>
#include <errno.h>
#include "builtins.h"

int exitt(char *[]);
int echo(char *[]);
int cd(char *[]);
int lkill(char *[]);
int ls(char *[]);
int num_of_args(char *[]);

builtin_pair builtins_table[] =
{
    {"exit",	&exitt},
    {"lecho",	&echo},
    {"echo",    &echo},
    {"lcd",     &cd},
    {"lkill",	&lkill},
    {"lls",     &ls},
    {NULL,NULL}
};

int num_of_args(char * argv[])
{
    int i = 1, count = 0;
    while(argv[i++])
    {
        count++;
    }
    return count;
}

int exitt(char * argv[])
{
    if(num_of_args(argv) != 0)
    {
        return -1;
    }
    exit(1);
}

int echo(char * argv[])
{
    int i = 1;
    if(argv[i])
    {
        printf("%s", argv[i++]);
    }
    while(argv[i])
    {
        printf(" %s", argv[i++]);
    }
    printf("\n");
    fflush(stdout);
    return 0;
}

int cd(char * argv[])
{
    int num = num_of_args(argv);
    if(num == 1)
    {
        if(chdir(argv[1]) == -1) // chdir error
        {
            return -1;
        }
    }
    else if(num == 0)
    {
        if(chdir(getenv("HOME")) == -1) // chdir error
        {
            return -1;
        }
    }
    else
    {
        return -1;
    }
    return 0;
}

int lkill(char * argv[]) // to fix
{
    int num = num_of_args(argv);
    if(num == 2)
    {
        char * check;
        long first = strtol(argv[1], &check, 10);
        if(argv[1][0] != '-')
        {
            return -1;
        }
        if(*check != 0)
        {
            return -1;
        }
        long second = strtol(argv[2], &check, 10);
        if(*check != 0)
        {
            return -1;
        }
        return kill(second, labs(first));
    }
    else if(num == 1)
    {
        char * check;
        long first = strtol(argv[1], &check, 10);
        if(*check != 0)
        {
            return -1;
        }
        return kill(first, SIGTERM);
    }
    else
    {
        return -1;
    }
    return 0;
}

int ls(char * argv[])
{
    int num = num_of_args(argv);
    if(num != 0)
    {
        return -1;
    }
    DIR * mydir;
    struct dirent * myfile;
    mydir = opendir(".");
    if(mydir == NULL) // opendir error
    {
        return -1;
    }
    errno = 0;
    while((myfile = readdir(mydir)) != NULL)
    {
        if(myfile->d_name[0] != '.')
        {
            printf("%s\n", myfile->d_name);
        }
    }
    if(errno != 0) // readdir error
    {
        return -1;
    }
    if(closedir(mydir) == -1) // closedir error
    {
        return -1;
    }
    fflush(stdout);
    return 0;
}