X11INC = /usr/X11R6/include
X11LIB = /usr/X11R6/lib
INCS = -I. -I/usr/include -I${X11INC}
LIBS = -L/usr/lib -lc -L${X11LIB} -lX11
CFLAGS = -std=c99 -Wall ${INCS}
LDFLAGS = -s ${LIBS}

all: hatewm

hatewm: hatewm.o

clean:
	rm *.o
	rm hatewm
