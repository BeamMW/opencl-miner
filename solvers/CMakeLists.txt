set(SOURCES
    beamHash_III_solver.cpp  beamHash_II_solver.cpp beamSolvers.h
)

add_library(solvers ${SOURCES})
target_include_directories(solvers PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/kernels)
target_include_directories(solvers PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../)
target_include_directories(solvers PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../beam)
target_include_directories(solvers PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../clHeaders)
