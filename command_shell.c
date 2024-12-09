#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <ctype.h>

// Helper function
char *trim_whitespace(char *str)
{
    char *end;
    while (isspace((unsigned char)*str))
        str++;

    // Trim trailing space
    if (*str == 0)
        return str;

    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end))
        end--;

    // Write new null terminator
    *(end + 1) = '\0';

    return str;
}

int change_directory(char *path)
{
    if (chdir(path) == 0)
        return 0;
    perror("cd");
    return -1;
}

void print_working_directory()
{
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL)
    {
        printf("%s\n", cwd);
    }
    else
    {
        perror("pwd");
    }
}

void set_variable(char *key, char *value)
{
    setenv(key, value, 1);
}

void unset_variable(char *key)
{
    unsetenv(key);
}

char *substitute_variables(char *input)
{
    static char result[1024];
    memset(result, 0, sizeof(result));
    char *start = input, *dollar;

    while ((dollar = strchr(start, '$')) != NULL)
    {
        strncat(result, start, dollar - start); // Copy up to $
        char *key = dollar + 1;                 // Variable name after $
        char *end = key;
        while (*end && (isalnum(*end) || *end == '_'))
            end++;
        char var_name[64];
        strncpy(var_name, key, end - key);
        var_name[end - key] = '\0';
        char *value = getenv(var_name);
        if (value)
            strcat(result, value);
        start = end;
    }
    strcat(result, start); // Append remaining string
    return result;
}

void parse_arguments(char *cmd, char **args)
{
    int i = 0;
    char *token = strtok(cmd, " ");
    while (token)
    {
        args[i++] = token;
        token = strtok(NULL, " ");
    }
    args[i] = NULL;
}

void execute_command(char **args, int background)
{
    pid_t pid = fork();
    if (pid == 0)
    {
        execvp(args[0], args);
        perror("execvp");
        exit(EXIT_FAILURE);
    }
    else if (pid > 0)
    {
        if (!background)
            waitpid(pid, NULL, 0);
    }
    else
    {
        perror("fork");
    }
}

void redirect_input(char *file)
{
    int fd = open(file, O_RDONLY);
    if (fd < 0)
    {
        perror("open");
        exit(EXIT_FAILURE);
    }
    dup2(fd, STDIN_FILENO);
    close(fd);
}

void redirect_output(char *file)
{
    int fd = open(file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0)
    {
        perror("open");
        exit(EXIT_FAILURE);
    }
    dup2(fd, STDOUT_FILENO);
    close(fd);
}

void handle_piping(char *cmd1[], char *cmd2[])
{
    int pipefd[2];
    pipe(pipefd);
    if (fork() == 0)
    {
        // Child process 1
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[0]);
        execvp(cmd1[0], cmd1);
        perror("execvp");
        exit(EXIT_FAILURE);
    }
    if (fork() == 0)
    {
        // Child process 2
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[1]);
        execvp(cmd2[0], cmd2);
        perror("execvp");
        exit(EXIT_FAILURE);
    }
    close(pipefd[0]);
    close(pipefd[1]);
    wait(NULL);
    wait(NULL);
}

void parse_and_execute(char *input)
{
    char *parsed_input = substitute_variables(trim_whitespace(input));

    int background = 0;
    if (parsed_input[strlen(parsed_input) - 1] == '&')
    {
        background = 1;
        parsed_input[strlen(parsed_input) - 1] = '\0'; // Remove '&'
    }

    char *commands[10];
    int num_commands = 0;
    char *token = strtok(parsed_input, "|");
    while (token)
    {
        commands[num_commands++] = token;
        token = strtok(NULL, "|");
    }

    if (num_commands == 1)
    {
        char *args[64];
        parse_arguments(commands[0], args);

        for (int i = 0; args[i]; i++)
        {
            if (strcmp(args[i], "<") == 0)
            {
                redirect_input(args[i + 1]);
                args[i] = NULL;
            }
            else if (strcmp(args[i], ">") == 0)
            {
                redirect_output(args[i + 1]);
                args[i] = NULL;
            }
        }
        execute_command(args, background);
    }
    else
    {
        // Handle piping
        char *cmd1[64], *cmd2[64];
        parse_arguments(commands[0], cmd1);
        parse_arguments(commands[1], cmd2);
        handle_piping(cmd1, cmd2);
    }
}

int main()
{
    char input[1024];

    while (1)
    {
        printf("xsh# ");
        fflush(stdout);

        if (!fgets(input, sizeof(input), stdin))
            break;
        input[strcspn(input, "\n")] = '\0'; // Remove trailing newline

        if (strcmp(input, "quit") == 0 || strcmp(input, "exit") == 0)
            break;

        if (strncmp(input, "cd ", 3) == 0)
        {
            change_directory(input + 3);
        }
        else if (strcmp(input, "pwd") == 0)
        {
            print_working_directory();
        }
        else if (strncmp(input, "set ", 4) == 0)
        {
            char *key = strtok(input + 4, " ");
            char *value = strtok(NULL, " ");
            set_variable(key, value);
        }
        else if (strncmp(input, "unset ", 6) == 0)
        {
            unset_variable(input + 6);
        }
        else
        {
            parse_and_execute(input);
        }
    }
    return 0;
}