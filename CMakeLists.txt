cmake_minimum_required(VERSION 3.9.0)
project(cdcm)
file(READ "build/version.txt" FULL_VER)
string(REGEX MATCH "([0-9]+\\.[0-9]+\\.[0-9]+)\\.([0-9]+)_([A-Z]+)" outvar  ${FULL_VER})
set(VERSION ${CMAKE_MATCH_1})
set(BUILD_TYPE ${CMAKE_MATCH_2})
set(RELEASE ${CMAKE_MATCH_3})
file(READ cdcm_settings.json settings_file_content)
string(REGEX REPLACE "\"cdcm_version\": .+\"\n"  "\"cdcm_version\": \"${VERSION}\"\n" settings_file_content "${settings_file_content}")
message(STATUS ${settings_file_content})
file(WRITE  cdcm_settings.json "${settings_file_content}")

add_subdirectory(src)