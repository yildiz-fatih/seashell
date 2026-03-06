#include <stdio.h>
#include <stdlib.h>

char *read_line();

int main(void) {
  while (1) {
    printf("$ ");

    char *line = read_line();
    if (line == NULL) {
      break;
    }

    // for testing
    printf("%s\n", line);

    free(line);
  }
  return 0;
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
