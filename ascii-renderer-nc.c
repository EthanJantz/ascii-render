#include <assert.h>
#include <ctype.h>
#include <curl/curl.h>
#include <math.h>
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

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

  char error[256];
  bool should_rerender;
  bool should_resize;
  bool should_quit;
} AppState;

// App
CURLcode setup_curl(AppState *state);
void setup_ncurses();
AppState init_state();
void update(AppState *state, int event);
void render(AppState *state);
void teardown(AppState *state);
void reinit_windows(AppState *state);

// CURL
bool is_valid_url(UserInput *buf);
static size_t write_cb(char *contents, size_t size, size_t nmemb, void *userp);

// ASCII
int read_img(char *filename, Image *img);
ASCIIRender generate_ascii_render(char *out, size_t out_size, short *pairs,
                                  size_t pairs_size, int cols, int rows);
char get_char_from_lightness(float lightness);
int quantize(unsigned char val);
ASCIIRender convert_to_ascii(Image *img, int block_width, int samples);

// Input
void clear_input(UserInput *buf);
void update_input(UserInput *buf, int in);

// Constants
char ASCII[10] = {' ', '.', ':', '-', '=', '+', '*', '#', '%', '@'};
int N_ASCII = sizeof(ASCII) / sizeof(ASCII[0]);
int BLOCK_ASPECT_RATIO = 2;
int SAMPLES = 4;
int CHANNELS = 3;

CURLcode setup_curl(AppState *state) {
  CURLcode result;
  result = curl_global_init(CURL_GLOBAL_ALL);
  if (result != CURLE_OK) {
    return result;
  }
  state->curl = curl_easy_init();
  return result;
}

void setup_ncurses() {
  srand(time(0));
  set_escdelay(25);
  initscr();
  curs_set(0);
  cbreak();
  noecho();
  keypad(stdscr, TRUE);
  if (has_colors()) {
    start_color();
    use_default_colors();
    // initialize colorspace
    for (int i = 0; i < 216; i++) {
      init_pair(i + 1, i + 16, -1); // pair 1–216 → color 16–231
    }
  }
  refresh();
}

AppState init_state() {
  int sx, sy;
  getmaxyx(stdscr, sy, sx);

  int input_w, input_h, input_x, input_y, ascii_w, ascii_h, ascii_x, ascii_y,
      margin;
  margin = 1; // 1 char
  input_w = sx - (margin * 2);
  input_h = 3;
  input_x = margin;
  input_y = margin;

  ascii_w = sx - (margin * 2);
  ascii_h = sy * .9 - (margin * 2);
  ascii_x = margin;
  ascii_y = input_y + input_h;

  WINDOW *input_win = newwin(input_h, input_w, input_y, input_x);
  wborder(input_win, 0, 0, 0, 0, 0, 0, 0, 0);

  WINDOW *ascii_win = newwin(ascii_h, ascii_w, ascii_y, ascii_x);
  wborder(ascii_win, 0, 0, 0, 0, 0, 0, 0, 0);

  ASCIIRender r = {0};
  UserInput in = {0};

  AppState state = {.term_w = sx,
                    .term_h = sy,
                    .input_win = input_win,
                    .ascii_win = ascii_win,
                    .cur_render = r,
                    .user_input = in,
                    .should_rerender = true,
                    .should_resize = false,
                    .should_quit = false};

  return state;
}

