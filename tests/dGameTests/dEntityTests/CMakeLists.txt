set(DENTITY_TESTS
	"EntityManagerTests.cpp"
	"EntityLifecycleTests.cpp"
)
get_filename_component(thisFolderName ${CMAKE_CURRENT_SOURCE_DIR} NAME)
list(TRANSFORM DENTITY_TESTS PREPEND "${thisFolderName}/")
set(DENTITY_TESTS ${DENTITY_TESTS} PARENT_SCOPE)
