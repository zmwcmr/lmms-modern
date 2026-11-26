ADD_EXECUTABLE(lmms-vst3-host
	main.cpp
	../Vst3Host.cpp
	../Vst3Module.cpp
)

TARGET_INCLUDE_DIRECTORIES(lmms-vst3-host
	PRIVATE
		../
		../travesty
		"${CMAKE_CURRENT_BINARY_DIR}/../vst3base"
)

TARGET_LINK_LIBRARIES(lmms-vst3-host
	Qt${QT_VERSION_MAJOR}::Core
	Qt${QT_VERSION_MAJOR}::Network
)

SET_TARGET_PROPERTIES(lmms-vst3-host PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
)

INSTALL(TARGETS lmms-vst3-host
	RUNTIME DESTINATION "${PLUGIN_DIR}"
)
