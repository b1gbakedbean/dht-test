FetchContent_Declare(
    libtommath
    GIT_REPOSITORY https://github.com/libtom/libtommath.git
    GIT_TAG 5809141a3a6ec1bf3443c927c02b955e19224016
    OVERRIDE_FIND_PACKAGE
)

FetchContent_MakeAvailable(libtommath)