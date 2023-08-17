package gopst

/*
#cgo LDFLAGS: libgopst.a
#cgo CFLAGS: -I./deps/libpst/ -I./deps/libpst/src/

#include "define.h"

#include <pst.h>
#include <stdlib.h>
inline void EntrySetRenaming(Entry * self, const char * renaming);
*/

import "C"
