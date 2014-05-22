`paxd` is a simple daemon for Arch Linux intended to allow PaX to be enabled out-of-the-box.

Support for hooks in Pacman would be significantly better, but this can fill the gap until that point.

It reads the `/etc/paxd.conf` configuration file and applies the listed PaX exception flags to
binaries on start-up. It waits for finished pacman transactions or changes to the configuration file
and re-applies the attributes as-needed. It will do more work than necessary, but setting extended
attributes for a hundred files is far cheaper than Pacman simply reading the database.
