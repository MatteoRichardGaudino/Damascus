cmake_minimum_required(VERSION 3.16)
project(Damascus C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

include(FetchContent)

# Fetch GLFW
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.3.8
)

# Fetch cglm (3D Math for C)
set(CGLM_SHARED OFF CACHE BOOL "" FORCE)
set(CGLM_STATIC ON CACHE BOOL "" FORCE)
FetchContent_Declare(
    cglm
    GIT_REPOSITORY https://github.com/recp/cglm.git
    GIT_TAG        v0.9.1
)

FetchContent_MakeAvailable(glfw cglm)

# Sources
set(SOURCES
    src/main.c
    src/window.c
    src/graphics.c
    src/camera.c
    src/game.c
    src/engine_random.c
    src/interaction.c
    src/ui.c
    third_party/glad/glad.c
)

add_executable(Damascus ${SOURCES})

target_include_directories(Damascus PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glad
    ${cglm_SOURCE_DIR}/include
    ${glfw_SOURCE_DIR}/include
)

target_link_libraries(Damascus PRIVATE
    glfw
    cglm
)

if(APPLE)
    target_link_libraries(Damascus PRIVATE "-framework OpenGL" "-framework Cocoa" "-framework IOKit" "-framework CoreVideo")
elseif(WIN32)
    target_link_libraries(Damascus PRIVATE opengl32)
else()
    target_link_libraries(Damascus PRIVATE GL m dl)
endif()

# Copy shaders directory to build output directory
add_custom_command(
    TARGET Damascus POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/shaders
            $<TARGET_FILE_DIR:Damascus>/shaders
)
