#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
  if (argc != 3) {
    printf("Usage: filecopy <input> <output>\n");
    return 1;
  }

  FILE *fi = fopen(argv[1], "rb");
  if (fi == NULL) {
    printf("Couldn't open file\n");
    return 1;
  }

  fseek(fi, 0, SEEK_END);
  long file_size = ftell(fi);
  rewind(fi);

  unsigned char *data = malloc(file_size);
  if (data == NULL) {
    printf("malloc failed\n");
    fclose(fi);
    return 1;
  }

  size_t bytes_read = fread(data, 1, file_size, fi);
  printf("Read %zu bytes\n", bytes_read);

  fclose(fi);

  FILE *fo = fopen(argv[2], "wb");
  if (fo == NULL) {
    printf("Couldn't open file\n");
    free(data);
    return 1;
  }

  size_t bytes_written = fwrite(data, 1, file_size, fo);
  if (bytes_written != bytes_read) {
    printf("Write error\n");
  }

  fclose(fo);
  free(data);
  return 0;
}
