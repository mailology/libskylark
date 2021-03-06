set(COMMON_TEST_LIBRARIES
  ${SKYLARK_LIBS}
  ${Elemental_LIBRARY}
  ${OPTIONAL_LIBS}
  ${Pmrrr_LIBRARY}
  ${Metis_LIBRARY}
  ${Boost_LIBRARIES})

add_executable(sparse_elemental_apply SparseSketchApplyElementalTest.cpp)
target_link_libraries(sparse_elemental_apply ${COMMON_TEST_LIBRARIES})
add_test( sparse_elemental_apply_test mpirun -np 1 ./sparse_elemental_apply )

add_executable(dense_elemental_apply DenseSketchApplyElementalTest.cpp)
target_link_libraries(dense_elemental_apply ${COMMON_TEST_LIBRARIES})
add_test( dense_elemental_apply_test mpirun -np 1 ./dense_elemental_apply )

add_executable(local_sparse_apply LocalSparseSketchApply.cpp)
target_link_libraries( local_sparse_apply ${COMMON_TEST_LIBRARIES})
add_test( local_sparse_apply_test mpirun -np 1 ./local_sparse_apply )

add_executable( dist_sparse_test DistSparseTest.cpp)
target_link_libraries( dist_sparse_test ${COMMON_TEST_LIBRARIES})
add_test( dist_sparse_test mpirun -np 5 ./dist_sparse_test )

#FIXME: same MPI_Finalize issue when compiled with USE_COMBBLAS
add_executable( internal_sparse_apply_test InternalSparseSketchApplyTest.cpp)
target_link_libraries( internal_sparse_apply_test ${COMMON_TEST_LIBRARIES})
add_test( internal_sparse_apply_test ./internal_sparse_apply_test)
#add_test( internal_sparse_apply_test mpirun -np 5 internal_sparse_apply_test)

add_executable(svd_elemental_test SVDElementalTest.cpp)
target_link_libraries(svd_elemental_test ${COMMON_TEST_LIBRARIES})
add_test( svd_elemental_test mpirun -np 1 ./svd_elemental_test )

add_executable(read_arc_list_test ReadArcList.cpp)
target_link_libraries(read_arc_list_test ${COMMON_TEST_LIBRARIES})
# add_test( read_arc_list_test mpirun -np 7 read_arc_list_test TEST_GRAPH )


#-----------------------------------------------------------------------------
# Tests depending on CombBLAS

 #FIXME: we have to find a way to fix the MPI_Finalize issue in hybrid mode
 #       with CombBLAS.
if (SKYLARK_HAVE_COMBBLAS AND !USE_HYBRID)

  add_executable(mixed_gemm MixedGemmTest.cpp)
  target_link_libraries(mixed_gemm
    ${COMMON_TEST_LIBRARIES}
    ${CombBLAS_LIBRARIES})
  add_test( mixed_gemm_test mpirun -np 4 ./mixed_gemm )

  add_executable(sparse_mixed_apply SparseSketchApplyMixedTest.cpp)
  target_link_libraries(sparse_mixed_apply
    ${COMMON_TEST_LIBRARIES}
    ${CombBLAS_LIBRARIES})
  add_test( sparse_mixed_apply_test mpirun -np 4 ./sparse_mixed_apply )

  add_executable(sparse_cb_apply SparseSketchApplyCombBLASTest.cpp)
  target_link_libraries( sparse_cb_apply
    ${COMMON_TEST_LIBRARIES}
    ${CombBLAS_LIBRARIES})
  add_test( sparse_cb_apply_test mpirun -np 1 ./sparse_cb_apply )

  add_executable(serialization_test SerializationTest.cpp)
  target_link_libraries( serialization_test
    ${COMMON_TEST_LIBRARIES}
    ${CombBLAS_LIBRARIES})
  add_test( serialization_test mpirun -np 1 ./serialization_test )

endif (SKYLARK_HAVE_COMBBLAS AND !USE_HYBRID)


#-----------------------------------------------------------------------------
# Python tests

find_package(PythonInterp REQUIRED)
#FIXME: Because of mpi4py/kdt MPI initialization issue we cannot run the test
#       in hybrid mode when we need to call MPI.Init_thread().
if(!USE_HYBRID)
  add_test(NAME ${NAME}
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/io_test.py ${CMAKE_SOURCE_DIR}
  )
endif(!USE_HYBRID)

# run all python tests
#file (GLOB PY_TEST_LIST RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.py )
#foreach (TEST ${PY_TEST_LIST})
#string (REGEX REPLACE "([a-zA-Z]+).py" "\\1" NAME ${TEST})

#add_test(NAME ${NAME}
#COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/${TEST} ${CMAKE_SOURCE_DIR}
#)
#endforeach (TEST)

