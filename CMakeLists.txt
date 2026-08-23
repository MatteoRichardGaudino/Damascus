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
    src/engine_checkerboard.c
    src/engine_kingsrow.c
    src/mcts_ucb1.c
    src/mcts_puct.c
    src/zobrist.c
    src/transposition.c
    src/wld_db.c
    src/wld_egdb.c
    src/wld_solver.c
    src/opening_book.c
    src/tune_ga.c
    src/interaction.c
    src/ui.c
    src/cli.c
    third_party/glad/glad.c
)

add_executable(Damascus ${SOURCES})

target_include_directories(Damascus PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glad
    ${cglm_SOURCE_DIR}/include
    ${glfw_SOURCE_DIR}/include
)

find_package(Threads REQUIRED)

target_link_libraries(Damascus PRIVATE
    glfw
    cglm
    Threads::Threads
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

# Automated Test Suite
enable_testing()

add_executable(test_wld
    tests/test_wld.c
    src/game.c
    src/zobrist.c
    src/transposition.c
    src/mcts_ucb1.c
    src/mcts_puct.c
    src/wld_db.c
    src/wld_egdb.c
    src/wld_solver.c
    src/opening_book.c
)

target_include_directories(test_wld PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(WIN32)
    target_link_libraries(test_wld PRIVATE Threads::Threads)
else()
    target_link_libraries(test_wld PRIVATE Threads::Threads m)
endif()

add_test(NAME test_wld COMMAND test_wld)

add_executable(test_wld_solver
    tests/test_wld_solver.c
    src/game.c
    src/zobrist.c
    src/transposition.c
    src/mcts_ucb1.c
    src/mcts_puct.c
    src/wld_db.c
    src/wld_egdb.c
    src/wld_solver.c
    src/opening_book.c
)

target_include_directories(test_wld_solver PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(WIN32)
    target_link_libraries(test_wld_solver PRIVATE Threads::Threads)
else()
    target_link_libraries(test_wld_solver PRIVATE Threads::Threads m)
endif()

add_test(NAME test_wld_solver COMMAND test_wld_solver)

add_executable(test_opening_book
    tests/test_opening_book.c
    src/game.c
    src/opening_book.c
    src/zobrist.c
    src/transposition.c
    src/mcts_ucb1.c
    src/mcts_puct.c
    src/wld_db.c
    src/wld_egdb.c
    src/wld_solver.c
)

target_include_directories(test_opening_book PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(WIN32)
    target_link_libraries(test_opening_book PRIVATE Threads::Threads)
else()
    target_link_libraries(test_opening_book PRIVATE Threads::Threads m)
endif()

add_test(NAME test_opening_book COMMAND test_opening_book)

add_executable(test_cb_study
    tests/test_cb_study.c
    src/game.c
    src/engine_checkerboard.c
    src/opening_book.c
    src/zobrist.c
    src/transposition.c
    src/mcts_ucb1.c
    src/mcts_puct.c
    src/wld_db.c
    src/wld_egdb.c
    src/wld_solver.c
)

target_include_directories(test_cb_study PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(WIN32)
    target_link_libraries(test_cb_study PRIVATE Threads::Threads)
else()
    target_link_libraries(test_cb_study PRIVATE Threads::Threads m)
endif()

add_executable(test_tune_ga
    tests/test_tune_ga.c
    src/game.c
    src/engine_random.c
    src/engine_checkerboard.c
    src/engine_kingsrow.c
    src/mcts_ucb1.c
    src/mcts_puct.c
    src/zobrist.c
    src/transposition.c
    src/wld_db.c
    src/wld_egdb.c
    src/wld_solver.c
    src/opening_book.c
    src/tune_ga.c
)

target_include_directories(test_tune_ga PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(WIN32)
    target_link_libraries(test_tune_ga PRIVATE Threads::Threads)
else()
    target_link_libraries(test_tune_ga PRIVATE Threads::Threads m)
endif()

add_test(NAME test_tune_ga COMMAND test_tune_ga)

set_tests_properties(test_wld test_wld_solver test_opening_book test_tune_ga PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")


