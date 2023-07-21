# add_library(SharedLibrary STATIC)

target_sources(
    # SharedLibrary PRIVATE
    ${MY_APP_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Camera.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Camera.h
    ${CMAKE_CURRENT_SOURCE_DIR}/CmdBufUtils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/CmdBufUtils.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Event.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Event.h
    ${CMAKE_CURRENT_SOURCE_DIR}/MathUtils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MathUtils.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Application.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Application.h
    ${CMAKE_CURRENT_SOURCE_DIR}/VulkanDbgUtils.h
)