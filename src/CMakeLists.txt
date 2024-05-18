list(APPEND dht_test_SOURCES
	pch.cpp
	main.cpp
)

add_executable(dht_test ${dht_test_SOURCES})
target_compile_features(dht_test PRIVATE cxx_std_17)
target_precompile_headers(dht_test PRIVATE pch.hpp)
target_compile_definitions(dht_test PRIVATE WIN32_LEAN_AND_MEAN)
target_link_libraries(dht_test PRIVATE Ws2_32 fmt::fmt-header-only dht_lib libtomcrypt)