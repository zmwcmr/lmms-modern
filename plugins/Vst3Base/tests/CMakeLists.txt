ADD_EXECUTABLE(vst3-host-test
	host_test.cpp
	../Vst3Host.cpp
	../Vst3Module.cpp
)

TARGET_INCLUDE_DIRECTORIES(vst3-host-test
	PRIVATE
		../
		../travesty
		"${CMAKE_CURRENT_BINARY_DIR}/../vst3base"
)

TARGET_LINK_LIBRARIES(vst3-host-test Qt${QT_VERSION_MAJOR}::Core)

SET_TARGET_PROPERTIES(vst3-host-test PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
)

ADD_EXECUTABLE(vst3-editor-test
	editor_test.cpp
	../Vst3Host.cpp
	../Vst3Module.cpp
	../Vst3EditorWidget.cpp
	../Vst3EditorWindow.cpp
	../Vst3X11Editor.cpp
	../Vst3RemoteEditorClient.cpp
	../Vst3RemoteEditorWidget.cpp
	../Vst3RemoteHostClient.cpp
)

TARGET_INCLUDE_DIRECTORIES(vst3-editor-test
	PRIVATE
		../
		../travesty
		"${CMAKE_CURRENT_BINARY_DIR}/../vst3base"
)

TARGET_LINK_LIBRARIES(vst3-editor-test Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Network)
IF(LMMS_BUILD_LINUX)
	TARGET_LINK_LIBRARIES(vst3-editor-test X11)
ENDIF()

SET_TARGET_PROPERTIES(vst3-editor-test PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
)

ADD_EXECUTABLE(vst3-remote-host-test
	remote_host_test.cpp
	../Vst3RemoteHostClient.cpp
	../Vst3Host.cpp
	../Vst3Module.cpp
)

TARGET_INCLUDE_DIRECTORIES(vst3-remote-host-test
	PRIVATE
		../
		../travesty
		"${CMAKE_CURRENT_BINARY_DIR}/../vst3base"
)

TARGET_LINK_LIBRARIES(vst3-remote-host-test
	Qt${QT_VERSION_MAJOR}::Core
	Qt${QT_VERSION_MAJOR}::Network
)

ADD_DEPENDENCIES(vst3-remote-host-test lmms-vst3-host)

SET_TARGET_PROPERTIES(vst3-remote-host-test PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
)
