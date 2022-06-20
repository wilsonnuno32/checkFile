//Nome: Nuno Wilson Monico Gaspar
//Nº:   2192011

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include <signal.h>

#include "debug.h"
#include "memory.h"
#include "args.h"

#define MAX_LINE_CHARS  256
#define DIR_FILE        8
#define EXTENSION_FLAG  1
#define FILETYPE_FLAG   0

int checkFileExistenceAndIfReadable(const char * path);
void checkFileExtension(const char * filename);
char* compareExtension(const char * filename, int flag);
void trata_sinal_info(int signal, siginfo_t *siginfo, void *context);


void Help(const char *msg){     //code from
    if( msg != NULL ){
        fprintf(stderr,"[ERRO]: %s\n", msg);
    }
    printf("Opções\n");
    printf("-f/--file nomeFicheiro  --  Verifica extensao do ficheiro nomeFicheiro, pode ser repetido ate 4 vezes\n");
    printf("-b/--batch nomeBatch  --  Verifica a extensao do ficheiro contido em cada linha do ficheiro nomeBatch\n");
    printf("-d/--dr nomeDiretoria  -- Verifica a extensao de cada ficheiro contido na diretoria nomeDiretoria\n");
    printf("--help  --  exibe ajuda suscinta\n");
}


int main(int argc, char *argv[]) {

    struct gengetopt_args_info args;
    char Err_S[MAX_LINE_CHARS];
    int counterFiles;

    // início da main/função
    struct sigaction act_info;
        
    act_info.sa_sigaction = trata_sinal_info;
    sigemptyset(&act_info.sa_mask);
    act_info.sa_flags = 0;              //fidedigno
    act_info.sa_flags |= SA_SIGINFO;    //info adicional sobre o sinal

    // Captura do sinal SIGQUIT 
    if(sigaction(SIGQUIT, &act_info, NULL) < 0)
        ERROR(4, "sigaction (sa_sigaction) - ???");

    if(cmdline_parser(argc, argv, &args))
        ERROR(1, "Erro: execução de cmdline_parser\n");

    if (argc == 1)
    {
        sprintf(Err_S,"No option selected");
        Help(Err_S);
        exit(1);
    }

    //****************************************************************************

    //option -f / --file
    if (args.file_given && !args.dir_given && !args.batch_given)
    {
        if (argc == 3)  //only one occurence
        {
            
            if (checkFileExistenceAndIfReadable(argv[2]) == 0) 
                checkFileExtension(argv[2]);
        }
        else            //multiple occurences
        {
            for (int i = 2; i < argc; i+=2)
            {
                if (strcmp(argv[i-1], "-f") == 0)
                {
                    if (checkFileExistenceAndIfReadable(argv[i]) == 0)
                        checkFileExtension(argv[i]);
                }
                else
                {
                    sprintf(Err_S,"Parâmetro desconhecido '%s'", argv[i-1]);
                    Help(Err_S);
                    exit(EXIT_FAILURE);
                } 
            }
        }
        
    }

    //****************************************************************************

    //option -b / --batch
    if (args.batch_given && !args.file_given && !args.dir_given)
    {
        FILE *fp;   //file pointer
        counterFiles = 0;
        
        if ((fp = fopen(argv[2], "r")) == NULL)
            ERROR(5, "fopen() - não foi possível abrir o ficheiro");

        char str_line[2048];    //string to save each line of batchfile

        printf("[INFO] analyzing files listed in ‘%s’\n", argv[2]);
        // "ler" até ao fim do ficheiro
        while (!feof(fp))   //while loop that stops when it reaches the end of the file
        {
            if (fgets(str_line, 2048, fp) != NULL) {        //get each line of batchfile
                strcpy(str_line, strtok(str_line, "\n"));   //removes \n at the end of str_line
                if (checkFileExistenceAndIfReadable(str_line) == 0)
                    checkFileExtension(str_line);
                counterFiles++;                             //increments counter of files analyzed in total
            }
        }
        printf("[SUMMARY] files analyzed: %d; files OK: %d; files MISMATCH: %d; errors: %d\n", counterFiles, 0, 0, 0);
        //free(str_line);
        
        if (fclose(fp) != 0)    //close file
            ERROR(6, "fclose() - não foi possível fechar o ficheiro");
    }

    //****************************************************************************

    //option -d / --dir
    if (args.dir_given && !args.file_given && !args.batch_given) {
        
        if (checkFileExistenceAndIfReadable(argv[2]) == 0)
        {
            DIR *directory;             //directory pointer
            struct dirent *dir;         //structure that contains name and type of entry -- d_type: 4 for directory, 8 for file
                                        //also contains name of file/directory
                                        //information given by function readdir()

            directory = opendir(argv[2]);   //open directory passed in third argument
            char copyArg[1024];             //string that holds the name of each entry of the directory

            if (directory) {
                printf("[INFO] analyzing files of directory ‘%s’\n", argv[2]);
                while ((dir = readdir(directory)) != NULL) {
                    if (dir->d_type == DIR_FILE) {      //if d_type == 8, it's a file, otherwise it is ignored
                        strcpy(copyArg, argv[2]);       //copy the value of argv[2] into copyArg
                        strcat(copyArg, dir->d_name);   //concatenate value of copyArg with entry of directory

                        if (checkFileExistenceAndIfReadable(copyArg) == 0)
                            checkFileExtension(copyArg);
                        
                        strncpy(copyArg, "", strlen(copyArg));  //clears content of copyArg
                        
                        counterFiles++;
                    }
                }
                closedir(directory);
            }
        printf("[SUMMARY] files analyzed: %d; files OK: %d; files MISMATCH: %d; errors: %d\n", counterFiles, 0, 0, 0);
        }
        
    }

    //****************************************************************************

    // gengetopt: libertar recurso (assim que possível)
    cmdline_parser_free(&args);

    return 0;
}


