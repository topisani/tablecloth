#include "view.hpp"

#include "util/algorithm.hpp"
#include "util/logging.hpp"
#include "wlroots.hpp"

#include "desktop.hpp"
#include "input.hpp"
#include "server.hpp"
#include "seat.hpp"

namespace cloth {

  WlShellPopup::WlShellPopup(View& p_view, wlr::wl_shell_surface_t* p_wlr_popup)
    : ViewChild(p_view, p_wlr_popup->surface), wlr_popup(p_wlr_popup)
  {
    on_destroy.add_to(wlr_popup->events.destroy);
    on_destroy = [this] { util::erase_this(view.children, this); };
    on_set_state.add_to(wlr_popup->events.set_state);
    on_set_state = [this] { util::erase_this(view.children, this); };
    on_new_popup.add_to(wlr_popup->events.new_popup);
    on_new_popup = [this](void* data) {
      dynamic_cast<WlShellSurface&>(view).create_popup(*(wlr::wl_shell_surface_t*) data);
    };
  }

  void WlShellSurface::do_resize(int width, int height)
  {
    wlr_wl_shell_surface_configure(wl_shell_surface, WL_SHELL_SURFACE_RESIZE_NONE, width, height);
  }

  void WlShellSurface::do_close()
  {
    wl_client_destroy(wl_shell_surface->client);
  }

  WlShellSurface::WlShellSurface(Workspace& p_workspace, wlr::wl_shell_surface_t* p_wl_shell_surface)
   : View(p_workspace), wl_shell_surface(p_wl_shell_surface) 
  {
    View::wlr_surface = wl_shell_surface->surface;
    width = wl_shell_surface->surface->current.width;
    height = wl_shell_surface->surface->current.height;
    on_request_move.add_to(wl_shell_surface->events.request_move);
    on_request_move = [this](void* data) {
      Input& input = this->desktop.server.input;
      auto&  e = *(wlr::wl_shell_surface_move_event_t*) data;
      Seat* seat = input.seat_from_wlr_seat(*e.seat->seat);
      if (!seat || seat->cursor.mode != Cursor::Mode::Passthrough) {
        return;
      }
      seat->begin_move(*this);
    };

    on_request_resize.add_to(wl_shell_surface->events.request_resize);
    on_request_resize = [this](void* data) {
      Input& input = this->desktop.server.input;
      auto&  e = *(wlr::wl_shell_surface_resize_event_t*) data;
      Seat* seat = input.seat_from_wlr_seat(*e.seat->seat);
      if (!seat || seat->cursor.mode != Cursor::Mode::Passthrough) {
        return;
      }
      seat->begin_resize(*this, wlr::edges_t(e.edges));
    };

    on_request_maximize.add_to(wl_shell_surface->events.request_maximize);
    on_request_maximize = [this](void* data) {
      // auto& e = *(wlr::wl_shell_surface_maximize_event_t *) data;
      maximize(true);
    };

    on_request_fullscreen.add_to(wl_shell_surface->events.request_fullscreen);
    on_request_fullscreen = [this](void* data) {
      auto&  e = *(wlr::wl_shell_surface_set_fullscreen_event_t*) data;
      set_fullscreen(true, e.output);
    };

    on_set_state.add_to(wl_shell_surface->events.set_state);
    on_set_state = [this](void* data) {
      if (maximized && wl_shell_surface->state != WLR_WL_SHELL_SURFACE_STATE_MAXIMIZED) {
        maximize(false);
      }
      if (fullscreen_output != nullptr &&
          wl_shell_surface->state != WLR_WL_SHELL_SURFACE_STATE_FULLSCREEN) {
        set_fullscreen(false, nullptr);
      }
    };

    on_surface_commit.add_to(wl_shell_surface->surface->events.commit);
    on_surface_commit = [this](void* data) {
      apply_damage();

      int width = wl_shell_surface->surface->current.width;
      int height = wl_shell_surface->surface->current.height;
      update_size(width, height);

      double x = this->x;
      double y = this->y;
      if (this->pending_move_resize.update_x) {
        x = this->pending_move_resize.x + this->pending_move_resize.width - width;
        this->pending_move_resize.update_x = false;
      }
      if (this->pending_move_resize.update_y) {
        y = this->pending_move_resize.y + this->pending_move_resize.height - height;
        this->pending_move_resize.update_y = false;
      }
      update_position(x, y);
    };

    on_new_popup.add_to(wl_shell_surface->events.new_popup);
    on_new_popup = [this](void* data) {
      create_popup(*(wlr::wl_shell_surface_t*) data);
    };

    on_destroy.add_to(wl_shell_surface->events.destroy);
    on_destroy = [this] { workspace->erase_view(*this); };
  }

  WlShellPopup& WlShellSurface::create_popup(wlr::wl_shell_surface_t& wlr_popup) {
    auto popup = std::make_unique<WlShellPopup>(*this, &wlr_popup);
    children.push_back(std::move(popup));
    return *popup;
  }

  void Desktop::handle_wl_shell_surface(void* data)
  {
    auto& surface = *(wlr::wl_shell_surface_t*) data;

    if (surface.state == WLR_WL_SHELL_SURFACE_STATE_POPUP) {
      LOGD("new wl shell popup");
      return;
    }

    LOGD("new wl shell surface: title={}, class={}", surface.title, surface.class_);
    wlr_wl_shell_surface_ping(&surface);

    auto& workspace = *outputs.front().workspace;
    auto view_ptr = std::make_unique<WlShellSurface>(workspace, &surface);
    auto& view = *view_ptr;
    workspace.add_view(std::move(view_ptr));

    view.map(*surface.surface);
    view.setup();

    if (surface.state == WLR_WL_SHELL_SURFACE_STATE_TRANSIENT) {
      // We need to map it relative to the parent
      auto parent = util::find_if(view.workspace->views(), [&] (auto& parent) { 
        auto* ptr = dynamic_cast<WlShellSurface*>(&parent);
        return ptr && ptr->wl_shell_surface == surface.parent;
      });
      if (parent != view.workspace->views().end()) {
        view.move(parent->x + surface.transient_state->x,
                  parent->y + surface.transient_state->y);
      }
    }
  }
} // namespace cloth
