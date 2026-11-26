# Builds the isolated LV2 host child (P6.4).
#
# This is a plain executable, not a plugin module: it links only the transport
# and shared-memory helpers and must not depend on the LMMS GUI. It is started
# by Lv2HostClient, one process per plugin instance.

IF(NOT LMMS_HAVE_LV2)
	RETURN()
ENDIF()

ADD_EXECUTABLE(lmms-lv2-host
	Lv2Host.cpp
	${CMAKE_SOURCE_DIR}/src/common/RemotePluginBase.cpp
	${CMAKE_SOURCE_DIR}/src/common/SharedMemory.cpp
	${CMAKE_SOURCE_DIR}/src/common/SystemSemaphore.cpp
)

TARGET_INCLUDE_DIRECTORIES(lmms-lv2-host PRIVATE
	"${CMAKE_SOURCE_DIR}/include"
	"${CMAKE_BINARY_DIR}"
	${LV2_INCLUDE_DIRS}
	${Lilv_INCLUDE_DIRS}
)

TARGET_COMPILE_DEFINITIONS(lmms-lv2-host PRIVATE BUILD_REMOTE_PLUGIN_CLIENT)
TARGET_COMPILE_FEATURES(lmms-lv2-host PRIVATE cxx_std_20)

TARGET_LINK_LIBRARIES(lmms-lv2-host PRIVATE Lilv::lilv)

IF(LMMS_BUILD_WIN32)
	TARGET_LINK_LIBRARIES(lmms-lv2-host PRIVATE ws2_32)
ELSE()
	TARGET_LINK_LIBRARIES(lmms-lv2-host PRIVATE pthread)
ENDIF()
IF(LMMS_HAVE_LIBRT)
	TARGET_LINK_LIBRARIES(lmms-lv2-host PRIVATE rt)
ENDIF()

# Inherit CMAKE_RUNTIME_OUTPUT_DIRECTORY from plugins/CMakeLists.txt so the
# binary lands next to the plugin modules and is found through PLUGIN_DIR.

INSTALL(TARGETS lmms-lv2-host RUNTIME DESTINATION "${PLUGIN_DIR}")
