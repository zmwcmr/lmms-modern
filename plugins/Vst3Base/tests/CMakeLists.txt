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
