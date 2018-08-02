#define _POSIX_C_SOURCE 199309L
#ifdef __linux__
#include <linux/input-event-codes.h>
#elif __FreeBSD__
#include <dev/evdev/input-event-codes.h>
#endif
#include <GLES2/gl2.h>
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <wayland-client.h>
#include <wayland-egl.h>
#define namespace namespace_
extern "C" {
  #include "wlr-layer-shell-unstable-v1-client-protocol.h"
  #include <wlr/render/egl.h>
  #include <wlr/util/log.h>
  #include "xdg-shell-client-protocol.h"
}
#undef namespace

static struct wl_display* display;
static struct wl_compositor* compositor;
static struct wl_shm* shm;
static struct xdg_wm_base* xdg_wm_base;
static struct zwlr_layer_shell_v1* layer_shell;

struct zwlr_layer_surface_v1* layer_surface;
struct wl_surface* wl_surface;
struct wlr_egl egl;
struct wl_egl_window* egl_window;
struct wlr_egl_surface* egl_surface;
struct wl_callback* frame_callback;

static uint32_t layer = ZWLR_LAYER_SHELL_V1_LAYER_TOP;
static uint32_t anchor = 0;
static uint32_t width = 256, height = 256;
static int32_t margin_top = 0;
static double alpha = 1.0;
static bool run_display = true;
static bool animate = false;
static bool keyboard_interactive = false;
static double frame = 0;
static int cur_x = -1, cur_y = -1;
static int buttons = 0;

static struct {
  struct timespec last_frame;
  float color[3];
  int dec;
} demo;

static void draw(void);

static void surface_frame_callback(void* data, struct wl_callback* cb, uint32_t time)
{
  wl_callback_destroy(cb);
  frame_callback = NULL;
  draw();
}

static struct wl_callback_listener frame_listener = {.done = surface_frame_callback};

static void draw(void)
{
  eglMakeCurrent(egl.display, egl_surface, egl_surface, egl.context);
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);

  long ms =
    (ts.tv_sec - demo.last_frame.tv_sec) * 1000 + (ts.tv_nsec - demo.last_frame.tv_nsec) / 1000000;
  int inc = (demo.dec + 1) % 3;

  if (!buttons) {
    demo.color[inc] += ms / 2000.0f;
    demo.color[demo.dec] -= ms / 2000.0f;

    if (demo.color[demo.dec] < 0.0f) {
      demo.color[inc] = 1.0f;
      demo.color[demo.dec] = 0.0f;
      demo.dec = inc;
    }
  }

  if (animate) {
    frame += ms / 50.0;
    int32_t old_top = margin_top;
    margin_top = -(20 - ((int) frame % 20));
    if (old_top != margin_top) {
      zwlr_layer_surface_v1_set_margin(layer_surface, margin_top, 0, 0, 0);
      wl_surface_commit(wl_surface);
    }
  }

  glViewport(0, 0, width, height);
  if (buttons) {
    glClearColor(1, 1, 1, alpha);
  } else {
    glClearColor(demo.color[0], demo.color[1], demo.color[2], alpha);
  }
  glClear(GL_COLOR_BUFFER_BIT);

  if (cur_x != -1 && cur_y != -1) {
    glEnable(GL_SCISSOR_TEST);
    glScissor(cur_x, height - cur_y, 5, 5);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);
  }

  frame_callback = wl_surface_frame(wl_surface);
  wl_callback_add_listener(frame_callback, &frame_listener, NULL);

  eglSwapBuffers(egl.display, egl_surface);

  demo.last_frame = ts;
}

static void layer_surface_configure(void* data,
                                    struct zwlr_layer_surface_v1* surface,
                                    uint32_t serial,
                                    uint32_t w,
                                    uint32_t h)
{
  width = w;
  height = h;
  if (egl_window) {
    wl_egl_window_resize(egl_window, width, height, 0, 0);
  }
  zwlr_layer_surface_v1_ack_configure(surface, serial);
}

