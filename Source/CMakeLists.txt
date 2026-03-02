#SNAP directory-specific file

#add files in this folder
add_sources(snap-gui
	TestableExport.h
	AccessClass.h
	AccessClass.cpp
	AutoUpdater.cpp
	AutoUpdater.h
	CoreServices.h
	CoreServices.cpp
	SNAPMainWindow.h
	SNAPMainWindow.cpp
	Main.cpp
)

#add nested directories
add_subdirectory(Audio)
add_subdirectory(Processors)
add_subdirectory(UI)
add_subdirectory(Utils)

