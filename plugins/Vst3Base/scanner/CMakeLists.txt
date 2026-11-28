ADD_EXECUTABLE(lmms-vst3-scanner
	main.cpp
	../Vst3Module.cpp
	../Vst3Scanner.cpp
	../Vst3List.cpp
	../Vst3Protocol.h
	"${CMAKE_SOURCE_DIR}/src/core/PluginScanCache.cpp"
	"${CMAKE_SOURCE_DIR}/src/core/PluginScanResult.cpp"
)

TARGET_INCLUDE_DIRECTORIES(lmms-vst3-scanner
	PRIVATE
		../
		../travesty
		"${CMAKE_CURRENT_BINARY_DIR}/../vst3base"
		"${CMAKE_SOURCE_DIR}/include"
)

# This executable compiles Vst3List.cpp directly instead of linking vst3base,
# so it must not see the DLL import declarations from vst3base_export.h.
TARGET_COMPILE_DEFINITIONS(lmms-vst3-scanner PRIVATE VST3BASE_STATIC_DEFINE)

TARGET_LINK_LIBRARIES(lmms-vst3-scanner Qt${QT_VERSION_MAJOR}::Core)

IF(APPLE)
	FIND_LIBRARY(COREFOUNDATION_FRAMEWORK CoreFoundation)
	IF(COREFOUNDATION_FRAMEWORK)
		TARGET_LINK_LIBRARIES(lmms-vst3-scanner "${COREFOUNDATION_FRAMEWORK}")
	ENDIF()
ENDIF()

SET_TARGET_PROPERTIES(lmms-vst3-scanner PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
)

INSTALL(TARGETS lmms-vst3-scanner
	RUNTIME DESTINATION "${PLUGIN_DIR}"
)
