cmake_minimum_required(VERSION 3.1) 
set(TARGET_NAME beamMiner)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#include_directories($ENV{BOOST_ROOT}/include)
#link_directories($ENV{BOOST_ROOT})

if (WIN32)
    add_definitions(-DNOMINMAX)
    find_package(Boost COMPONENTS system date_time regex thread)
    if (Boost_FOUND)
        include_directories(${Boost_INCLUDE_DIRS})
        add_definitions( -DBOOST_ALL_DYN_LINK )
    endif()
endif()

if (Boost_FOUND)
    find_package(Boost COMPONENTS system date_time regex thread)
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

target_link_libraries(${TARGET_NAME} Boost::system Boost::date_time Boost::regex Boost::thread OpenSSL::Crypto OpenSSL::SSL OpenCL::OpenCL)

