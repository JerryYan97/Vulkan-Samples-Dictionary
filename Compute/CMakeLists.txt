# Compute samples projects
get_directory_property(hasParent PARENT_DIRECTORY)

if(hasParent)
    # Has a parent scope
    include_directories(./MatrixMultiply)
else()
    # Doesn't have a parent scope. We only build project at top-level or bottom-level for all samples.
    message(FATAL_ERROR "You shouldn't build project in the Compute samples folder.")
endif()

