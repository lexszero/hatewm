HateWM
=======

Most present window managers (WMs) are not actually WMs - they are combined things, that not only manages windows, but also processes hotkeys, draws fancy bars, spawning programs, whistles, etc. I think, this is wrong: window manager should <b>only</b> manage windows, leaving all other things to programs, that can do it better. For example, one may want use conky for monitoring CPU load and xbindkeys to process hotkeys stuff. For this purposes, WM should have an easy to use and common interface that leaves great freedom for external software. The file system is giving us apropriate interface, programs can interfere with windows by simply reading and writing files, creating dirs, etc.

So, we need to create an abstraction layer beetween X11 and file system. The 9P protocol is a good choice for this: it's lightweight, crossplatform and we can mount a 9P server into our filesystem. As source of all X-related stuff, was chosen [dwm](http://dwm.suckless.org/): it has easy-readable source code and not very big pile of crap inside.

At the moment, hatewm is nor ready or just doing base functions, but I have some code an planning to make it do something useful before the end of 2009 year. Source code repository will be created and public-accesibly.

Some technical stuff
------

Planned structure of the FS tree:

    root
        events
        ctl
        properties
        1           // this is window id
            events
            ctl
            properties
        2
            events
            ctl
            properties
        ...and so on
