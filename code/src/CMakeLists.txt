INCLUDE_DIRECTORIES("${CMAKE_CURRENT_BINARY_DIR}")

# find libbgpdump
LINK_DIRECTORIES("${PROJECT_SOURCE_DIR}/external/bgpdump")

#analyzer
ADD_EXECUTABLE(analyzer analyzer.cpp)
TARGET_LINK_LIBRARIES(analyzer libbgpdump.so)




