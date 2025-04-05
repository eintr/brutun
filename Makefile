include config.mk

CFLAGS+= -I. -pthread -Wall -D_GNU_SOURCE -O3

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

tarball: all
	mkdir -p .root/$(SBINDIR) .root/$(PLUGINDIR)
	install $(SERVERFNAME) .root/$(SBINDIR)
	install carrier/*/*.so .root/$(PLUGINDIR)
	tar -cf brutun_install-0-$(shell uname -m).tar.gz -C .root .
	$(RM) -r .root

clean:
	make -C carrier $@
	rm -f $(objects) $(SERVERFNAME)

