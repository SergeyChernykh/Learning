cmake_minimum_required(VERSION 3.16)

project(msqueue_exercise LANGUAGES CXX)

include(CTest)
find_package(Threads REQUIRED)

option(ENABLE_TSAN "Build tests with ThreadSanitizer" OFF)
option(ENABLE_ASAN "Build tests with AddressSanitizer and UBSan" OFF)
option(BUILD_BENCHMARKS "Build MSQueue benchmarks" ON)

function(configure_msqueue_target target)
    target_compile_features(${target} PRIVATE cxx_std_17)
    target_link_libraries(${target} PRIVATE Threads::Threads)
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
endfunction()

add_executable(msqueue_tests tests/msqueue_tests.cpp)
configure_msqueue_target(msqueue_tests)

if(BUILD_BENCHMARKS)
    add_executable(msqueue_benchmarks benchmarks/msqueue_benchmarks.cpp)
    configure_msqueue_target(msqueue_benchmarks)

    foreach(benchmark IN ITEMS sequential spsc mpmc)
        add_test(
            NAME "msqueue.benchmark.${benchmark}.smoke"
            COMMAND msqueue_benchmarks
                --benchmark "${benchmark}"
                --iterations 1000
                --producers 2
                --consumers 2
                --csv
        )
        set_tests_properties("msqueue.benchmark.${benchmark}.smoke"
            PROPERTIES TIMEOUT 30)
    endforeach()
endif()

if(ENABLE_ASAN)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(FATAL_ERROR "ENABLE_ASAN requires GCC or Clang")
    endif()
    target_compile_options(msqueue_tests PRIVATE
        -fsanitize=address,undefined -fno-omit-frame-pointer -g)
    target_link_options(msqueue_tests PRIVATE -fsanitize=address,undefined)
endif()

if(ENABLE_TSAN)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(FATAL_ERROR "ENABLE_TSAN requires GCC or Clang")
    endif()
    if(ENABLE_ASAN)
        message(FATAL_ERROR "ENABLE_ASAN and ENABLE_TSAN require separate builds")
    endif()
    target_compile_options(msqueue_tests PRIVATE
        -fsanitize=thread -fno-omit-frame-pointer -g)
    target_link_options(msqueue_tests PRIVATE -fsanitize=thread)
endif()

set(MSQUEUE_SCENARIOS
    empty
    sequential_fifo
    reuse_after_empty
    move_only
    destructor_lifetime
    multiple_producers_fifo
    multiple_consumers
    mixed_mpmc
)

foreach(scenario IN LISTS MSQUEUE_SCENARIOS)
    add_test(NAME "msqueue.${scenario}" COMMAND msqueue_tests "${scenario}")
    set_tests_properties("msqueue.${scenario}" PROPERTIES TIMEOUT 30)
endforeach()

if(ENABLE_ASAN)
    foreach(scenario IN LISTS MSQUEUE_SCENARIOS)
        set_tests_properties("msqueue.${scenario}" PROPERTIES
            ENVIRONMENT "ASAN_OPTIONS=detect_leaks=1:halt_on_error=1")
    endforeach()
endif()

if(ENABLE_TSAN)
    foreach(scenario IN LISTS MSQUEUE_SCENARIOS)
        set_tests_properties("msqueue.${scenario}" PROPERTIES
            ENVIRONMENT "TSAN_OPTIONS=halt_on_error=1")
    endforeach()
endif()
