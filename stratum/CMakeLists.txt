set(SOURCES
    beamStratum.cpp beamStratum.h
)

add_library(stratum ${SOURCES})
target_link_libraries(stratum Boost::boost OpenSSL::SSL OpenSSL::Crypto)
target_include_directories(stratum PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../)
target_include_directories(stratum PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../beam)
