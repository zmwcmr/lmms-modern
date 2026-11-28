IF(LMMS_BUILD_LINUX)
	SET(VST3_REMOTE_HOST_SRC ../Vst3RemoteHostClient.cpp)
ELSE()
	SET(VST3_REMOTE_HOST_SRC ../Vst3RemoteHostClientStub.cpp)
ENDIF()

BUILD_PLUGIN(vst3base
	../vst3_base.cpp
	${CMAKE_SOURCE_DIR}/src/gui/X11LoadingPlaceholder.cpp
	${CMAKE_SOURCE_DIR}/src/gui/X11EditorWindow.cpp
	../Vst3Module.cpp ../Vst3Module.h
	../Vst3Scanner.cpp ../Vst3Scanner.h
	../Vst3List.cpp ../Vst3List.h
	../Vst3Host.cpp ../Vst3Host.h
	../Vst3PluginHost.cpp ../Vst3PluginHost.h
	../Vst3ParameterModels.cpp ../Vst3ParameterModels.h
	../Vst3EditorWidget.cpp ../Vst3EditorWidget.h
	../Vst3EditorWindow.cpp ../Vst3EditorWindow.h
	../Vst3X11Editor.cpp ../Vst3X11Editor.h
	../Vst3RemoteEditorClient.cpp ../Vst3RemoteEditorClient.h
	../Vst3RemoteEditorWidget.cpp ../Vst3RemoteEditorWidget.h
	${VST3_REMOTE_HOST_SRC} ../Vst3RemoteHostClient.h
	../Vst3SharedAudio.h
	../Vst3Protocol.h
	../Vst3SubPluginFeatures.cpp ../Vst3SubPluginFeatures.h
	EXPORT_BASE_NAME vst3base
	LINK SHARED
)

TARGET_INCLUDE_DIRECTORIES(vst3base
	PUBLIC ../
	PRIVATE ../travesty "${CMAKE_SOURCE_DIR}/include"
)

FIND_PACKAGE(Qt${QT_VERSION_MAJOR} COMPONENTS Core REQUIRED)
TARGET_LINK_LIBRARIES(vst3base Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Network)

IF(LMMS_BUILD_LINUX)
	TARGET_LINK_LIBRARIES(vst3base X11)
ENDIF()

IF(APPLE)
	FIND_LIBRARY(COREFOUNDATION_FRAMEWORK CoreFoundation)
	IF(COREFOUNDATION_FRAMEWORK)
		TARGET_LINK_LIBRARIES(vst3base "${COREFOUNDATION_FRAMEWORK}")
	ENDIF()
ENDIF()

IF(MSVC)
	# The lmms executable exports ValueBuffer, which pulls the base
	# std::vector<float> special members into its import library. vst3base
	# instantiates the same templates itself, which MSVC otherwise reports as
	# multiply defined symbols (LNK2005). They are identical COMDATs.
	TARGET_LINK_OPTIONS(vst3base PRIVATE /FORCE:MULTIPLE)
ENDIF()
