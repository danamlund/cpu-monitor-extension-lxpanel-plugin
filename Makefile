VERSION=1.5
NAME=cpu-monitor-extension-lxpanel-plugin
INPUT=cpuda.c
OUTPUT=cpuda.so

all: ${OUTPUT}

${OUTPUT}: ${INPUT}
	./check_required.sh
	gcc -O2 -Wall `pkg-config --cflags glib-2.0 gtk+-2.0` \
	    -shared -fPIC ${INPUT} -o ${OUTPUT} \
	    `pkg-config --libs glib-2.0 gtk+-2.0`

clean:
	rm -f ${OUTPUT}

install: ${OUTPUT}
	./install.sh

package:
	rm -Rf ${NAME}-${VERSION}
	mkdir ${NAME}-${VERSION}
	cp -Rf README Makefile ${INPUT} COPYING AUTHORS \
	       check_required.sh install.sh lxpanel \
               ${NAME}-${VERSION}
	tar czvf ${NAME}-${VERSION}.tar.gz \
		 ${NAME}-${VERSION}
	rm -Rf ${NAME}-${VERSION}
