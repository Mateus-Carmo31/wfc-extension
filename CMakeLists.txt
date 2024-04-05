# cmake -DCMAKE_BUILD_TYPE=Debug -DCUSTOMIZE_BUILD=On -DSUPPORT_FILEFORMAT_SVG=On -G "Unix Makefiles" ..

cmake_minimum_required(VERSION 3.2)
project (tcc C CXX)

set (CMAKE_C_STANDARD 11)
set (CMAKE_CXX_STANDARD 17)
set (CMAKE_EXPORT_COMPILE_COMMANDS ON)
set (CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g3 -ggdb")
set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g3 -ggdb")
# list (APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/external/

set(RAYLIB_VERSION 4.5.0)
# find_package(raylib ${RAYLIB_VERSION} REQUIRED)
# if (raylib_FOUND)
#   message(STATUS "Raylib installed in system. Including that.")
# else ()
  add_subdirectory(external/raylib)
# endif ()

set(EXAMPLES
    sudoku
    graph
    wfc
    wfc_overlap
    regions
)

foreach(EXAMPLE ${EXAMPLES})
    file(GLOB ${EXAMPLE}_SRC examples/${EXAMPLE}/*.cpp examples/${EXAMPLE}/*.c)
    add_executable (${EXAMPLE} ${${EXAMPLE}_SRC})
    target_link_libraries(${EXAMPLE} PRIVATE raylib)
    target_include_directories(${EXAMPLE} PRIVATE src/ examples/${EXAMPLE})

    if(NOT WIN32)
      target_compile_options(${EXAMPLE} PRIVATE -fsanitize=address)
      target_link_options(${EXAMPLE} PRIVATE -fsanitize=address)
    endif()
endforeach()

# file(GLOB SUDOKU_SRC examples/sudoku/*.c)

# add_executable (sudoku ${SUDOKU_SRC})
# target_link_libraries(sudoku PRIVATE raylib)
# target_include_directories(sudoku PRIVATE src/)

# # target_compile_options(sudoku PRIVATE -fsanitize=address)
# # target_link_options(sudoku PRIVATE -fsanitize=address)

# # file(GLOB VORONOI_SRC examples/voronoi/*.c)

# # add_executable (voronoi ${VORONOI_SRC})
# # target_link_libraries(voronoi PRIVATE raylib)
# # target_include_directories(voronoi PRIVATE src/)

# # target_compile_options(voronoi PRIVATE -fsanitize=address)
# # target_link_options(voronoi PRIVATE -fsanitize=address)

# file(GLOB WFC_SRC examples/wfc_simple/*.cpp examples/wfc_simple/*.c)

# add_executable (wfc ${WFC_SRC})
# target_link_libraries(wfc PRIVATE raylib)
# target_include_directories(wfc PRIVATE src/)
# target_include_directories(wfc PRIVATE examples/wfc_simple/*.h)

# # target_compile_options(wfc PRIVATE -fsanitize=address)
# # target_link_options(wfc PRIVATE -fsanitize=address)

# file(GLOB WFC_OVERLAP_SRC examples/wfc_overlap/*.cpp examples/wfc_overlap/*.c)

# add_executable (wfc_overlap ${WFC_OVERLAP_SRC})
# target_link_libraries(wfc_overlap PRIVATE raylib)
# target_include_directories(wfc_overlap PRIVATE src/)
# target_include_directories(wfc_overlap PRIVATE examples/wfc_overlap/*.h)

# # target_compile_options(wfc_overlap PRIVATE -fsanitize=address)
# # target_link_options(wfc_overlap PRIVATE -fsanitize=address)

# file(GLOB POETRY_SRC examples/poetry/*.cpp examples/poetry/*.c)

# add_executable (poetry ${POETRY_SRC})
# target_link_libraries(poetry PRIVATE raylib)
# target_include_directories(poetry PRIVATE src/)
# target_include_directories(poetry PRIVATE examples/poetry/*.h)

# target_compile_options(poetry PRIVATE -fsanitize=address)
# target_link_options(poetry PRIVATE -fsanitize=address)

# add_custom_command(
#   TARGET ${PROJECT_NAME}
#   POST_BUILD
#   COMMAND mv "${CMAKE_SOURCE_DIR}/build/compile_commands.json" "${CMAKE_SOURCE_DIR}/compile_commands.json"
#   COMMENT "Moving compile_commands.json..."
# )

# file(RENAME "${CMAKE_SOURCE_DIR}/build/compile_commands.json" "${CMAKE_SOURCE_DIR}/compile_commands.json")

