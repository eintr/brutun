CFLAGS+= -I. -pthread -Wall -g -D_GNU_SOURCE -O0

LDFLAGS+=-lpthread -lm -lssl -lcrypto -lrt

SERVERFNAME=brutun

sources=main.c relayer.c util_time.c json_conf.c cJSON.c cryp.c

objects=$(sources:.c=.o)

all: $(SERVERFNAME)

$(SERVERFNAME): $(objects)
	    $(CC) -o $@ $^ $(LDFLAGS)

install: all
	    install $(SERVERFNAME) /usr/local/sbin/

clean:
	    rm -f $(objects) $(SERVERFNAME)

