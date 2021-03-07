SOURCES = main

INCLUDES = -I. -I$(srcdir) -I../include -I ../include
COMPILE = $(CC) $(DEFS) $(INCLUDES) $(CFLAGS)
CC = gcc -lasound -lpthread
CFLAGS = -I. -g -Wall -Wshadow -Wpointer-arith -Wwrite-strings -Wno-strict-prototypes 
FLAG = -lasound -lpthread

all: $(SOURCES)

.SUFFIXES: .c .o

main: main.o  audio.o g722_decode.o g722_encode.o
	$(CC) $(CFLAGS) -lm $(LDFLAGS) -o $@ main.o audio.o g722_decode.o g722_encode.o $(FLAG)

clean: clean-SOURCES
	rm -f *.o

clean-SOURCES:
	@list='$(SOURCES)'; for p in $$list; do \
	  f=`echo $$p|sed 's/$$//'`; \
	  echo " rm -f $$p $$f"; \
	  rm -f $$p $$f ; \
	done


# Tell versions [3.59,3.63) of GNU make to not export all variables.
# Otherwise a system limit (for SysV at least) may be exceeded.
.NOEXPORT:
