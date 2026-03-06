#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void handle_echo(char **args);

char *read_line();
char **tokenize_line(char *line);
void free_tokens(char **tokens);

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

    if (strcmp(tokens[0], "exit") == 0) {
      free_tokens(tokens);
      free(line);
      exit(0);
    } else if (strcmp(tokens[0], "echo") == 0) {
      handle_echo(tokens);
    } else {
      printf("%s: command not found\n", tokens[0]);
    }

    free_tokens(tokens);
    free(line);
  }
  return 0;
}

void handle_echo(char **args) {
  for (int i = 0; args[i] != NULL; i++) {
    printf("%s", args[i]);

    if (args[i + 1] != NULL) {
      printf(" ");
    }
  }
  printf("\n");
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
