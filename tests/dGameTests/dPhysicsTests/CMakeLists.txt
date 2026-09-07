set(DPHYSICS_TESTS
	"dpGridTests.cpp"
)
get_filename_component(thisFolderName ${CMAKE_CURRENT_SOURCE_DIR} NAME)
list(TRANSFORM DPHYSICS_TESTS PREPEND "${thisFolderName}/")
set(DPHYSICS_TESTS ${DPHYSICS_TESTS} PARENT_SCOPE)
