#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

char getCharFromLightness(float lightness);

char ASCII[10] = {' ', '.', ':', '-', '=', '+', '*', '#', '%', '@'};
int N_ASCII = sizeof(ASCII) / sizeof(ASCII[0]);

char getCharFromLightness(float lightness) {
  int idx = floor(lightness * (N_ASCII - 1));
  return ASCII[idx];
}

int main(int argc, char *argv[]) {

  if (argc != 3) {
    printf("Usage: ascii-render <file>\n");
    return 1;
  }

  int width, height, channels;
  unsigned char *img = stbi_load(argv[1], &width, &height, &channels, 3);
  if (img == NULL) {
    printf("Could not load image\n");
    return 1;
  }

  printf("Loaded: %dx%d, %d channels\n", width, height, channels);

  int block_width = 4;
  int block_height = 8;
  int cols = width / block_width;
  int rows = height / block_height;

  size_t out_size = cols * rows + rows + 1;
  char *out = malloc(out_size);

  unsigned char r, g, b;
  float rel_lum;

  for (int row = 0; row < rows; row++) {
    for (int col = 0; col < cols; col++) {
      for (int ch = 0; ch < channels; ch++) {

        int center_x = col * block_width + block_width / 2;
        int center_y = row * block_height + block_height / 2;

        if (ch == 0) {
          r = img[(center_y * width + center_x) * 3 + ch];
        } else if (ch == 1) {
          g = img[(center_y * width + center_x) * 3 + ch];
        } else {
          b = img[(center_y * width + center_x) * 3 + ch];
          rel_lum = ((r * 0.2126) + (g * 0.7152) + (b * 0.0722)) / 255;
        }
      }
      printf("Relative luminance: %f\n", rel_lum);

      if (rel_lum < 0.08) {
        out[row * (cols + 1) + col] = ' ';
      } else if (rel_lum < 0.09) {
        out[row * (cols + 1) + col] = '.';
      } else {
        out[row * (cols + 1) + col] = '@';
      }

      if (col == cols - 1) {
        out[row * (cols + 1) + cols] = '\n';
      }
    }
  }

  FILE *fo = fopen(argv[2], "wb");
  if (fo == NULL) {
    printf("Couldn't open file\n");
    free(out);
    return 1;
  }

  size_t bytes_written = fwrite(out, 1, out_size, fo);
  printf("Wrote %zu bytes\n", bytes_written);
  fclose(fo);
  free(out);
  stbi_image_free(img);
}
