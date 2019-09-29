My wayland/sway GUI tools, written in moder c++

I don't have any plans to make theese configurable, but i do plan to keep it fairly easilly forkable, so they can be used as starting points for other people's customizations.

Contributions welcome! - have fun

# cloth-notifications

![](https://i.ibb.co/PsLPZm7/image.png)

 - Support for icons, actions, urgencies, and replacing notifications
 - compatible with sway, and anything else that supports the layer_shell protocol
 - written in C++, drawn using GTK
 - Very little code, so should be easy to extend/modify to your liking.

# cloth-lock

![](https://i.ibb.co/7SBQdjr/image.png)

 - Extremely simple lock screen
 - uses PAM for authentication
 - multi-monitor support
 - background image can be set (using css)

# cloth-outputs
WIP arandr-style GUI for sway output configuration. Could easilly be adapted for output configuration in most other wlroots compositors.
### Works:
 - repositioning with drag'n'drop snapping gui
 - right click to toggle
### Planned features:
 - export layout to script
 - set resolution
