
PST_VER=libpst-0.6.76
PST_DIR=libpst

all: build


deps_clean:
	rm -rf deps

deps:
	mkdir -p deps
	wget https://www.five-ten-sg.com/libpst/packages/$(PST_VER).tar.gz -P deps/
	cd deps && tar -xvf $(PST_VER).tar.gz
	ln -s $(PST_VER) deps/$(PST_DIR)

build_pst:
	cd deps/$(PST_DIR) && ./configure --enable-python=no
	cd deps/$(PST_DIR) && make

pst.o: pst.c
	$(CC) -c -fPIC -Wall -g -I./deps/libpst/ -I./deps/libpst/src/ -o $@ $<

pstexport.o: pstexport.c
	$(CC) -c -fPIC -Wall -g -I./deps/libpst/ -I./deps/libpst/src/ -o $@ $<


build:
	$(CC) -c -fPIC -Wall -g -I./deps/libpst/ -I./deps/libpst/src/ pstexport.c pst.c
#	$(CC) -c -fPIC -Wall -g -I./deps/libpst/ -I./deps/libpst/src/ pst.c -o pst.o
	$(CC) -g -I./deps/libpst/ -I./deps/libpst/src/ ./deps/libpst/src/.libs/*.o pst.o pstexport.o -lz -lpthread -shared -o libgopst.so


libgopst.a: pst.o pstexport.o
	ar rcs $@ $^

example: build
	$(CC) -g -I./deps/libpst/ -I./deps/libpst/src/ ./deps/libpst/src/.libs/*.o -lz -lpthread pstexport.o pst.o example.c -o example.bin
