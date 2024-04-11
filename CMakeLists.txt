cmake_minimum_required(VERSION 3.0.0)
project(Term VERSION 0.1.0 LANGUAGES C)
set(C_STANDARD 11)

include(CTest)
enable_testing()

set (
    TERM_SOURCE_FILES

    src/main.c
    src/list.h
    src/file.c src/file.h
    src/input.c src/input.h
    src/window.c src/window.h
    src/grid.c src/grid.h
    src/color.c src/color.h
    src/reader.c src/reader.h
    src/text_buffer.c src/text_buffer.h
    src/pseudo_console.c src/pseudo_console.h
    src/graphics/mesh.c src/graphics/mesh.h
    src/graphics/resources.c src/graphics/resources.h
    src/graphics/renderer.c src/graphics/renderer.h
    src/graphics/sprite_batch.c src/graphics/sprite_batch.h
)

add_executable(
    Term

    ${TERM_SOURCE_FILES}
    deps/glad/src/glad.c
)
target_include_directories(Term PRIVATE deps/glad/include deps/stb_image/include)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(deps/glfw)

add_subdirectory(deps/cglm)

target_link_libraries(Term PRIVATE glfw cglm)

if(NOT MSVC)
    set_source_files_properties(${TERM_SOURCE_FILES} PROPERTIES COMPILE_FLAGS -Wall -Werror -Wpedantic)
endif()

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
