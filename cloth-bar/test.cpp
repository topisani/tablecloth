#include <cassert>

#include <gdk/gdkwayland.h>
#include <gtk/gtk.h>

#include <protocols.hpp>
#include <wayland-client.hpp>

namespace wl = wayland;

static wl::display_t display;
static wl::zwlr_layer_shell_v1_t layer_shell;

wl::zwlr_layer_surface_v1_t layer_surface;
wl::surface_t wl_surface;
wl::callback_t frame_callback;

auto layer = wl::zwlr_layer_shell_v1_layer::overlay;

GtkWidget* window;

int main(int argc, char** argv)
{
  const char* _namespace = "wlroots";

  gtk_init(&argc, &argv);

  GdkDisplay* gdk_display = gdk_display_get_default();

  wl::display_t display = wl::display_t(gdk_wayland_display_get_wl_display(gdk_display));
  if (!display) {
    fprintf(stderr, "Failed to create display\n");
    return 1;
  }

  wl::registry_t registry = display.get_registry();
  registry.on_global() = [&](uint32_t name, std::string interface, uint32_t version) {
    if (interface == layer_shell.interface_name) {
      registry.bind(name, layer_shell, version);
    }
  };

  display.roundtrip();

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
  assert(!!wl_surface);

  layer_surface = layer_shell.get_layer_surface(wl_surface, nullptr, layer, _namespace);
  layer_surface.set_size(300, 300);
  layer_surface.set_anchor(wl::zwlr_layer_surface_v1_anchor::top | wl::zwlr_layer_surface_v1_anchor::left);

  assert(!!layer_surface);

  layer_surface.on_closed() = [&] {
    gtk_window_close(GTK_WINDOW(window));
    wl_surface_destroy(wl_surface);
  };

  layer_surface.on_configure() = [&](uint32_t serial, uint32_t w, uint32_t h) {
    if (window) {
      gtk_window_resize(GTK_WINDOW(window), w, h);
    }
    layer_surface.ack_configure(serial);
    gtk_widget_show_all(window);
  };

  wl_surface.commit();

  display.roundtrip();

  gtk_widget_show_all(window);

  display.roundtrip();

  gtk_main();

  return 0;
}
