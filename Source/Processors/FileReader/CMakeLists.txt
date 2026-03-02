#SNAP directory-specific file

#add files in this folder
add_sources(snap-gui 
	FileReader.cpp
	FileReader.h
	FileReaderActions.cpp
	FileReaderActions.h
	FileReaderEditor.cpp
	FileReaderEditor.h
	FileSource.cpp
	FileSource.h
	ScrubberInterface.cpp
	ScrubberInterface.h
)

#add nested directories
add_subdirectory(BinaryFileSource)

