PREFIX=/usr/local
CPPFLAGS=-Wall
ALL=fanctrld init.fanctrld

all: $(ALL)

fanctrld: fanctrld.c

init.fanctrld: init.fanctrld.sh
	sed -e s#@FANCTRLD@#$(PREFIX)/sbin/fanctrld#g \
		init.fanctrld.sh > init.fanctrld

install: $(ALL)
	install -o 0 -g 0 fanctrld $(DESTDIR)$(PREFIX)/sbin
	install -o 0 -g 0 -m 755 init.fanctrld $(DESTDIR)/etc/init.d/fanctrld
	install -o 0 -g 0 -m 644 sysconf.fanctrld.conf $(DESTDIR)/etc/sysconfig/fanctrld

clean:
	-rm -f *~ *.o $(ALL)
