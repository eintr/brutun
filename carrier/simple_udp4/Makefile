targets=simple_udp4.so

CFLAGS+=-I../.. -DGNU_SOURCE -fPIC -O3

all: $(targets)

simple_udp4.so: simple_udp4.o
	gcc -shared $^ -o $@

clean:
	rm -f $(targets) *.o
