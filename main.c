#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct {
  char *target_filename;
  int open_flags;
  int target_fd;
} Redirection;

typedef struct {
  char **args;
  Redirection redir;
} Command;

typedef struct {
  char *name;
  void (*handler)(char **args);
} Builtin;

void handle_exit(char **args);
void handle_type(char **args);
void handle_echo(char **args);
void handle_pwd(char **args);
void handle_cd(char **args);

char *read_line();
char **tokenize_line(char *line);
void free_tokens(char **tokens);
char *find_executable(char *program_name);
void run_external(char **args, Redirection *redir);
Command parse_command(char **tokens);
void free_command(Command *cmd);
void evaluate_command(Command *cmd);

Builtin builtins[] = {
    {"exit", handle_exit}, {"type", handle_type}, {"echo", handle_echo},
    {"pwd", handle_pwd},   {"cd", handle_cd},
};

int main(void) {
  while (1) {
    printf("$ ");

    char *line = read_line();
    if (line == NULL) {
      break;
    }

    char **tokens = tokenize_line(line);
    if (tokens[0] == NULL) {
      free_tokens(tokens);
      free(line);
      continue; // just prompt again
    }

    Command cmd = parse_command(tokens);

    evaluate_command(&cmd);

    free_command(&cmd);
    free_tokens(tokens);
    free(line);
  }
  return 0;
}

void handle_exit(char **args) { exit(0); }

void handle_type(char **args) {
  char *program_name = args[1];
  if (program_name == NULL) {
    return;
  }

  for (int i = 0; i < sizeof(builtins) / sizeof(builtins[0]); i++) {
    if (strcmp(program_name, builtins[i].name) == 0) {
      printf("%s is a shell builtin\n", program_name);
      return;
    }
  }

  char *exec_path = find_executable(program_name);
  if (exec_path == NULL) {
    printf("%s: not found\n", program_name);
    return;
  }

  printf("%s is %s\n", program_name, exec_path);

  free(exec_path);
}

void handle_echo(char **args) {
  for (int i = 1; args[i] != NULL; i++) {
    printf("%s", args[i]);

    if (args[i + 1] != NULL) {
      printf(" ");
    }
  }
  printf("\n");
}

void handle_pwd(char **args) {
  char *cwd = getcwd(NULL, 0);
  if (cwd == NULL) {
    return;
  }

  printf("%s\n", cwd);

  free(cwd);
}

void handle_cd(char **args) {
  char *dir = args[1];

  if (dir == NULL || strcmp(dir, "~") == 0) {
    dir = getenv("HOME");
  }

  int result = chdir(dir);
  if (result == -1) {
    printf("cd: %s: No such file or directory\n", dir);
  }
}

char *read_line() {
  char *line = NULL;   // dynamically allocated buffer
  size_t capacity = 0; // current buffer capacity

  ssize_t len = getline(&line, &capacity, stdin); // excludes '\0'
  // Check for error or EOF
  if (len == -1) {
    free(line);
    return NULL;
  }

  // Remove trailing newline
  if (len > 0 && line[len - 1] == '\n') {
    line[len - 1] = '\0';
  }

  return line;
}

char **tokenize_line(char *line) {
  int line_len = strlen(line);
  // Number of arguments cannot exceed the length of the line
  int args_count = 0;
  char **args = malloc((line_len + 1) * sizeof(char *));
  // Number of chars in an argument cannot exceed the length of the line
  int builder_len = 0;
  char *builder = malloc((line_len + 1) * sizeof(char));
  // Track quote state ('\0' = none,  '\'' = single, '"' = double)
  char active_quote = '\0';

  for (int i = 0; i < line_len; i++) {
    char curr_char = line[i];

    if (active_quote != '\0') {
      if (curr_char == active_quote) {
        active_quote = '\0';
        continue;
      }

      if (active_quote == '"' && curr_char == '\\' && (i + 1 < line_len)) {
        char next_char = line[i + 1];
        if (next_char == '\\' || next_char == '"' || next_char == '$' ||
            next_char == '\n') {
          builder[builder_len++] = next_char;
          i++;
          continue;
        }
      }

      builder[builder_len++] = curr_char;
      continue;
    }

    switch (curr_char) {
    case '\'':
    case '"':
      active_quote = curr_char;
      continue;
    case ' ':
    case '\t':
      if (builder_len > 0) {
        builder[builder_len] = '\0';
        args[args_count++] = strdup(builder);
        builder_len = 0;
      }
      continue;
    case '\\':
      if (i + 1 < line_len) {
        builder[builder_len++] = line[i + 1];
        i++;
      }
      continue;
    default:
      builder[builder_len++] = curr_char;
    }
  }

  if (builder_len > 0) {
    builder[builder_len] = '\0';
    args[args_count++] = strdup(builder);
  }

  args[args_count] = NULL; // NULL-terminate the array

  free(builder);

  return args;
}

