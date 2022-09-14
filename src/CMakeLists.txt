file (GLOB HydraList_SRCS "*.cpp")
target_sources(hydralist PRIVATE
    ${HydraList_SRCS}
)
target_include_directories(hydralist PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
# add_executable (main main.cpp)
# target_link_libraries (main hydralist numa jemalloc art)
# INSTALL(TARGETS hydralist
#         ARCHIVE DESTINATION ${CMAKE_SOURCE_DIR}
#         )
