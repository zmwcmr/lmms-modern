BUILD_PLUGIN(vst3base
	../vst3_base.cpp
	../Vst3Module.cpp ../Vst3Module.h
	../Vst3Scanner.cpp ../Vst3Scanner.h
	../Vst3List.cpp ../Vst3List.h
	../Vst3Protocol.h
	../Vst3SubPluginFeatures.cpp ../Vst3SubPluginFeatures.h
	EXPORT_BASE_NAME vst3base
	LINK SHARED
)

TARGET_INCLUDE_DIRECTORIES(vst3base
	PUBLIC ../
	PRIVATE ../travesty
)

FIND_PACKAGE(Qt${QT_VERSION_MAJOR} COMPONENTS Core REQUIRED)
TARGET_LINK_LIBRARIES(vst3base Qt${QT_VERSION_MAJOR}::Core)

IF(APPLE)
	FIND_LIBRARY(COREFOUNDATION_FRAMEWORK CoreFoundation)
	IF(COREFOUNDATION_FRAMEWORK)
		TARGET_LINK_LIBRARIES(vst3base "${COREFOUNDATION_FRAMEWORK}")
	ENDIF()
ENDIF()
