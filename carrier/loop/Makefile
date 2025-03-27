targets=loop.so

CFLAGS+=-I../.. -DGNU_SOURCE -fPIC -g -O0

all: $(targets)

loop.so: loop.o
	$(CC) -shared $^ -o $@

clean:
	rm -f $(targets) *.o
