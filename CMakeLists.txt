cmake_minimum_required(VERSION 3.2.1)

project(FailsBot)

find_package(CURL REQUIRED)

set(SOURCES
	"main.cpp"
	)

include_directories(include)
include_directories(${CURL_INCLUDE_DIR})
add_subdirectory(botapi)
add_subdirectory(plugins)

source_group("Bot API" botapi)

add_executable(FailsBot ${SOURCES})
target_link_libraries(FailsBot ${CURL_LIBRARY}/libcurl_a.lib botapi)