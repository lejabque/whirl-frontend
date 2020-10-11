set(REQUIRED_CXX_COMPILER "Clang")
set(CXX_COMPILER_MIN_VERSION 8.0)

message(STATUS "Checking compiler...")

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL REQUIRED_CXX_COMPILER)
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS CXX_COMPILER_MIN_VERSION)
        message(FATAL_ERROR
                "Old version of ${REQUIRED_CXX_COMPILER} compiler: ${CMAKE_CXX_COMPILER_VERSION}, required ${CXX_COMPILER_MIN_VERSION}."
        )
    endif()
else()
    message(FATAL_ERROR
            "Unsupported compiler: ${CMAKE_CXX_COMPILER_ID}. Use ${REQUIRED_CXX_COMPILER}, version >= ${CXX_COMPILER_MIN_VERSION}."
    )
endif()
