#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

#define INPUT_LENGTH 4096
#define PWD_LENGTH 4096

char defined_commands[][20] = {
    "cd", "type", "echo", "exit", "pwd"
};

char *trim(char *ptr){
    if(ptr == NULL){
        return NULL;
    }
    while(ptr[0] == ' ' && ptr[0] != '\0'){
        ptr++;
    }
    if(ptr[0] == '\0'){
        return NULL;
    }
    return ptr;
}
void push_left(char *ptr){
    int i = 0;
    while(ptr[i] != '\0'){
        ptr[i] = ptr[i+1];
        i++;
    }
}

int parse_args(char *start, char **argv, char **file, int *fd, char *mode){
    int argc = 0;
    char *ptr = start;
    if(start == NULL){
        return 0;
    }
    ptr = trim(ptr);
    if(ptr == NULL){
        return argc;
    }

    char delim;
    int check_delim = 1;
    int is_file = 0;
    char *arg = NULL;
    while(ptr[0] != '\0'){
        if(check_delim){
            if(ptr[0] == '\'' || ptr[0] == '"'){
                delim = ptr[0];
                if(is_file){
                    *file = ptr + 1;
                }
                else{
                    arg = ptr + 1;
                }
                ptr++;
            }
            else if(ptr[0] == '>' || ((ptr[0] == '1' || ptr[0] == '2') && ptr[1] == '>')){
                if(file != NULL){
                    is_file = 1;
                }
                else{
                    break;
                }
                if(ptr[0] == '1' || ptr[0] == '2'){
                    *fd = (ptr[0] == '1') ? 1 : 2;
                    if(ptr[2] == '>'){
                        *mode = 'a';
                        ptr+=3;
                    }
                    else{
                        *mode = 'w';
                        ptr+=2;
                    }
                }
                else{
                    *fd = 1;
                    if(ptr[1] == '>'){
                        *mode = 'a';
                        ptr+=2;
                    }
                    else{
                        *mode = 'w';
                        ptr++;
                    }
                }
                ptr = trim(ptr);
                continue;
            }
            else if(ptr[0] != ' '){
                delim = ' ';
                if(is_file){
                    *file = ptr;
                }
                else{
                    arg = ptr;
                }
            }
            else{
                ptr = trim(ptr);
                if(ptr == NULL){
                    break;
                }
                continue;
            }
            check_delim = 0;
            continue;
        }
        if(ptr[0] == delim){
            if((delim != ' ' && ptr[1] == ' ') || delim == ' '){
                ptr[0] = '\0';
                if(!is_file){
                    if(argv != NULL){
                        argv[argc] = arg;
                    }
                    argc++;
                }
                else{
                    break;
                }
                ptr++;
                check_delim = 1;
            }
            else{
                push_left(ptr);
                if(ptr[0] == '\'' || ptr[0] == '"'){
                    delim = ptr[0];
                }
                else{
                    delim = ' ';
                }
            }
            continue;
        }
        if (ptr[0] == '\\' && delim != '\'') {
            if ((ptr[1] == '"' || ptr[1] == '\\' || ptr[1] == '$' || 
                 (delim == ' ' && (ptr[1] == ' ' || ptr[1] == 'n')))) {
                push_left(ptr);
                ptr++;
            } else {
                ptr += 2;
            }
            continue;
        }
        if(delim == ' ' && (ptr[0] == '"' || ptr[0] == '\'')){
            push_left(ptr);
            continue;
        }
        ptr++;
    }
    if(delim == ' ' & ptr[0] == '\0'){
        if(!is_file){
            if(argv != NULL){
                argv[argc] = arg;
            }
            argc++;
        }
    }

    return argc;
}

