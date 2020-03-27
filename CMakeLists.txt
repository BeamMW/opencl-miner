cmake_minimum_required(VERSION 3.1) 
project(beam-opencl-miner)
set(TARGET_NAME beam-opencl-miner)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_STATIC_RUNTIME ON)

set(OPENSSL_USE_STATIC_LIBS TRUE)

find_package(Boost REQUIRED COMPONENTS system date_time thread)
if(Boost_FOUND)
  include_directories(${Boost_INCLUDE_DIRS})
  link_directories(${Boost_LIBRARY_DIRS})
endif()

if (MSVC)
    # use static runtime
    foreach(flag_var CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
        CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO
        flag_var CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
        CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO)

        if(${flag_var} MATCHES "/MD")
            string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
        endif(${flag_var} MATCHES "/MD")
    endforeach(flag_var)

    set(OPENSSL_USE_STATIC_LIBS TRUE)
    set(OPENSSL_MSVC_STATIC_RT TRUE)
endif()


find_package(OpenSSL REQUIRED)
if( OpenSSL_FOUND )   
    link_directories(${OPENSSL_LIBRARIES})
    include_directories(${OPENSSL_INCLUDE_DIR})
    message(STATUS "Using OpenSSL ${OPENSSL_VERSION}")
else()
    message(FATAL_ERROR "error, OpenSSL not found, please install: 'apt-get install libssl-dev' on Ubuntu")
endif()

find_package(OpenCL REQUIRED)
if(OpenCL_FOUND)
    include_directories(${OpenCL_INCLUDE_DIRS})
    link_directories(${OpenCL_LIBRARIES})
    message(STATUS "Using OpenCL ${OpenCL_VERSION_STRING}")
else()
    message(FATAL_ERROR "error, OpenCL not found")
endif()

set (HEADERS
    beam/utility/common.h
    beam/core/difficulty.h
)

set(MINER_SRC
    main.cpp
    crypto/sha256.c
    crypto/blake2b.cpp
    beam/core/difficulty.cpp
    beam/core/uintBig.cpp
    beam/utility/common.cpp
)

add_executable(${TARGET_NAME} ${MINER_SRC} ${HEADERS}  )

target_include_directories(${TARGET_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/beam)
target_include_directories(${TARGET_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/clHeaders)
target_include_directories(${TARGET_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/clHost)

add_subdirectory(solvers)
target_link_libraries(${TARGET_NAME} solvers)

add_subdirectory(clHost)
target_link_libraries(${TARGET_NAME} clHost)

add_subdirectory(stratum)
target_link_libraries(${TARGET_NAME} stratum)

if(UNIX)
    find_package(Threads)
    target_link_libraries(${TARGET_NAME} Threads::Threads)
    set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++")
endif()

if(MSVC)
    target_link_libraries(${TARGET_NAME} crypt32.lib)
    if(MSVC_VERSION GREATER_EQUAL 1900)
        target_link_libraries(${TARGET_NAME} legacy_stdio_definitions.lib)
    endif()
endif()

target_link_libraries(${TARGET_NAME} OpenCL::OpenCL)
target_link_libraries(${TARGET_NAME} ${OPENSSL_SSL_LIBRARY} ${OPENSSL_CRYPTO_LIBRARY})
target_link_libraries(${TARGET_NAME} ${Boost_LIBRARIES})
if(UNIX)
target_link_libraries(${TARGET_NAME} -ldl)
endif()
