package gopst

/*
#cgo LDFLAGS: ./src/libgopst.a
#cgo CFLAGS: -I./src/ -I./src/deps/libpst/ -I./src/deps/libpst/src/

#include "define.h"

#include <pst.h>
#include <stdlib.h>
inline void EntrySetRenaming(Entry * self, const char * renaming);
*/

import "C"
