set(DCOMBAT_TESTS
	"BaseCombatAITests.cpp"
)
get_filename_component(thisFolderName ${CMAKE_CURRENT_SOURCE_DIR} NAME)
list(TRANSFORM DCOMBAT_TESTS PREPEND "${thisFolderName}/")
set(DCOMBAT_TESTS ${DCOMBAT_TESTS} PARENT_SCOPE)
