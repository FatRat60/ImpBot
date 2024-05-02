cmake_minimum_required(VERSION 3.5.0)
set(PROJ_NAME Imp_Bot)
project(${PROJ_NAME} VERSION 0.1.0 LANGUAGES C CXX)

file(GLOB_RECURSE SRCS ${PROJECT_SOURCE_DIR}/src/*.cpp)
add_executable(${PROJ_NAME} ${SRCS})

target_link_libraries(${PROJ_NAME} dpp)

set_target_properties(${PROJECT_NAME} PROPERTIES
CXX_STANDARD 17
CXX_STANDARD_REQUIRED ON
)

add_custom_command(TARGET ${PROJ_NAME} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_directory
                    ${CMAKE_CURRENT_SOURCE_DIR}/resources/ $<TARGET_FILE_DIR:${PROJ_NAME}>/resources)
