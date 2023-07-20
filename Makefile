
PST_VER=libpst-0.6.76
PST_DIR=libpst

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

build:
	gcc -I./deps/libpst/ -I./deps/libpst/src/ ./deps/libpst/src/.libs/*.o -lz -lpthread pst.c pstexport.c -o test.bin

