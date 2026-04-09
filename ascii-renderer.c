#include <stdio.h>
#include <stdlib.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

int main(int argc, char *argv[]) {

  if (argc != 2) {
    printf("Usage: ascii-render <file>\n");
    return 1;
  }

  int width, height, channels;
  unsigned char *img = stbi_load(argv[1], &width, &height, &channels, 1);
  if (img == NULL) {
    printf("Could not load image\n");
    return 1;
  }

  printf("Loaded: %dx%d, %d channels\n", width, height, channels);

  stbi_image_free(img);
}
