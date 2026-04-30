// Minimal example for downloading an image to memory via libcurl
#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

typedef struct {
  unsigned char *img;
  int width;
  int height;
  int channels;
} Image;

// Small memory struct for whatever is downloaded
typedef struct MemoryStruct {
  unsigned char *memory; // This is the buffer to read from
  size_t size;
} MemoryStruct;

static size_t write_cb(char *contents, size_t size, size_t nmemb, void *userp) {
  size_t realsize = size * nmemb;
  MemoryStruct *mem = (MemoryStruct *)userp;

  unsigned char *ptr = realloc(mem->memory, mem->size + realsize + 1);
  if (!ptr) {
    printf("Not enough memory (realloc returned NULL)\n");
    return 0;
  }

  mem->memory = ptr;
  memcpy(&(mem->memory[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;

  return realsize;
}

int main() {
  // curl setup
  CURL *curl;
  CURLcode result;
  result = curl_global_init(CURL_GLOBAL_ALL);
  if (result != CURLE_OK) {
    return (int)result;
  }
  curl = curl_easy_init();

  // prepare memory to allocate
  MemoryStruct chunk;
  chunk.memory = malloc(1);
  chunk.size = 0;

  // pull the data
  char buf[256] = "https://upload.wikimedia.org/wikipedia/commons/1/19/"
                  "Under_construction_graphic.gif";
  if (curl) {
    curl_easy_setopt(curl, CURLOPT_URL, buf);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");
    result = curl_easy_perform(curl);

    if (result != CURLE_OK) {
      fprintf(stderr, "curl_easy_perform() failed %s\n",
              curl_easy_strerror(result));
    } else {
      printf("%lu bytes retrieved\n", (unsigned long)chunk.size);
    }
  }

  // read into Image struct
  Image img;
  img.img = stbi_load_from_memory(chunk.memory, chunk.size, &img.width,
                                  &img.height, &img.channels, 4);
  if (img.img == NULL) {
    return 1;
  } else {
    printf("Successfully loaded img from memory");
  }

  // internal teardown
  free(img.img);
  free(chunk.memory);

  // curl teardown
  curl_easy_cleanup(curl);
  curl_global_cleanup();
  return (int)result;
}