void free_tokens(char **tokens) {
  if (tokens == NULL) {
    return;
  }

  for (int i = 0; tokens[i] != NULL; i++) {
    free(tokens[i]);
  }
  free(tokens);
}

char *find_executable(char *program_name) {
  const char *path_env = getenv("PATH");
  if (path_env == NULL) {
    return NULL;
  }
  char *path_copy = strdup(path_env);
  if (path_copy == NULL) {
    return NULL;
  }

  char *directory;
  char *remaining = path_copy;
  while ((directory = strsep(&remaining, ":")) != NULL) {
    // Skip empty directories (consecutive colons)
    if (*directory == '\0') {
      continue;
    }

    char *exec_path;
    if (asprintf(&exec_path, "%s/%s", directory, program_name) == -1) {
      free(path_copy);
      return NULL;
    }

    if (access(exec_path, F_OK | X_OK) == 0) {
      free(path_copy);
      return exec_path;
    }

    free(exec_path);
  }

  free(path_copy);

  return NULL;
}

void run_external(char **args, Redirection *redir) {
  char *program_name = args[0];
  if (program_name == NULL) {
    return;
  }

  char *exec_path = find_executable(program_name);
  if (exec_path == NULL) {
    printf("%s: not found\n", program_name);
    return;
  }

  pid_t pid = fork();
  if (pid < 0) { // fork failed
    printf("An error has occurred\n");
    free(exec_path);
    return;
  }

  if (pid == 0) { // child process
    if (redir->target_filename != NULL) {
      int fd = open(redir->target_filename, redir->open_flags, 0644);
      dup2(fd, redir->target_fd);
      close(fd);
    }

    execv(exec_path, args);
    exit(1);
  }

  // parent process
  free(exec_path);
  wait(NULL);
}

Command parse_command(char **tokens) {
  Command cmd = {0};
  int args_cap = 10;
  int args_count = 0;
  cmd.args = malloc(args_cap * sizeof(char *));

  for (int i = 0; tokens[i] != NULL; i++) {
    if (strcmp(tokens[i], ">") == 0 || strcmp(tokens[i], "1>") == 0) {
      if (tokens[i + 1] != NULL) {
        cmd.redir.target_fd = STDOUT_FILENO;
        cmd.redir.target_filename = strdup(tokens[i + 1]);
        cmd.redir.open_flags = O_WRONLY | O_CREAT | O_TRUNC;
        break;
      }
    } else if (strcmp(tokens[i], "2>") == 0) {
      if (tokens[i + 1] != NULL) {
        cmd.redir.target_fd = STDERR_FILENO;
        cmd.redir.target_filename = strdup(tokens[i + 1]);
        cmd.redir.open_flags = O_WRONLY | O_CREAT | O_TRUNC;
        break;
      }
    } else if (strcmp(tokens[i], ">>") == 0 || strcmp(tokens[i], "1>>") == 0) {
      if (tokens[i + 1] != NULL) {
        cmd.redir.target_fd = STDOUT_FILENO;
        cmd.redir.target_filename = strdup(tokens[i + 1]);
        cmd.redir.open_flags = O_WRONLY | O_CREAT | O_APPEND;
        break;
      }
    } else if (strcmp(tokens[i], "2>>") == 0) {
      if (tokens[i + 1] != NULL) {
        cmd.redir.target_fd = STDERR_FILENO;
        cmd.redir.target_filename = strdup(tokens[i + 1]);
        cmd.redir.open_flags = O_WRONLY | O_CREAT | O_APPEND;
        break;
      }
    } else {
      if (args_count + 1 >= args_cap) {
        args_cap *= 2;
        cmd.args = realloc(cmd.args, args_cap * sizeof(char *));
      }

      cmd.args[args_count++] = strdup(tokens[i]);
    }
  }

  cmd.args[args_count] = NULL; // NULL-terminate the array

  return cmd;
}

void free_command(Command *cmd) {
  if (cmd == NULL) {
    return;
  }

  for (int i = 0; cmd->args[i] != NULL; i++) {
    free(cmd->args[i]);
  }
  free(cmd->args);
  free(cmd->redir.target_filename);
}

void evaluate_command(Command *cmd) {
  // Check for empty input
  if (cmd->args == NULL || cmd->args[0] == NULL) {
    return;
  }

  for (int i = 0; i < sizeof(builtins) / sizeof(builtins[0]); i++) {
    if (strcmp(cmd->args[0], builtins[i].name) == 0) {
      if (cmd->redir.target_filename != NULL) {
        int copy = dup(cmd->redir.target_fd);
        int fd = open(cmd->redir.target_filename, cmd->redir.open_flags, 0644);
        dup2(fd, cmd->redir.target_fd);
        builtins[i].handler(cmd->args);
        dup2(copy, cmd->redir.target_fd);
        close(copy);
        close(fd);
      } else {
        builtins[i].handler(cmd->args);
      }

      return;
    }
  }

  run_external(cmd->args, &cmd->redir);
}