static void layer_surface_closed(void* data, struct zwlr_layer_surface_v1* surface)
{
  wlr_egl_destroy_surface(&egl, egl_surface);
  wl_egl_window_destroy(egl_window);
  zwlr_layer_surface_v1_destroy(surface);
  wl_surface_destroy(wl_surface);
  run_display = false;
}

struct zwlr_layer_surface_v1_listener layer_surface_listener = {
  .configure = layer_surface_configure,
  .closed = layer_surface_closed,
};

static void handle_global(void* data,
                          struct wl_registry* registry,
                          uint32_t name,
                          const char* interface,
                          uint32_t version)
{
  if (strcmp(interface, wl_compositor_interface.name) == 0) {
    compositor = (wl_compositor*) wl_registry_bind(registry, name, &wl_compositor_interface, 1);
  } else if (strcmp(interface, wl_shm_interface.name) == 0) {
    shm = (wl_shm*) wl_registry_bind(registry, name, &wl_shm_interface, 1);
  } else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
    layer_shell = (zwlr_layer_shell_v1*) wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface, 1);
  } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
    xdg_wm_base = (struct xdg_wm_base*) wl_registry_bind(registry, name, &xdg_wm_base_interface, 1);
  }
}

static void handle_global_remove(void* data, struct wl_registry* registry, uint32_t name)
{
  // who cares
}

static const struct wl_registry_listener registry_listener = {
  .global = handle_global,
  .global_remove = handle_global_remove,
};

int main(int argc, char** argv)
{
  wlr_log_init(WLR_DEBUG, NULL);
  const char* _namespace = "wlroots";
  int exclusive_zone = 0;
  int32_t margin_right = 0, margin_bottom = 0, margin_left = 0;

  display = wl_display_connect(NULL);
  if (display == NULL) {
    fprintf(stderr, "Failed to create display\n");
    return 1;
  }

  struct wl_registry* registry = wl_display_get_registry(display);
  wl_registry_add_listener(registry, &registry_listener, NULL);
  wl_display_roundtrip(display);

  if (compositor == NULL) {
    fprintf(stderr, "wl_compositor not available\n");
    return 1;
  }
  if (shm == NULL) {
    fprintf(stderr, "wl_shm not available\n");
    return 1;
  }
  if (layer_shell == NULL) {
    fprintf(stderr, "layer_shell not available\n");
    return 1;
  }

  EGLint attribs[] = {EGL_ALPHA_SIZE, 8, EGL_NONE};
  wlr_egl_init(&egl, EGL_PLATFORM_WAYLAND_EXT, display, attribs, WL_SHM_FORMAT_ARGB8888);

  wl_surface = wl_compositor_create_surface(compositor);
  assert(wl_surface);

  layer_surface =
    zwlr_layer_shell_v1_get_layer_surface(layer_shell, wl_surface, nullptr, layer, _namespace);
  assert(layer_surface);
  zwlr_layer_surface_v1_set_size(layer_surface, width, height);
  zwlr_layer_surface_v1_set_anchor(layer_surface, anchor);
  zwlr_layer_surface_v1_set_exclusive_zone(layer_surface, exclusive_zone);
  zwlr_layer_surface_v1_set_margin(layer_surface, margin_top, margin_right, margin_bottom,
                                   margin_left);
  zwlr_layer_surface_v1_set_keyboard_interactivity(layer_surface, keyboard_interactive);
  zwlr_layer_surface_v1_add_listener(layer_surface, &layer_surface_listener, layer_surface);
  wl_surface_commit(wl_surface);
  wl_display_roundtrip(display);

  egl_window = wl_egl_window_create(wl_surface, width, height);
  assert(egl_window);
  egl_surface = (struct wlr_egl_surface*) wlr_egl_create_surface(&egl, egl_window);
  assert(egl_surface);

  wl_display_roundtrip(display);
  draw();

  while (wl_display_dispatch(display) != -1 && run_display) {
    // This space intentionally left blank
  }

  return 0;
}
