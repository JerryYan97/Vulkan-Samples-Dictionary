# add_library(SharedLibrary STATIC)

target_sources(
    SharedLibrary PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/AssetsLoader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AssetsLoader.h
    ${CMAKE_CURRENT_SOURCE_DIR}/GltfLoader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/GltfLoader.h
)