void update(AppState *state, int event) {
  switch (event) {
  case 23:
    clear_input(&state->user_input);
    state->should_rerender = 1;
    break;
  case 27:
    state->should_quit = 1;
    return;
  case '\n':
  case '\r':
    state->should_rerender = 1;
    // Check input and read image to state
    if (state->user_input.len > 0) {
      if (is_valid_url(&state->user_input) && state->curl) {

        curl_easy_setopt(state->curl, CURLOPT_WRITEFUNCTION, write_cb);
        curl_easy_setopt(state->curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");
        curl_easy_setopt(state->curl, CURLOPT_URL, state->user_input.buf);
        curl_easy_setopt(state->curl, CURLOPT_WRITEDATA, (void *)&state->chunk);
        CURLcode result = curl_easy_perform(state->curl);

        if (result != CURLE_OK) {
          werase(state->ascii_win);
          wborder(state->ascii_win, 0, 0, 0, 0, 0, 0, 0, 0);
          mvwprintw(state->ascii_win, 2, 1, "curl_easy_perform() failed %s\n",
                    curl_easy_strerror(result));
          wrefresh(state->ascii_win);
          return;
        } else {
          if (state->img.img != 0)
            stbi_image_free(state->img.img);

          state->img.img = stbi_load_from_memory(
              state->chunk.memory, state->chunk.size, &state->img.width,
              &state->img.height, &state->img.channels, CHANNELS);
        }
      }
    }

    // Calculate maximum block width for render to fit in window
    int ascii_w, ascii_h;
    getmaxyx(state->ascii_win, ascii_h, ascii_w);
    int interior_h = ascii_h - 2;
    int interior_w = ascii_w - 2;
    int denom_h = BLOCK_ASPECT_RATIO * interior_h;
    // ceil idiom: ceil(x / y) == (x + y - 1) / y
    int bw_w = (state->img.width + interior_w - 1) / interior_w;
    int bw_h = (state->img.height + denom_h - 1) / denom_h;
    int bw = bw_h > bw_w ? bw_h : bw_w;
    if (bw == 0)
      bw = 1;

    if (state->cur_render.buf != 0)
      free(state->cur_render.buf);
    if (state->cur_render.pairs != 0)
      free(state->cur_render.pairs);

    state->cur_render = convert_to_ascii(&state->img, bw, SAMPLES);
    if (state->chunk.memory != NULL)
      state->chunk.memory = NULL;
    if (state->chunk.size > 0)
      state->chunk.size = 0;
    return;
  case KEY_RESIZE:
    state->should_resize = 1;
    state->should_rerender = 1;
    break;
  default:
    update_input(&state->user_input, event);
    state->should_rerender = 1;
    break;
  }

  return;
}

void render(AppState *state) {
  if (!state->should_rerender)
    return;

  if (state->should_resize) {
    reinit_windows(state);
    state->should_resize = false;
  }

  int ascii_w, ascii_h;
  getmaxyx(state->ascii_win, ascii_h, ascii_w);
  int interior_h = ascii_h - 2;
  int interior_w = ascii_w - 2;

  // center render if necessary
  int offset_h = 1;
  int offset_w = 1;
  if (state->cur_render.cols < interior_w) {
    offset_w = (interior_w - state->cur_render.cols) / 2;
    offset_w = offset_w == 0 ? 1 : offset_w;
  }

  if (state->cur_render.rows < interior_h) {
    offset_h = (interior_h - state->cur_render.rows) / 2;
    offset_h = offset_h == 0 ? 1 : offset_h;
  }

  mvwprintw(state->input_win, 1, 1, "%s", state->user_input.buf);
  mvwprintw(state->input_win, 1, 1, "%-*s", state->term_w - 4,
            state->user_input.buf);
  wrefresh(state->input_win);

  werase(state->ascii_win);
  wborder(state->ascii_win, 0, 0, 0, 0, 0, 0, 0, 0);
  for (int i = 0; i < state->cur_render.rows; i++) {
    for (int j = 0; j < state->cur_render.cols; j++) {
      short pair = state->cur_render.pairs[i * state->cur_render.cols + j];
      wattron(state->ascii_win, COLOR_PAIR(pair));
      mvwaddch(state->ascii_win, offset_h + i, offset_w + j,
               state->cur_render.buf[i * (state->cur_render.cols + 1) + j]);
      wattroff(state->ascii_win, COLOR_PAIR(pair));
    }
  }
  wrefresh(state->ascii_win);

  werase(state->input_win);
  wborder(state->input_win, 0, 0, 0, 0, 0, 0, 0, 0);
  mvwprintw(state->input_win, 1, 1, "%s", state->user_input.buf);
  wrefresh(state->input_win);
  state->should_rerender = 0;
}

void reinit_windows(AppState *state) {
  int sx, sy;
  getmaxyx(stdscr, sy, sx);

  int input_w, input_h, input_x, input_y, ascii_w, ascii_h, ascii_x, ascii_y,
      margin;
  margin = 1; // 1 char
  input_w = sx - (margin * 2);
  input_h = 3;
  input_x = margin;
  input_y = margin;

  ascii_w = sx - (margin * 2);
  ascii_h = sy * .9 - (margin * 2);
  ascii_x = margin;
  ascii_y = input_y + input_h;

  WINDOW *input_win = newwin(input_h, input_w, input_y, input_x);
  wborder(input_win, 0, 0, 0, 0, 0, 0, 0, 0);
  mvwprintw(input_win, 1, 1, "pos: %dx%d size: %dwx%dh", input_x, input_y,
            input_w, input_h);

  WINDOW *ascii_win = newwin(ascii_h, ascii_w, ascii_y, ascii_x);
  wborder(ascii_win, 0, 0, 0, 0, 0, 0, 0, 0);
  mvwprintw(ascii_win, 1, 1, "pos: %dx%d size: %dwx%dh", ascii_x, ascii_y,
            ascii_w, ascii_h);

  state->term_w = sx;
  state->term_h = sy;
  delwin(state->ascii_win);
  state->ascii_win = ascii_win;
  delwin(state->input_win);
  state->input_win = input_win;
}

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

int read_img(char *filename, Image *img) {
  img->img =
      stbi_load(filename, &img->width, &img->height, &img->channels, CHANNELS);
  if (img->img == NULL) {
    return 1;
  }
  return 0;
}

void teardown(AppState *state) {
  free(state->cur_render.pairs);
  free(state->chunk.memory);
  free(state->cur_render.buf);
  stbi_image_free(state->img.img);
  curl_easy_cleanup(state->curl);
  curl_global_cleanup();
  delwin(state->input_win);
  delwin(state->ascii_win);
  endwin();
}

char get_char_from_lightness(float lightness) {
  int idx = floor(lightness * (N_ASCII - 1));
  return ASCII[idx];
}

int quantize(unsigned char val) { return (int)round(val * 5.0 / 255.0); }

ASCIIRender generate_ascii_render(char *out, size_t out_size, short *pairs,
                                  size_t pairs_size, int cols, int rows) {
  ASCIIRender render = {
      .buf = out, .pairs = pairs, .rows = rows, .cols = cols, .size = out_size};
  return render;
}

ASCIIRender convert_to_ascii(Image *img, int block_width, int samples) {
  int block_height =
      block_width *
      BLOCK_ASPECT_RATIO; // monospaced term chars are 2:1 height/width
  int cols = img->width / block_width;
  int rows = img->height / block_height;

  size_t out_size = cols * rows + rows;
  char *out = malloc(out_size);

  unsigned char r, g, b;
  float avg_r, avg_g, avg_b;
  int ri, gi, bi, pair_id;

  size_t pairs_size = cols * rows;
  short *pairs = malloc(pairs_size * sizeof(short));

  float rel_lum;
  float avg_rel_lum;

  for (int row = 0; row < rows; row++) {
    for (int col = 0; col < cols; col++) {
      avg_r = 0.0, avg_g = 0.0, avg_b = 0.0;
      avg_rel_lum = 0.0;

      for (int s = 1; s <= samples; s++) {
        int x = col * block_width + (s * block_width) / (s + 1);
        int y = row * block_height + (s * block_height) / (s + 1);

        r = img->img[(y * img->width + x) * 3 + 0];
        g = img->img[(y * img->width + x) * 3 + 1];
        b = img->img[(y * img->width + x) * 3 + 2];
        rel_lum = ((r * 0.2126) + (g * 0.7152) + (b * 0.0722)) / 255;

        avg_r += r;
        avg_g += g;
        avg_b += b;
        avg_rel_lum += rel_lum;
      }

      avg_r /= samples;
      avg_g /= samples;
      avg_b /= samples;
      avg_rel_lum /= samples;

      ri = quantize(avg_r);
      gi = quantize(avg_g);
      bi = quantize(avg_b);
      pair_id = 1 + (36 * ri) + (6 * gi) + bi;
      pairs[row * cols + col] = pair_id;

      out[row * (cols + 1) + col] = get_char_from_lightness(avg_rel_lum);

      if (col == cols - 1) {
        out[row * (cols + 1) + cols] = '\n';
      }
    }
  }

  ASCIIRender render =
      generate_ascii_render(out, out_size, pairs, pairs_size, cols, rows);
  return render;
}

bool is_valid_url(UserInput *buf) {
  CURLU *url = curl_url();
  CURLUcode rc = curl_url_set(url, CURLUPART_URL, buf->buf, 0);
  bool valid = false;
  char *scheme = NULL;

  if (rc) {
    printf("Couldn't parse URL\n");
    curl_url_cleanup(url);
    return valid;
  }

  rc = curl_url_get(url, CURLUPART_SCHEME, &scheme, 0);
  if (scheme == NULL) {
    curl_url_cleanup(url);
    return valid;
  }

  if (!strcmp("https", scheme) || !strcmp("http", scheme))
    valid = true;
  curl_free(scheme);
  curl_url_cleanup(url);
  return valid;
}

void clear_input(UserInput *buf) {
  buf->buf[buf->len] = '\0';
  buf->len = 0;
}

void update_input(UserInput *buf, int in) {
  if (in == KEY_BACKSPACE || in == 127 || in == 8) {
    if (buf->len == 0)
      return;
    --buf->len;
    buf->buf[buf->len] = '\0';
    return;
  }

  if (!isprint(in) || buf->len >= sizeof(buf->buf) - 1)
    return;

  buf->buf[buf->len] = in;
  ++buf->len;
  buf->buf[buf->len] = '\0';
  return;
}

int main() {
  setup_ncurses();
  AppState state = init_state();
  setup_curl(&state);
  while (!state.should_quit) {
    render(&state);
    int key = getch();
    update(&state, key);
  }
  teardown(&state);
}
