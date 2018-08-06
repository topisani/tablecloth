Written in modern C++, and based on the [wlroots example compositor rootston](https://github.com/swaywm/wlroots/tree/master/rootston) (after porting that to said modern c++), tablecloth is focused on being able to do fancy-pants animations, because who doesnt like those!

Also, it includes a crude gtk-based bar, and a WIP notification server, which should be sway-compatible

tablecloth features:
 - workspaces (separate on outputs planned)
 - floating first
 - some cool form of tiling is planned, focused on being as easy to use with keyboard, mouse or touch.

cloth-notifications features:
 - compatible with sway, and anything else that supports the layer_shell protocol
 - written in C++, drawn using GTK
 - Support for icons, actions and urgencies
 - 350 lines of code.
 - bugs

cloth-bar features:
 - not much yet, i should be spending my time porting polybar instead

I plan to do a osx expose-style window switcher (animated of course), maybe some "notification-center" with some settings, possibly a custom app launcher.

I don't really plan to make it highly configurable, but i do plan to keep it fairly easilly forkable, so it can be used as a starting point for other people's customizations.

Contributions welcome! - have fun
