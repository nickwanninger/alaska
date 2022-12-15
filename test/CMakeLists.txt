

function(compile_test name binary)
    add_test(
        NAME ${name}
        COMMAND ${CMAKE_SOURCE_DIR}/tools/test_runner.sh ${name} ${CMAKE_CURRENT_SOURCE_DIR}/${binary}
    )
endfunction()


message(STATUS ${CMAKE_CTEST_COMMAND})
add_custom_target(verbose_test COMMAND ${CMAKE_CTEST_COMMAND}
    --force-new-ctest-process
    --progress
    --output-on-failure
    # -j 8
)


FILE(GLOB ALASKA_TESTS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} alaska/*.c)

FOREACH(subdir ${ALASKA_TESTS})
    compile_test(${subdir} ${subdir})
ENDFOREACH()

# compile_test(alaska/alloc.c alaska/alloc.c)
