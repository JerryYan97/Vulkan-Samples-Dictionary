target_sources(
    SharedLibrary PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/StrPathUtils.h
    ${CMAKE_CURRENT_SOURCE_DIR}/StrPathUtils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/CmdBufUtils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/CmdBufUtils.h
    ${CMAKE_CURRENT_SOURCE_DIR}/VulkanDbgUtils.h
    ${CMAKE_CURRENT_SOURCE_DIR}/DataGenUtils.h
    ${CMAKE_CURRENT_SOURCE_DIR}/DataGenUtils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MathUtils.h
    ${CMAKE_CURRENT_SOURCE_DIR}/MathUtils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AppUtils.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AppUtils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/DiskOpsUtils.h
    ${CMAKE_CURRENT_SOURCE_DIR}/DiskOpsUtils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/GltfUtils.h
    ${CMAKE_CURRENT_SOURCE_DIR}/GltfUtils.cpp
)