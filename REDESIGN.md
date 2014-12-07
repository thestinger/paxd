At the moment, paxd is based on detection of package manager transactions. It
needs to be migrated to watching paths with exceptions in order to be package
manager agnostic, support /usr/local and most importantly to work as a user
service for setting executables in a user's home directory.

Sample configuration:

    em /usr/bin/foo
    em /usr/bin/bar
    em /usr/local/bin/baz
    em /opt/a/b

The following 6 directory watches (`IN_CREATE|IN_MOVED_TO`) are necessary:

* `/` -> check for `/usr/` or `/opt/`
* `/usr/` -> check for `/usr/bin/` or `/usr/local/bin/`
* `/usr/bin/` -> check for `/usr/bin/foo` or `/usr/bin/bar`
* `/usr/local/bin/` -> check for `/usr/local/bin/baz`
* `/opt/` -> check for `/opt/a/`
* `/opt/a/` -> check for `/opt/a/b`

The configuration file needs to be translated into a lookup table tracking the
children belonging to each directory watch. To make things easy, the table can
be completely recreated upon any configuration changes along with the inotify
file descriptor being replaced with a fresh one.

The kernel will automatically close inotify watches when files are deleted. A
call to `inotify_rm_watch` is actually incorrect because the descriptor may be
released and recycled by another file at any point. This does mean that watches
will essentially be leaked if a directory is moved away and recreated, at least
until it is finally removed.

It needs to recurse through a subdirectory upon creation, setting up the new
watches as it goes. This will handle dynamic creation of application-specific
directories. Special care needs to be taken to avoid race conditions.
