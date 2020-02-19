file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/dawn_515 DESTINATION ${CMAKE_CURRENT_BINARY_DIR} )

add_test(NAME p2p_dawn515_test COMMAND tests/p2p_tests/dawn_515/test.sh WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
set_property(TEST p2p_dawn515_test PROPERTY LABELS nonparallelizable_tests)
