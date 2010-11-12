# Include the directory itself as a path to include directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# select all c-files in this directory
file(GLOB coolmud_SOURCES *.c)

# generate the parser
set(BISON_TARGET_cmdopt -y)
bison_target(Parser cool.y ${CMAKE_CURRENT_BINARY_DIR}/parser.c)
#include_directories(${CMAKE_CURRENT_BINARY_DIR})


# Create the coolmud executable
add_executable(coolmud ${coolmud_SOURCES} ${BISON_Parser_OUTPUTS})
target_link_libraries(coolmud ${GDBM_LIBRARY} ${GDBM_LIBRARY_COMPAT} "crypt")

