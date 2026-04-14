#include <stdio.h>
#include <stdlib.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

int main(int argc, char *argv[]) {

  if (argc != 3) {
    printf("Usage: downsampler <file> <out-file>\n");
    return 1;
  }

  int width, height, channels;
  unsigned char *img = stbi_load(argv[1], &width, &height, &channels, 4);
  if (img == NULL) {
    printf("Could not load image\n");
    return 1;
  }

  printf("Loaded: %dx%d, %d channels\n", width, height, channels);

  int block_width = 8;
  int block_height = 8;
  int cols = width / block_width;
  int rows = height / block_height;

  size_t out_size = cols * rows * channels;
  unsigned char *out = malloc(out_size);

  unsigned char r, g, b;
  float rel_lum;

  for (int row = 0; row < rows; row++) {
    for (int col = 0; col < cols; col++) {
      for (int ch = 0; ch < channels; ch++) {

        int center_x = col * block_width + block_width / 2;
        int center_y = row * block_height + block_height / 2;

        out[(row * cols + col) * channels + ch] =
            img[(center_y * width + center_x) * channels + ch];
      }
    }
  }

  if (!stbi_write_png(argv[2], cols, rows, channels, out, cols * channels)) {
    printf("failed to write %s\n", argv[2]);
  }
  free(out);
  stbi_image_free(img);
}
