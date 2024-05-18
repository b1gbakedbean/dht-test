FetchContent_Declare(
	dht
	GIT_REPOSITORY	https://github.com/jech/dht.git
	GIT_TAG			0bbb8f4a5bd914b60de5e9fbb51573aa33a1cf18
)

FetchContent_MakeAvailable(dht)

add_library(dht_lib STATIC ${dht_SOURCE_DIR}/dht.c)
target_compile_definitions(dht_lib PRIVATE _WIN32_WINNT=0x0600)
target_include_directories(dht_lib PUBLIC ${dht_SOURCE_DIR})