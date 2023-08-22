package gopst_test

import (
	"fmt"
	"testing"

	. "github.com/SpongeData-cz/gopst"
)

func TestPst(t *testing.T) {

	printEnum := func(enum PstRecordEnumerator) {
		println("\n--PRINTING ENUM--")
		for i, e := range enum.Items {
			fmt.Printf("Item number %d: %+v\n", i, e)
			fmt.Printf("	TypeOfRecord: %d\n	LogicalPath: %s\n	Name: %s\n	Renaming: %s\n	ExtraMImeHeaders: %s\n\n",
				e.TypeOfRecord,
				// e.PFile,
				// e.PItem,
				e.LogicalPath,
				e.Name,
				e.Renaming,
				e.ExtraMimeHeaders,
			)
		}
		fmt.Printf(
			"Capacity: %d\nUsed: %d\nFile: %+v\nLastError: %s\nNumError: %d\n",
			enum.Capacity,
			enum.Used,
			enum.File,
			enum.LastError,
			enum.NumError,
		)
		println("\n")
	}

	t.Run("init", func(t *testing.T) {

		def := PstExporConfDefault()
		ret := NewPstExport(def)

		fmt.Printf("Return of new: %+v\n", ret)

	})

	t.Run("list", func(t *testing.T) {

		enum := PstList("./src/sample.pst")
		if enum.NumError != NO_ERROR {
			t.Error(enum.LastError)
		}

		printEnum(enum)

		// Non-existing path
		// TODO: Printing error on stdout/stderr, make it shhhh?
		enumNon := PstList("./im/not/existing.pst")
		if enumNon.NumError != ERROR_OPEN {
			t.Error("Should throw ERROR_OPEN error.")
		}

		printEnum(enumNon)

	})

}
