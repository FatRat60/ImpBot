cmake_minimum_required(VERSION 3.5.0)
set(PROJ_NAME Imp_Bot)
project(${PROJ_NAME} VERSION 0.1.0 LANGUAGES C CXX)

add_executable(${PROJ_NAME} 
    ${PROJECT_SOURCE_DIR}/src/ENV/parseENV.c
    ${PROJECT_SOURCE_DIR}/src/MUSIC/music_queue.cpp
    ${PROJECT_SOURCE_DIR}/src/MUSIC/youtube.cpp
    ${PROJECT_SOURCE_DIR}/src/MUSIC/spotify.cpp
    ${PROJECT_SOURCE_DIR}/src/MUSIC/music.cpp
    ${PROJECT_SOURCE_DIR}/src/SERVER/server.cpp
    ${PROJECT_SOURCE_DIR}/src/DISCORD/discord.cpp
    ${PROJECT_SOURCE_DIR}/src/main.cpp
)

target_include_directories(${PROJ_NAME} PRIVATE
    ${PROJECT_SOURCE_DIR}/src/ENV/
    ${PROJECT_SOURCE_DIR}/src/DISCORD/
    ${PROJECT_SOURCE_DIR}/src/MUSIC/
    ${PROJECT_SOURCE_DIR}/src/SERVER/
)

target_link_libraries(${PROJ_NAME} dpp oggz yaml-cpp)

set_target_properties(${PROJ_NAME} PROPERTIES
CXX_STANDARD 20
CXX_STANDARD_REQUIRED ON
)

add_custom_command(TARGET ${PROJ_NAME} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_directory
                    ${CMAKE_CURRENT_SOURCE_DIR}/resources/ $<TARGET_FILE_DIR:${PROJ_NAME}>/resources)
