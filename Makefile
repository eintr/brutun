include config.mk

CFLAGS+= -I. -pthread -Wall -D_GNU_SOURCE -O0 -g

#LDFLAGS+=-lpthread -lm -lssl -lcrypto -lrt -ldl
LDFLAGS+=-lpthread -lm -ldl -rdynamic

SERVERFNAME=brutun

sources=main.c tunnel.c carrier.c util_time.c cryp.c util_cjson.c cJSON.c

objects=$(sources:.c=.o)

all: $(SERVERFNAME)
	make -C carrier $@

$(SERVERFNAME): $(objects)
	    $(CC) -o $@ $^ $(LDFLAGS)

install: all
	mkdir -p $(INSTALL_SBINDIR) $(INSTALL_PLUGINDIR)
	install $(SERVERFNAME) $(INSTALL_SBINDIR)
	install carrier/*/*.so $(INSTALL_PLUGINDIR)

clean:
	make -C carrier $@
	rm -f $(objects) $(SERVERFNAME)

