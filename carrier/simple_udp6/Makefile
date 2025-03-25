targets=simple_udp6.so

CFLAGS+=-I../.. -DGNU_SOURCE -fPIC -g -O0

all: $(targets)

simple_udp6.so: simple_udp6.o
	gcc -shared $^ -o $@

clean:
	rm -f $(targets) *.o
