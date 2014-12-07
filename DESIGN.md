Sample configuration:

    em /usr/bin/foo
    em /usr/bin/bar
    em /usr/local/bin/baz
    em /opt/a/b

The following 7 directory watches (`IN_CREATE|IN_MOVED_TO`) are necessary:

* `/` -> check for `/usr/` or `/opt/`
* `/usr/` -> check for `/usr/bin/` or `/usr/local/`
* `/usr/bin/` -> check for `/usr/bin/foo` or `/usr/bin/bar`
* `/usr/local/` -> check for `/usr/local/bin/`
* `/usr/local/bin/` -> check for `/usr/local/bin/baz`
* `/opt/` -> check for `/opt/a/`
* `/opt/a/` -> check for `/opt/a/b`

The configuration file is translated into 3 lookup tables tracking `watch ->
directory`, `directory -> { watch, children }` and `executable -> exception` on
start-up and then again whenever it is updated. At that point, *all* of the
exceptions are applied.

Filesystem event are mapped to paths via the lookup table. Recursion through
the directory tree will be triggered, removing the old watches from the table
and adding new ones. Upon reaching a leaf node (executable), the exception will
be applied.

The kernel will automatically close inotify watches when files are deleted. A
call to `inotify_rm_watch` is actually incorrect because the descriptor may be
released and recycled by another file at any point. This does mean that it will
hold onto irrelevant watches if a directory is moved, but they will be released
when it is removed.
