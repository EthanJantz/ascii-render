#include <stdio.h>
#include <stdlib.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

const int BLOCK_WIDTH = 8;
const int BLOCK_HEIGHT = 8;

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

  int cols = width / BLOCK_WIDTH;
  int rows = height / BLOCK_HEIGHT;

  size_t downsampled_size = cols * rows * channels;
  unsigned char *downsampled = malloc(downsampled_size);

  unsigned char r, g, b;
  float rel_lum;

  for (int row = 0; row < rows; row++) {
    for (int col = 0; col < cols; col++) {
      for (int ch = 0; ch < channels; ch++) {

        int center_x = col * BLOCK_WIDTH + BLOCK_WIDTH / 2;
        int center_y = row * BLOCK_HEIGHT + BLOCK_HEIGHT / 2;

        downsampled[(row * cols + col) * channels + ch] =
            img[(center_y * width + center_x) * channels + ch];
      }
    }
  }

  size_t out_size = width * height * channels;
  unsigned char *out = malloc(out_size);

  for (int row = 0; row < height; row++) {
    for (int col = 0; col < width; col++) {
      for (int ch = 0; ch < channels; ch++) {
        // index into downsampled and expand each pixel to the size of the block
      }
    }
  }

  if (!stbi_write_png(argv[2], cols, rows, channels, out, cols * channels)) {
    printf("failed to write %s\n", argv[2]);
  }

  free(downsampled);
  free(out);
  stbi_image_free(img);
}
