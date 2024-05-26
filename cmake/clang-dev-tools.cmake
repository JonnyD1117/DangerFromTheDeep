# Additional target to perform clang-format run
# Requires clang-format

# Get all project files
# kinda weird solution. Global include paths are missing then. For clang-format it is sufficient
file(GLOB_RECURSE ALL_SOURCE_FILES *.cpp *.hpp)

add_custom_target(
        clang-format
        COMMAND /usr/bin/clang-format
        -style=file
        -i
        ${ALL_SOURCE_FILES}
)
