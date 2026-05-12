#ifndef ASCII_RENDERER_NC_H 
#define ASCII_RENDERER_NC_H

#include <curl/curl.h>
#include <ncurses.h>
#include <stdlib.h>

typedef struct {
  char *buf;
  short *pairs;
  int rows;
  int cols;
  size_t size;
} ASCIIRender;

typedef struct {
  unsigned char *img;
  int width;
  int height;
  int channels;
} Image;

typedef struct {
  char buf[256];
  int len;
} UserInput;

typedef struct {
  unsigned char *memory;
  size_t size;
} MemoryStruct;

typedef struct {
  int term_w, term_h;
  WINDOW *input_win, *ascii_win;

  CURL *curl;
  MemoryStruct chunk;
  Image img;
  ASCIIRender cur_render;
  UserInput user_input;

  bool should_rerender;
  bool should_resize;
  bool should_quit;
} AppState;

// App
CURLcode setup_curl(AppState *state);
void setup_ncurses();
void init_state(AppState *state);
void welcome_message(AppState *state);
void update(AppState *state, int event);
void render(AppState *state);
void teardown(AppState *state);
void reinit_windows(AppState *state);

// CURL
bool is_valid_url(UserInput *buf);
static size_t write_cb(char *contents, size_t size, size_t nmemb, void *userp);

// ASCII
int read_img(char *filename, Image *img);
char get_char_from_lightness(float lightness);
int quantize(unsigned char val);
ASCIIRender convert_to_ascii(Image *img, int block_width, int samples);

// Input
void clear_input(UserInput *buf);
void update_input(UserInput *buf, int in);

#endif
