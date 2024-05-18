FetchContent_Declare(
    libtomcrypt
    GIT_REPOSITORY https://github.com/libtom/libtomcrypt.git
    GIT_TAG f7e6519fae1e11ff5ff9d36c84101a673002133b
)

FetchContent_MakeAvailable(libtomcrypt)

target_compile_definitions(libtomcrypt PUBLIC LTC_NO_PROTOTYPES LTC_NO_FAST LTC_NO_RSA_BLINDING LTC_NO_FILE ARGTYPE=4)