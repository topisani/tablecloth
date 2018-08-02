#define _POSIX_C_SOURCE 199309L
#ifdef __linux__
#include <linux/input-event-codes.h>
#elif __FreeBSD__
#include <dev/evdev/input-event-codes.h>
#endif
#include <GLES2/gl2.h>
#include <assert.h>
#include <gdk/gdkwayland.h>
#include <gtk/gtk.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <wayland-client.h>

#define namespace namespace_
extern "C" {
#include <wlr/util/log.h>
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
}
#undef namespace

static struct wl_display* display;
static struct zwlr_layer_shell_v1* layer_shell;

struct zwlr_layer_surface_v1* layer_surface;
struct wl_surface* wl_surface;
struct wl_callback* frame_callback;

static uint32_t layer = ZWLR_LAYER_SHELL_V1_LAYER_TOP;
static bool run_display = true;

GtkWidget* window;

static void layer_surface_configure(void* data,
                                    struct zwlr_layer_surface_v1* surface,
                                    uint32_t serial,
                                    uint32_t w,
                                    uint32_t h)
{
  if (window) {
    gtk_window_resize(GTK_WINDOW(window), w, h);
  }
  zwlr_layer_surface_v1_ack_configure(surface, serial);
}

static void layer_surface_closed(void* data, struct zwlr_layer_surface_v1* surface)
{
  gtk_window_close(GTK_WINDOW(window));
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
  if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
    layer_shell =
      (zwlr_layer_shell_v1*) wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface, 1);
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

  gtk_init(&argc, &argv);

  GdkDisplay* gdk_display = gdk_display_get_default();

  display = gdk_wayland_display_get_wl_display(gdk_display);
  if (display == NULL) {
    fprintf(stderr, "Failed to create display\n");
    return 1;
  }

  struct wl_registry* registry = wl_display_get_registry(display);
  wl_registry_add_listener(registry, &registry_listener, NULL);
  wl_display_roundtrip(display);

  GtkWidget *box1, *button;

  window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

  gtk_window_set_title(GTK_WINDOW(window), "gtk shell");
  gtk_window_set_decorated(GTK_WINDOW(window), FALSE);
  gtk_widget_realize(window);

  box1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_container_add(GTK_CONTAINER(window), box1);

  button = gtk_button_new_with_label("launch terminal");
  gtk_box_pack_start(GTK_BOX(box1), button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  gtk_widget_show(box1);

  GdkWindow* gdk_window = gtk_widget_get_window(window);

  gdk_wayland_window_set_use_custom_surface(gdk_window);

  wl_surface = gdk_wayland_window_get_wl_surface(gdk_window);
  assert(wl_surface);

  layer_surface =
    zwlr_layer_shell_v1_get_layer_surface(layer_shell, wl_surface, nullptr, layer, _namespace);
  assert(layer_surface);
  zwlr_layer_surface_v1_add_listener(layer_surface, &layer_surface_listener, layer_surface);
  wl_surface_commit(wl_surface);
  wl_display_roundtrip(display);

  gtk_widget_show_all(window);

  wl_display_roundtrip(display);

  gtk_main();

  while (wl_display_dispatch(display) != -1 && run_display) {
    // This space intentionally left blank
  }

  return 0;
}
