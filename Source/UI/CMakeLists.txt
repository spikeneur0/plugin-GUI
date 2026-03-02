#SNAP directory-specific file

#add files in this folder
add_sources(snap-gui 
	ConsoleViewer.cpp
	ConsoleViewer.h
	SNAPControlPanel.cpp
	SNAPControlPanel.h
	CustomArrowButton.cpp
	CustomArrowButton.h
	MessageCenterButton.h
	SNAPDataViewport.cpp
	SNAPDataViewport.h
	DefaultConfig.cpp
	DefaultConfig.h
	MessageWindow.cpp
	MessageWindow.h
	SNAPEditorViewport.cpp
	SNAPEditorViewport.h
	FilenameConfigWindow.cpp
	FilenameConfigWindow.h
	CanvasAnnotation.cpp
	CanvasAnnotation.h
	SNAPGraphViewer.cpp
	SNAPGraphViewer.h
	InfoLabel.cpp
	InfoLabel.h
	SNAPPluginInstaller.cpp 
	SNAPPluginInstaller.h
	PopupComponent.cpp
	PopupComponent.h
	SNAPProcessorList.cpp
	SNAPProcessorList.h
	SplashScreen.cpp
	SplashScreen.h
	SNAPUIComponent.cpp
	SNAPUIComponent.h
)

#add nested directories
add_subdirectory(LookAndFeel)
add_subdirectory(Utils)
