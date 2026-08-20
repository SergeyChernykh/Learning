cmake_minimum_required(VERSION 3.16)

project(treiber_stack_tests LANGUAGES CXX)

include(CTest)
find_package(Threads REQUIRED)

option(ENABLE_TSAN "Build the tests with ThreadSanitizer" ON)
option(ENABLE_ASAN "Build the tests with AddressSanitizer and LeakSanitizer" OFF)
option(BUILD_BENCHMARKS "Build TStack microbenchmarks" ON)
option(TSAN_DISABLE_ASLR
    "Run TSAN tests with ASLR disabled to avoid shadow-memory mapping conflicts"
    ON
)

add_executable(tstack_tests tests/tstack_tests.cpp)
target_compile_features(tstack_tests PRIVATE cxx_std_17)
target_link_libraries(tstack_tests PRIVATE Threads::Threads)

if(BUILD_BENCHMARKS)
    add_executable(tstack_benchmarks benchmarks/tstack_benchmarks.cpp)
    target_compile_features(tstack_benchmarks PRIVATE cxx_std_17)
    target_link_libraries(tstack_benchmarks PRIVATE Threads::Threads)
endif()

if(ENABLE_ASAN)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_executable(tstack_tests_asan tests/tstack_tests.cpp)
        target_compile_features(tstack_tests_asan PRIVATE cxx_std_17)
        target_link_libraries(tstack_tests_asan PRIVATE Threads::Threads)
        target_compile_options(tstack_tests_asan PRIVATE
            -fsanitize=address,undefined
            -fno-omit-frame-pointer
            -g
        )
        target_link_options(tstack_tests_asan PRIVATE
            -fsanitize=address,undefined
        )
    else()
        message(FATAL_ERROR "ENABLE_ASAN requires GCC or Clang")
    endif()
endif()

if(ENABLE_TSAN)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_executable(tstack_tests_tsan tests/tstack_tests.cpp)
        target_compile_features(tstack_tests_tsan PRIVATE cxx_std_17)
        target_link_libraries(tstack_tests_tsan PRIVATE Threads::Threads)
        target_compile_options(tstack_tests_tsan PRIVATE
            -fsanitize=thread
            -fno-omit-frame-pointer
            -g
        )
        target_link_options(tstack_tests_tsan PRIVATE -fsanitize=thread)
    else()
        message(FATAL_ERROR "ENABLE_TSAN requires GCC or Clang")
    endif()
endif()

function(add_tstack_test target prefix scenario)
    add_test(NAME "${prefix}.${scenario}" COMMAND "${target}" "${scenario}")
    set_tests_properties("${prefix}.${scenario}" PROPERTIES TIMEOUT 30)
endfunction()

function(add_tstack_tsan_test scenario)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND TSAN_DISABLE_ASLR)
        find_program(SETARCH_EXECUTABLE setarch)
        if(NOT SETARCH_EXECUTABLE)
            message(FATAL_ERROR
                "TSAN_DISABLE_ASLR is ON, but the setarch utility was not found"
            )
        endif()

        add_test(
            NAME "tstack.tsan.${scenario}"
            COMMAND
                "${SETARCH_EXECUTABLE}"
                "${CMAKE_SYSTEM_PROCESSOR}"
                -R
                "$<TARGET_FILE:tstack_tests_tsan>"
                "${scenario}"
        )
    else()
        add_test(
            NAME "tstack.tsan.${scenario}"
            COMMAND tstack_tests_tsan "${scenario}"
        )
    endif()

    set_tests_properties("tstack.tsan.${scenario}" PROPERTIES
        TIMEOUT 30
        ENVIRONMENT "TSAN_OPTIONS=halt_on_error=1"
    )
endfunction()

set(TSTACK_TEST_SCENARIOS
    empty
    lifo
    reuse_after_empty
    move_only_value
    throwing_move_reclaims_popped_node
    destructor_reclaims_remaining_nodes
    pop_reclaims_retired_node_on_destruction
    concurrent_pop_reclaims_nodes_on_destruction
    concurrent_push
    concurrent_pop
    concurrent_push_pop
)

foreach(scenario IN LISTS TSTACK_TEST_SCENARIOS)
    add_tstack_test(tstack_tests tstack "${scenario}")

    if(ENABLE_TSAN)
        add_tstack_tsan_test("${scenario}")
    endif()

    if(ENABLE_ASAN)
        add_tstack_test(tstack_tests_asan tstack.asan "${scenario}")
        set_tests_properties("tstack.asan.${scenario}" PROPERTIES
            ENVIRONMENT "ASAN_OPTIONS=detect_leaks=1:halt_on_error=1"
        )
    endif()
endforeach()
