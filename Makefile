VERSION=1.2

FILE="cpuda.so"
DIR="cpu-monitor-extension-lxpanel-plugin"

all: $(FILE)

$(FILE): cpuda.c
	gcc -O2 -Wall `pkg-config --cflags lxpanel gtk+-2.0` \
	-shared -fPIC cpuda.c -o $(FILE) \
	`pkg-config --libs lxpanel gtk+-2.0`

clean:
	rm -f $(FILE)

install: 
	@ if [ "$(DESTDIR)" ]; then \
	  cp -v $(FILE) $(DESTDIR); \
	elif [ -d "/usr/lib/lxpanel/plugins" ]; then \
	  cp -v $(FILE) /usr/lib/lxpanel/plugins; \
	elif [ -d "/usr/lib64/lxpanel/plugins" ]; then \
	  cp -v $(FILE) /usr/lib64/lxpanel/plugins; \
	else \
	  echo ;\
	  echo Couldn\'t find lxpanel/plugins directory.; \
	  echo Checked /usr/lib/lxpanel/plugins and /usr/lib64/lxpanel/plugins; \
	  echo Find it yourself by running \'locate deskno.so\'; \
	  echo Then copy sensors.so to that directory.; \
	fi

package:
	rm -Rf $(DIR)-${VERSION}
	mkdir $(DIR)-${VERSION}
	cp README Makefile cpuda.c COPYING AUTHORS $(DIR)-${VERSION}
	tar czvf $(DIR)-${VERSION}.tar.gz \
		 $(DIR)-${VERSION}
	rm -Rf $(DIR)-${VERSION}
