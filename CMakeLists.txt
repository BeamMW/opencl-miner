cmake_minimum_required(VERSION 3.1) 
set(TARGET_NAME beamMiner)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost REQUIRED)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_STATIC_RUNTIME ON)

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

endif()


find_package(OpenSSL REQUIRED)

if( OpenSSL_FOUND )
    set(OPENSSL_USE_STATIC_LIBS TRUE)
    link_directories(${OPENSSL_LIBRARIES})
    include_directories(${OPENSSL_INCLUDE_DIR})
    message(STATUS "Using OpenSSL ${OPENSSL_VERSION}")
    set(OPENSSL_LIBCRYPTO OpenSSL::Crypto)
    set(OPENSSL_LIBSSL OpenSSL::SSL)
else()
    message(FATAL_ERROR "error, OpenSSL not found, please install: 'apt-get install libssl-dev' on Ubuntu")
endif()

if(MSVC)
    include_directories($ENV{OPENSSL_ROOT_DIR}/include)
endif()

# cmake can't find static lib on macos, harcoded
if(APPLE)
    set(OPENSSL_LIBCRYPTO $ENV{OPENSSL_ROOT_DIR}/lib/libcrypto.a)
    set(OPENSSL_LIBSSL $ENV{OPENSSL_ROOT_DIR}/lib/libssl.a)
elseif(UNIX)
    set(OPENSSL_LIBCRYPTO /usr/lib/x86_64-linux-gnu/libcrypto.a)
    set(OPENSSL_LIBSSL /usr/lib/x86_64-linux-gnu/libssl.a)
elseif(MSVC)
    set(OPENSSL_LIBCRYPTO $ENV{OPENSSL_ROOT_DIR}/lib/VC/static/libcrypto64MT.lib crypt32.lib)
    set(OPENSSL_LIBSSL $ENV{OPENSSL_ROOT_DIR}/lib/VC/static/libssl64MT.lib)

    if(MSVC_VERSION GREATER_EQUAL 1900)
        set(OPENSSL_LIBCRYPTO ${OPENSSL_LIBCRYPTO} legacy_stdio_definitions.lib)
    endif()
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
    beamStratum.cpp
    clHost.cpp
    main.cpp
    crypto/sha256.c
    beam/core/difficulty.cpp
    beam/core/uintBig.cpp
    beam/utility/common.cpp
)

add_executable(${TARGET_NAME} ${MINER_SRC} ${HEADERS}  )

target_include_directories(${TARGET_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/beam)

target_link_libraries(${TARGET_NAME} OpenCL::OpenCL)

target_link_libraries(${TARGET_NAME} ${OPENSSL_LIBSSL} ${OPENSSL_LIBCRYPTO})