int main() {
    char input[INPUT_LENGTH];
    memset(input, 0, sizeof(input));
    const char *PATH = getenv("PATH");
    if(PATH == NULL){
        perror("Failed to get PATH");
        return -1;
    }

    while(1){
        printf("$ ");
        fflush(stdout);
        fgets(input, INPUT_LENGTH, stdin);
        input[strlen(input) - 1] = '\0';

        char *args_str_copy = strdup(input);
        char *args_str_copy_start = args_str_copy;
        args_str_copy = trim(args_str_copy);

        int argc = parse_args(args_str_copy, NULL, NULL, NULL, NULL);

        free(args_str_copy_start);

        char **argv = malloc((argc+1) * sizeof(char *));
        if(argv == NULL){
            perror("Failed to allocate memory");
            return -1;
        }

        char *args_str = input;
        args_str = trim(args_str);
       
        char *file = NULL;
        int fd = 0;
        char mode;
        int argc2 = parse_args(args_str, argv, &file, &fd, &mode);
        if(argc != argc2){
            perror("argc != argc2");
            return -1;
        }
        argv[argc] = NULL;
        char *command = argv[0];

        int std_fd;
        int file_fd;
        if(file != NULL){
            std_fd = dup(fd);
            if(mode == 'w'){
                file_fd = open(file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            }
            else{
                file_fd = open(file, O_WRONLY | O_CREAT | O_APPEND, 0644);
            }
            if(file_fd < 0){
                perror("Failed to open file");
                file = NULL;
            }
            dup2(file_fd, fd);
            close(file_fd);
        }

        if(command != NULL){

            if(strcmp(command, "exit") == 0){
                char *code = argv[1];
                if(code != NULL){
                    exit(atoi(code));
                }
                else{
                    exit(0);
                }
            }
            else if(strcmp(command, "echo") == 0){
                for(int i = 1; i < argc; i++){
                    char *val = argv[i];

                    printf("%s", val);
                    if(i != argc-1){
                        printf(" ");
                    }
                    else{
                        printf("\n");
                    }
                }
            }
            else if(strcmp(command, "type") == 0){
                for(int i = 1; i < argc; i++){
                    char *val = argv[i];

                    int builtin = 0;
                    for(int i = 0; i < sizeof(defined_commands)/sizeof(defined_commands[0]); i++){
                        char *builtin_command = defined_commands[i];
                        if(strcmp(val, builtin_command) == 0){
                            builtin = 1;
                            printf("%s is a shell builtin\n", val);
                            break;
                        }
                    }
                    if(!builtin){
                        char *path_copy = strdup(PATH);
                        if(path_copy == NULL){
                            perror("Failed to copy PATH");
                            return -1;
                        }
                        char *current_path = strtok(path_copy, ":");
                        int exists = 0;

                        long path_size;
                        char *full_path;
                        do{
                            path_size = strlen(current_path) + strlen(val) + strlen("/") + 1;
                            full_path = malloc(path_size);
                            if(full_path == NULL){
                                perror("Failed to allocate memory");
                                free(path_copy);
                                return -1;
                            }
                            snprintf(full_path, path_size, "%s/%s", current_path, val);
                            if(access(full_path, F_OK) == 0){
                                exists = 1;
                                break;
                            }
                            free(full_path);
                        } while((current_path = strtok(NULL, ":")) != NULL);

                        if(exists){
                            printf("%s is %s\n", val, full_path);
                            free(full_path);
                        }
                        else{
                            fprintf(stderr, "%s: not found\n", val);
                        }

                        free(path_copy);

                    }
                }
            }
            else if(strcmp(command, "pwd") == 0){
                char cwd[PWD_LENGTH];

                if(getcwd(cwd, sizeof(cwd)) == NULL){
                    perror("Failed to get cwd");
                    return -1;
                }
                printf("%s\n", cwd);
            }
            else if(strcmp(command, "cd") == 0){
                if(argc > 2){
                    fprintf(stderr, "cd: too many arguments\n");
                }
                else{
                    char *val = argv[1];
                    char *HOME = getenv("HOME");
                    if(val == NULL || val[0] == '~'){
                        char *rest_of_val = val + 1;
                        long full_path_size = strlen(HOME) + strlen(rest_of_val) + 1;
                        char *full_path = malloc(full_path_size);
                        if(full_path == NULL){
                            perror("Failed to allocate memory");
                            return -1;
                        }
                        
                        snprintf(full_path, full_path_size, "%s%s", HOME, rest_of_val);

                        if(chdir(full_path) != 0){
                            fprintf(stderr, "cd: %s: No such file or directory\n", full_path);
                        }

                        free(full_path);
                    }
                    else if(chdir(val) != 0){
                        fprintf(stderr, "cd: %s: No such file or directory\n", val);
                    }
                }
            }
            else{
                char *path_copy = strdup(PATH);
                if(path_copy == NULL){
                    perror("Failed to copy PATH");
                    free(argv);
                    return -1;
                }
                char *current_path = strtok(path_copy, ":");
                int exists = 0;

                long path_size;
                char *full_path;

                while(current_path != NULL){
                    path_size = strlen(current_path) + strlen(command) + strlen("/") + 1;
                    full_path = malloc(path_size);
                    if(full_path == NULL){
                        perror("Failed to allocate memory");
                        free(argv);
                        free(path_copy);
                        return -1;
                    }
                    snprintf(full_path, path_size, "%s/%s", current_path, command);
                    if(access(full_path, X_OK) == 0){
                        exists = 1;
                        break;
                    }
                    free(full_path);

                    current_path = strtok(NULL, ":");
                }

                if(exists){
                    pid_t pid = fork();
                    if(pid == 0){
                        if(execv(full_path, argv) == -1){
                            perror("Failed to execute command");
                            free(full_path);
                            free(path_copy);
                            free(argv);
                            return -1;
                        }
                    }
                    else if(pid > 0){
                        int status;
                        if(waitpid(pid, &status, 0) == -1){
                            perror("waitpid failed");
                            free(full_path);
                            free(path_copy);
                            free(argv);
                            return -1;
                        }
                    }
                    else{
                        perror("Fork failed");
                        free(full_path);
                        free(path_copy);
                        free(argv);
                        return -1;
                    }
                    free(full_path);
                }
                else{
                    fprintf(stderr, "%s: command not found\n", input);
                }
                free(path_copy);
            }
            free(argv);
        }

        if(file != NULL){
            dup2(std_fd, fd);
            close(std_fd);
        }
        memset(input, 0, sizeof(input));
    }
    return 0;
}
