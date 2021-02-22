cmake_minimum_required(VERSION 3.16)
project(jshn-jsonc C)

set(CMAKE_C_STANDARD 11)

include(FindPkgConfig)
pkg_check_modules(JSONC json-c)
add_definitions(-DJSONC)
include_directories(${JSONC_INCLUDE_DIRS})
find_library(jsonc NAMES json-c)

add_executable(jshn jshn.c)
target_link_libraries(jshn ${jsonc})

install(TARGETS jshn
	ARCHIVE DESTINATION lib
	LIBRARY DESTINATION lib
	RUNTIME DESTINATION bin
)
file(GLOB scripts sh/*.sh)
install(FILES ${scripts}
	DESTINATION share/libubox
)
