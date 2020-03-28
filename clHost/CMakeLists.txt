set(SOURCES
    clHost.cpp clHost.h
)

add_library(clHost ${SOURCES})
target_include_directories(clHost PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(clHost PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../)
target_include_directories(clHost PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../beam)
target_include_directories(clHost PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../solvers)
target_include_directories(clHost PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../stratum)
target_include_directories(clHost PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../clHeaders)
