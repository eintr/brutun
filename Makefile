CFLAGS+=-I -pthread -Wall -g -D_GNU_SOURCE -O0

LDFLAGS+=-lpthread -lm -lssl -lcrypto -lrt 

SERVERFNAME=warmhole

sources=main.c relayer.c

objects=$(sources:.c=.o)

all: $(SERVERFNAME)

$(SERVERFNAME): $(objects)
	    $(CC) -o $@ $^ $(LDFLAGS)

install: all
	    $(INSTALL) $(SERVERFNAME) /usr/local/sbin/

clean:
	    rm -f $(objects) $(SERVERFNAME)

