`paxd` is a daemon for automatically maintaining PaX exceptions. It re-applies
exceptions whenever an executable is created / replaced. It also applies all of
the exceptions at start-up and when the configuration file is modified.

Since `paxd` watches the parent directory chain for each executable, it has no
problem dealing with the creation of an executable in a directory that did not
exist when it was started. It works fine with all package managers it has been
tried with.

The sample `paxd.conf` is targeted at Arch Linux, and the expectation is that
maintainers / users of other distributions will maintain a modified version
downstream.

## Setting up user level paxd

Copy `user.conf` to `~/.config/paxd.conf`, and then enable the user service:

    systemctl --user enable paxd
    systemctl --user start paxd

Files controlled by non-root shouldn't be added to the global configuration
file, as those users would be able to use symlinks to set exceptions on any
files.
