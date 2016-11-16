INCLUDE_DIRECTORIES("${CMAKE_CURRENT_BINARY_DIR}")

# find libbgpdump
LINK_DIRECTORIES("${PROJECT_SOURCE_DIR}/external/bgpdump")



# test btree

# test fstree

# test ptree

# test mptree 
ADD_EXECUTABLE(test_mptree test_mptree.cpp)


