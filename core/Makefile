# default: all

CFLAGS+=     \
  -std=gnu11 \
  -g         \
  -Wall      \
  -fpic      \

LDFLAGS+=   \
  -L. \
  -lsharedMemory \
  -lpthread \
  -lc		\

INCLUDES=        \
  sharedMemory.h \
  semaphore.h    \
  utils.h        \

TARGETS=  \
  libsharedMemory.so \
  dump    \
  insert  \
  fetch   \
  writeRecord \
  readRecord  \

EXTRA_FILES=  \
  Makefile    \
  README.md   \

#
#	Suppress all of the automatic command echoing.
#
.SILENT:

default: rebuild

all:  $(TARGETS)

rebuild:
	make -s distclean; \
	make all

insert : insert.o libsharedMemory.so $(INCLUDES)
	echo "Rebuilding the $@ executable..."; \
	gcc $(CFLAGS) -o $@ $@.o $(LDFLAGS)

fetch : fetch.o libsharedMemory.so $(INCLUDES)
	echo "Rebuilding the $@ executable..."; \
	gcc $(CFLAGS) -o $@ $@.o $(LDFLAGS)

dump : dump.o libsharedMemory.so $(INCLUDES)
	echo "Rebuilding the $@ executable..."; \
	gcc $(CFLAGS) -o $@ $@.o $(LDFLAGS)

writeRecord : writeRecord.o libsharedMemory.so $(INCLUDES)
	echo "Rebuilding the $@ executable..."; \
	gcc $(CFLAGS) -o $@ $@.o $(LDFLAGS)

readRecord : readRecord.o libsharedMemory.so $(INCLUDES)
	echo "Rebuilding the $@ executable..."; \
	gcc $(CFLAGS) -o $@ $@.o $(LDFLAGS)

libsharedMemory.so: sharedMemory.o semaphore.o utils.o
	echo "Linking the $@ shared library..."; \
	gcc $(CFLAGS) -shared -o $@ sharedMemory.o semaphore.o utils.o

tar:
	echo "Creating the development tar package..."; \
	make clean; \
	make all; \
	tar -cvzf sviPrototype.tz *.c *.h $(EXTRA_FILES) $(TARGETS)

clean:
	echo "Deleting all derived files..."; \
	rm -f *.o *~ $(TARGETS) sviPrototype.tz

cleanshm:
	echo "Deleting the shared memory segment file.."; \
	rm -f /var/run/shm/vsiSharedMemorySegment

distclean:
	make -s clean; \
	make -s cleanshm


.c.o: $(INCLUDES)
	echo "Compiling $?";  \
	gcc $(CFLAGS) -c $?