void checkFileExtension(const char * filename) {
    char * fileCurrentExtension = compareExtension(filename, EXTENSION_FLAG);    //compareExtension returns the extension of the file

    int fd[2]; //pipes to read and write
    pid_t pid;
    char outputExec[256];

    if (pipe(fd) == -1)
        ERROR(1, "pipe() failed!\n");

    pid = fork();

    if (pid == 0)
    {
        dup2(fd[1], STDOUT_FILENO);     //redirects stdout to fd[1]
        close(fd[0]);                   //child process doesn't read
            
        execlp("file", "file", "-b", "--mime-type", filename, NULL);
    }
    else if (pid > 0)
    {
        close(fd[1]);                                   //parent process doesn't write
        read(fd[0], outputExec, sizeof(outputExec));    //reads fd[0] output and stores in outputExec
        char * fileRealExtension = compareExtension(outputExec, FILETYPE_FLAG);
        if (strcmp(fileRealExtension, "jpeg") == 0 && strcmp(fileCurrentExtension, "jpg") == 0) //file type always returns jpeg
            strcpy(fileRealExtension, "jpg");                                                   //in this condition, transforms jpeg in jpg to match

        if (strcmp(fileRealExtension, "pdf") != 0 && strcmp(fileRealExtension, "gif") != 0 && strcmp(fileRealExtension, "jpeg") != 0 &&
            strcmp(fileRealExtension, "png") != 0 && strcmp(fileRealExtension, "mp4") != 0 && strcmp(fileRealExtension, "zip") != 0 &&
            strcmp(fileRealExtension, "html") != 0 && strcmp(fileRealExtension, "jpg") != 0) //extensions supported by the program
        {
            printf("[INFO] '%s': type '%s' is not supported by checkFile\n", filename, fileRealExtension);
        }   
        else if (strcmp(fileCurrentExtension, fileRealExtension) == 0)
            printf("[OK] '%s': extension '%s' matches file type '%s'\n", filename, fileCurrentExtension, fileRealExtension);
        else if (strcmp(fileCurrentExtension, "") == 0)
            printf("[ERROR] '%s': file doesn't have an extension visible\n", filename);
        else
            printf("[MISMATCH] '%s': extension is '%s', file type is '%s'\n", filename, fileCurrentExtension, fileRealExtension);
        
        strncpy(outputExec, "", strlen(outputExec));    //empties content of outputExec

        free(fileRealExtension);
        free(fileCurrentExtension);

        close(fd[0]);   //close pipe to read
        wait(NULL);
    }
    else
        ERROR(1, "fork() failed!\n");
}


char* compareExtension(const char * filename, int flag) {
    char * tokenExtension = strdup(filename);   //duplicates content of filename into tokenExtension
    char * extension;

    if (flag == EXTENSION_FLAG)
    {
        extension = malloc(sizeof(char)*5);
        if (strrchr(tokenExtension, '.') == NULL)   //if filename doesn't have extension visible
            strcpy(extension, ""); 
        else {
            strcpy(extension, (strrchr(tokenExtension, '.') + 1)); //searches last occurence of character '.'
        }
    } else {
        extension = malloc(sizeof(char)*strlen(tokenExtension));

        strcpy(extension, (strrchr(tokenExtension, '/') + 1));     //searches last occurence of character '/'
        strcpy(extension, strtok(extension, "\n")); //removes '\n' from extension
    }

    return extension;
}


int checkFileExistenceAndIfReadable(const char * path) {
    if (access(path, F_OK | R_OK) == -1) {          //F_OK if file is found and R_OK if file is readable
        fprintf(stderr, "[ERROR]: cannot open file '%s'", path);
        perror(" ");
        return -1;
    }
        
    return 0;
}


void trata_sinal_info(int signal, siginfo_t *siginfo, void *context) //code from aux_sinais.c from moodle
{
    (void)context;
    int aux;
    /* Cópia da variável global errno */
    aux = errno;

    if (signal == SIGQUIT)
        printf("Captured SIGQUIT signal (sent by PID: %ld). Use SIGINT to terminate application.\n", (long)siginfo->si_pid);
   
    errno = aux;
}