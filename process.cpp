#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>

#include <stdlib.h>
#include <stdio.h>


struct process
{
    pid_t PID;

    int stdoutFd;
    int stdinFd;
};

void
SearchProgramInPath(char* ProgramName, char* Out)
{
    // TODO: Don't search in path if ProgramName starts with '.' or '/'

    strcpy(Out, ProgramName);

    const char* Path = getenv("PATH");
    const char* NextPathPart = Path;
    while (NextPathPart)
    {
        char ProgramAtPath[1024];

        const char* P = NextPathPart;
        while (*P != ':' && *P != '\0')
            P++;

        int Len = P - NextPathPart;
        strncpy(ProgramAtPath, NextPathPart, Len);

        ProgramAtPath[Len] = '/';

        strcpy(&ProgramAtPath[Len+1], ProgramName);

        printf("Checking file %s\n", ProgramAtPath);
        if (access(ProgramAtPath, F_OK) != -1)
        {
            strcpy(Out, ProgramAtPath);
            break;
        }

        if (*P == ':')
            NextPathPart = P+1;
        else
            NextPathPart = NULL;
    }
}

void
ProcessCreate(process* Process, char* CommandLine)
{
    int NumSpaces = 0;

    char* P = CommandLine;
    while (*P != 0)
    {
        if (*P == ' ')
            NumSpaces++;

        P++;
    }


    char* Argv[NumSpaces+2];

    int I = 0;
    P = CommandLine;
    Argv[I] = CommandLine;
    while (*P != 0)
    {
        if (*P == ' ')
        {
            *P = '\0';

            I++;
            Argv[I] = P+1;
        }
        P++;
    }
    Argv[I+1] = NULL;

    char ProgramName[1024];
    SearchProgramInPath(Argv[0], ProgramName);
    Argv[0] = ProgramName;

    for (int I = 0; I < NumSpaces+1; I++)
    {
        printf("Argv[%d]: %s\n", I, Argv[I]);
    }

    int fdOut[2];
    int fdIn[2];

    if (pipe(fdOut))
        printf("Error in pipe\n");

    if (pipe(fdIn))
        printf("Error in pipe\n");
    
    pid_t PID = fork();

    if (PID == -1)
    {
        // TODO: Handle error
        printf("Error in handling\n");
    }
    else if (PID == 0)
    {
        if (dup2(fdOut[1], 1) == -1)
            printf("Error in dup2\n");

        // Duplicate stderr to the same fd
        if (dup2(fdOut[1], 2) == -1)
            printf("Error in dup2\n");

        if (dup2(fdIn[0], 0) == -1)
            printf("Error in dup2\n");

        close(fdOut[0]);
        close(fdOut[1]);

        close(fdIn[0]);
        close(fdIn[1]);

        execv(Argv[0], Argv);

        printf("Error: ");
        if (errno == ENOENT)
            printf("File not found\n");

        exit(1);
    }
    else
    {
        close(fdOut[1]);
        close(fdIn[0]);

        Process->PID = PID;
        Process->stdoutFd = fdOut[0];
        Process->stdinFd = fdIn[1];
        
        int ret = fcntl(fdOut[0], F_SETFL, O_NONBLOCK);
        if (ret == -1) {
            printf("fcntl failed\n");
        }
    }
}

int
ProcessReadFromStdout(process* Process, char* Buffer, int Size)
{
    int NRead = read(Process->stdoutFd, Buffer, Size);
    return NRead;
}

int
ProcessExited(process* Process)
{
    int ProcessStatus;
    int ret = waitpid(Process->PID, &ProcessStatus, WNOHANG);
    
    if (ret == -1)
        perror("waitpid");

    if (ret != 0 && WIFEXITED(ProcessStatus))
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

