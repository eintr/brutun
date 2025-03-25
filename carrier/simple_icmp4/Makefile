targets=simple_icmp4.so

CFLAGS+=-I../.. -DGNU_SOURCE -fPIC -g -O0

all: $(targets)

simple_icmp4.so: simple_icmp4.o
	gcc -shared $^ -o $@

clean:
	rm -f $(targets) *.o
