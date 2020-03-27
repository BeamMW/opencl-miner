set(SOURCES
    beamStratum.cpp beamStratum.h
)

add_library(stratum ${SOURCES})
target_include_directories(stratum PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../)
target_include_directories(stratum PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../beam)
