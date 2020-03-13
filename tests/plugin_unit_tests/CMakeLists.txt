
find_package( Gperftools QUIET )
if( GPERFTOOLS_FOUND )
    message( STATUS "Found gperftools; compiling tests with TCMalloc")
    list( APPEND PLATFORM_SPECIFIC_LIBS tcmalloc )
endif()

include_directories("${CMAKE_SOURCE_DIR}/plugins/wallet_plugin/include")

file(GLOB UNIT_TESTS "*.cpp")

add_executable( plugin_test ${UNIT_TESTS} )
target_link_libraries( plugin_test eosio_testing eosio_chain chainbase chain_plugin wallet_plugin fc ${PLATFORM_SPECIFIC_LIBS} )

target_include_directories( plugin_test PUBLIC
                            ${CMAKE_SOURCE_DIR}/plugins/net_plugin/include
                            ${CMAKE_SOURCE_DIR}/plugins/chain_plugin/include
                            ${CMAKE_BINARY_DIR}/unittests/include/ )

#To run plugin_test with all log from blockchain displayed, put --verbose after --, i.e. plugin_test -- --verbose
add_test(NAME plugin_test COMMAND plugin_test --report_level=detailed --color_output)


