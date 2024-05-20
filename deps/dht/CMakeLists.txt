FetchContent_Declare(
	dht
	GIT_REPOSITORY	https://github.com/b1gbakedbean/dht.git
	GIT_TAG			master
)

FetchContent_MakeAvailable(dht)

add_library(dht_lib STATIC ${dht_SOURCE_DIR}/dht.c)
target_compile_definitions(dht_lib PRIVATE _WIN32_WINNT=0x0600)
target_include_directories(dht_lib PUBLIC ${dht_SOURCE_DIR})