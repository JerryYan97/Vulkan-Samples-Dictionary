# add_library(SharedLibrary STATIC)

target_sources(
    SharedLibrary PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Event.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Event.h
)