targets=simple_udp.so

CFLAGS+=-I../.. -DGNU_SOURCE -fPIC -O3

all: $(targets)

simple_udp.so: simple_udp.o
	gcc -shared $^ -o $@

clean:
	rm -f $(targets) *.o
