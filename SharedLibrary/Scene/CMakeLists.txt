# add_library(SharedLibrary STATIC)

target_sources(
    SharedLibrary PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Entity.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Entity.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Level.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Level.h
)