add_executable(ase-utils-test
    strmatch_test.cpp
)

#[[
The doctest entry point is switched on here rather than through a define in the test source, so
the test file stays free of macro definitions — same arrangement as foundation/ase-math/tests.
]]
target_compile_definitions(ase-utils-test
    PRIVATE
        DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
)

target_link_libraries(ase-utils-test
    PRIVATE
        ase::utils
        doctest::doctest
)

add_test(NAME ase-utils-test COMMAND ase-utils-test)

set_target_properties(ase-utils-test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
