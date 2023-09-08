# gopst
Libpst tools (pst parsing) integration.

# Installation
## Building dependecies

```bash
cd src
make deps
cd deps/libpst
sudo apt install libgsf-1-dev
./configure --enable-python=no --enable-libpst-shared=yes
make
sudo make install
sudo cp define.h config.h /usr/local/include/libpst-4/libpst
cd ../..
make all
make install # ?
```

# Usage

## Creating an Pst
Creates a new Pst initialization and passes the path to the .pst file with the parameter.

Has to be deallocated with *Destroy* method after use.
```go
pst := gopst.NewPst("./fixtures/sample.pst")
if pst.NumError != gopst.NO_ERROR {
    return errors.New(pst.LastError)
}
```
## Export Configuration
Next, it is necessary to create the export configurator, according to which the subsequent Export is created.

This configurator is necessary for **how** the individual records will be unpacked.


Export configuration structure:
```go
type ExportConf struct {
	Mode                 int
	ModeMH               int // A submode of MODE_SEPARATE
	ModeEX               int // A submode of MODE_SEPARATE
	ModeMSG              int // A submode of MODE_SEPARATE
	ModeThunder          int // A submode of MODE_RECURSE
	OutputMode           int
	ContactMode          int // Not used within the code
	DeletedMode          int // Not used within the code
	OutputTypeMode       int // Default to all. Not used within the code
	ContactModeSpecified int // Not used within the code
	Overwrite            int
	PreferUtf8           int
	SaveRtfBody          int // Unused
	FileNameLen          int // Enough room for MODE_SPEARATE file name
	AcceptableExtensions string
}
```
### Settings options
#### Mode
* **MODE_NORMAL** - Normal mode just creates mbox format files in the current directory. Each file is named the same as the folder's name that it represents.
* **MODE_KMAIL** - KMail mode creates a directory structure suitable for being used directly by the KMail application.
* **MODE_RECURSE** - Recurse mode creates a directory structure like the PST file. Each directory contains only one file which stores the emails in mboxrd format.
* **MODE_SEPARATE** - Separate mode creates the same directory structure as recurse. The emails are stored in separate files, numbering from 1 upward. Attachments belonging to the emails are saved as email_no-filename (e.g. 1-samplefile.doc or 1-Attachment2.zip).

#### OutputMode
* **OUTPUT_NORMAL** - Output Normal just prints the standard information about what is going on.
* **OUTPUT_QUIET** - Output Quiet is provided so that only errors are printed.

#### ContactMode
* **CMODE_VCARD**
* **CMODE_LIST**

#### DeletedMode
* **DMODE_EXCLUDE**
* **DMODE_INCLUDE**

#### OutputTypeMode
* **OTMODE_EMAIL**
* **OTMODE_APPOINTMENT**
* **OTMODE_JOURNAL**
* **OTMODE_CONTACT**


### Export Configuration default settings
```go
func ExportConfDefault() ExportConf {
	return ExportConf{
		Mode:                 MODE_NORMAL,
		ModeMH:               0,
		ModeEX:               0,
		ModeMSG:              0,
		ModeThunder:          0,
		OutputMode:           OUTPUT_NORMAL,
		ContactMode:          CMODE_VCARD,
		DeletedMode:          DMODE_EXCLUDE,
		OutputTypeMode:       0xff, // all
		ContactModeSpecified: 0,
		Overwrite:            0,
		PreferUtf8:           1,
		SaveRtfBody:          0,
		FileNameLen:          10,
		AcceptableExtensions: "",
	}
}
```

### Creating and Export
Then we create a new initialization of the Export using the configurator.

Has to be deallocated with *Destroy* method after use.
```go
export := gopst.NewExport(gopst.ExportConfDefault())
if export == nil {
    return errors.New("NewExport failed")
}
```
## List
The next step is the *List* method. This method lists content of an pst in form of arrays.

Records must be destroyed by *DestroyList* call explicitly. 

Alternatively, it is possible to destroy individual records using the *Destroy* function.
```go
records := pst.List()
```
## Extraction to file and renaming
Here comes the time for extraction.  To extract, you need to iterate over the individual records and call the *RecordToFile* function on them individually with the [export](https://github.com/SpongeData-cz/gopst#creating-and-export) parameter passed.

Alternatively, Records can be renamed before extraction using the *SetRecordRenaming* method. The full path with the new name must be passed as a parameter.
```go
for i, record := range records {
    newName := fmt.Sprintf("output_%d.eml", i)
    record.SetRecordRenaming(pathToExtract + newName)
    record.RecordToFile(export)
}
```
## Errors in records
Next, it is a good idea to iterate through the records to see if any of them have errors.

This can be done, for example, with the following code:
```go
for i, record := range records {
	if record.Err != NO_ERROR {
		fmt.Printf("Record %s has error %d\n", record.Name, record.Err)
	}
}
```
## Destroy
### Destroy Pst
First, the Pst must be destroyed using the *Destroy* function.
```go
if err := pst.Destroy(); err != nil {
    return err
}
```
### Destroy List
It is also necessary to destroy the list of records using the *DestroyList* function.
```go
if err := gopst.DestroyList(records); err != nil {
    return err
}
```
Alternatively, it is possible to destroy individual records using the *Destroy* function.
```go
if err := record.Destroy(); err != nil{
	return err
}
```
### Destroy Export
The last thing to do is to destroy the export also using the *Destroy* function.
```go
if err := export.Destroy(); err != nil {
    return err
}
```

## Errors
Different types of errors are defined by constants.
```go
const (
    NO_ERROR = iota
    ERROR_NOT_UNIQUE_MSG_STORE
    ERROR_ROOT_NOT_FOUND
    ERROR_OPEN
    ERROR_INDEX_LOAD
    ERROR_UNKNOWN_RECORD
)
```