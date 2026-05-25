set(DCOMMONTEST_SOURCES
	"AMFDeserializeTests.cpp"
	"Amf3Tests.cpp"
	"ToUnderlyingTests.cpp"
	"HeaderSkipTest.cpp"
	"TestCDFeatureGatingTable.cpp"
	"TestLDFFormat.cpp"
	"TestNiPoint3.cpp"
	"TestEncoding.cpp"
	"TestLUString.cpp"
	"TestLUWString.cpp"
	"dCommonDependencies.cpp"
	"LxfmlTests.cpp"
	"CDClientTableTests.cpp"
	"ConfigTests.cpp"
	"BitStreamExtendedTests.cpp"
)

add_subdirectory(dEnumsTests)
list(APPEND DCOMMONTEST_SOURCES ${DENUMS_TESTS})

# Set our executable
add_executable(dCommonTests ${DCOMMONTEST_SOURCES})

# Needs to be in binary dir for ctest 
if(APPLE)
	add_custom_target(dCommonTestsLink
		${CMAKE_COMMAND} -E copy $<TARGET_FILE:MariaDB::ConnCpp> ${CMAKE_CURRENT_BINARY_DIR})

	add_dependencies(dCommonTests dCommonTestsLink)
endif()

# Link needed libraries
target_link_libraries(dCommonTests ${COMMON_LIBRARIES} GTest::gtest_main)

target_compile_definitions(dCommonTests PRIVATE
	CDCLIENT_TEST_PATH="${CMAKE_SOURCE_DIR}/resServer/CDServer.sqlite")

# Copy test files to testing directory
add_subdirectory(TestBitStreams)
file(COPY ${TESTBITSTREAMS} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
add_subdirectory(LxfmlTestFiles)
file(COPY ${LXFMLTESTFILES} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

# Discover the tests. DISCOVERY_MODE PRE_TEST defers gtest's enumeration step
# from build time to ctest time. Without this, a sanitizer build (the `asan`
# preset) fails because ASAN's startup overhead pushes the build-time test
# enumeration past gtest_discover_tests's timeout, and make then deletes the
# (successfully linked) binary as a casualty of the discovery-step failure.
# DISCOVERY_TIMEOUT raised from the 5s default for the same reason.
gtest_discover_tests(dCommonTests
	DISCOVERY_MODE PRE_TEST
	DISCOVERY_TIMEOUT 60)
