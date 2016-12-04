INCLUDE_DIRECTORIES("${CMAKE_CURRENT_BINARY_DIR}")


# test rbt
ADD_EXECUTABLE(test_rbt test_rbt.cpp)

# test rpt
ADD_EXECUTABLE(test_rpt test_rpt.cpp)

# test rfst
ADD_EXECUTABLE(test_rfst test_rfst.cpp)

# test rfst
ADD_EXECUTABLE(test_fst test_fst.cpp)

# test rmpt
ADD_EXECUTABLE(test_rmpt test_rmpt.cpp)

#test test_u128
ADD_EXECUTABLE(test_u128 test_u128.cpp)
