cmake_minimum_required(VERSION 3.16)

project(learning_smart_pointers LANGUAGES CXX)

option(SMART_PTR_USE_STD
       "Test std::unique_ptr/std::shared_ptr/std::weak_ptr instead of my_smart_ptr.hpp"
       OFF)
option(SMART_PTR_ENABLE_SANITIZERS
       "Build with AddressSanitizer and UndefinedBehaviorSanitizer (GCC/Clang)"
       OFF)
option(SMART_PTR_ENABLE_TSAN
       "Build with ThreadSanitizer (GCC/Clang; incompatible with ASan)"
       OFF)

if(SMART_PTR_ENABLE_SANITIZERS AND SMART_PTR_ENABLE_TSAN)
    message(FATAL_ERROR
        "SMART_PTR_ENABLE_SANITIZERS and SMART_PTR_ENABLE_TSAN cannot be enabled together")
endif()

find_package(Threads REQUIRED)

add_executable(smart_ptr_tests
    tests/smart_ptr_tests.cpp
)

target_compile_features(smart_ptr_tests PRIVATE cxx_std_20)
target_include_directories(smart_ptr_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests"
)
target_link_libraries(smart_ptr_tests PRIVATE Threads::Threads)

if(SMART_PTR_USE_STD)
    target_compile_definitions(smart_ptr_tests PRIVATE SMART_PTR_USE_STD=1)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(smart_ptr_tests PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wshadow
    )

    if(SMART_PTR_ENABLE_SANITIZERS)
        target_compile_options(smart_ptr_tests PRIVATE
            -fsanitize=address,undefined
            -fno-omit-frame-pointer
        )
        target_link_options(smart_ptr_tests PRIVATE
            -fsanitize=address,undefined
        )
    endif()

    if(SMART_PTR_ENABLE_TSAN)
        target_compile_options(smart_ptr_tests PRIVATE
            -fsanitize=thread
            -fno-omit-frame-pointer
        )
        target_link_options(smart_ptr_tests PRIVATE
            -fsanitize=thread
        )

        # Older GCC/TSan combinations can collide with PIE mappings on Linux.
        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            target_compile_options(smart_ptr_tests PRIVATE -fno-pie)
            target_link_options(smart_ptr_tests PRIVATE -no-pie)
        endif()
    endif()
endif()

enable_testing()

set(SMART_PTR_TEST_COMMAND $<TARGET_FILE:smart_ptr_tests>)
if(SMART_PTR_ENABLE_TSAN AND UNIX)
    find_program(SMART_PTR_SETARCH_EXECUTABLE setarch)
    if(SMART_PTR_SETARCH_EXECUTABLE)
        # TSan needs the process personality without address randomization on
        # some Linux/GCC combinations (otherwise it reports an unexpected map).
        set(SMART_PTR_TEST_COMMAND
            ${SMART_PTR_SETARCH_EXECUTABLE}
            ${CMAKE_SYSTEM_PROCESSOR}
            -R
            $<TARGET_FILE:smart_ptr_tests>
        )
    endif()
endif()

add_test(NAME smart_ptr_basic
         COMMAND ${SMART_PTR_TEST_COMMAND} "[basic]")
add_test(NAME smart_ptr_advanced
         COMMAND ${SMART_PTR_TEST_COMMAND} "[advanced]")
add_test(NAME smart_ptr_exceptions
         COMMAND ${SMART_PTR_TEST_COMMAND} "[exceptions]")
add_test(NAME smart_ptr_threads
         COMMAND ${SMART_PTR_TEST_COMMAND} "[threads]")
add_test(NAME smart_ptr_all
         COMMAND ${SMART_PTR_TEST_COMMAND})
