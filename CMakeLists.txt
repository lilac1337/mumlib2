#
# CMake/Global Settings
#

set(MUMLIB2_VERSION "1.0.0")

cmake_minimum_required(VERSION 3.13)
project(mumlib2 LANGUAGES CXX VERSION ${MUMLIB2_VERSION})

include(CMakePackageConfigHelpers)
include(GenerateExportHeader)
include(GNUInstallDirs)


#
# Dependencies
#

find_package(asio REQUIRED)
find_package(Protobuf REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Opus CONFIG REQUIRED)



#
# Variabls
#

set(MUMLIB2_STANDALONE OFF)
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(MUMLIB2_STANDALONE ON)
endif()
set(CMAKE_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}")



#
# Options
#

if(MUMLIB2_STANDALONE AND NOT DEFINED BUILD_SHARED_LIBS)
	set(BUILD_SHARED_LIBS ON)
endif()
option(MUMLIB2_BUILD_SHARED_LIBS "Build shared libraries (.dll/.so) instead of static ones (.lib/.a)" ${BUILD_SHARED_LIBS})
option(MUMLIB2_BUILD_EXAMPLE "Build example" ${MUMLIB2_STANDALONE})

if(MUMLIB2_BUILD_SHARED_LIBS)
	set(MUMLIB2_LIBRARY_TYPE SHARED)
    set(MUMLIB2_DEPS_VISIBLITY PRIVATE)
else()
	set(MUMLIB2_LIBRARY_TYPE STATIC)
    set(MUMLIB2_DEPS_VISIBLITY PUBLIC)
endif()



#
# Protobuf
#

protobuf_generate_cpp(MUMLIB2_SOURCES_PROTO MUMLIB2_HEADERS_PROTO "src_proto/mumble.proto")



#
# Library
#

add_library(mumlib2 ${MUMLIB2_LIBRARY_TYPE})

# Library/Sources
set(MUMLIB2_SOURCES
    src/audio_decoder.cpp
    src/audio_decoder_session.cpp
    src/audio_encoder.cpp
    src/audio_packet.cpp
    src/crypto_state.cpp
    src/Logger.cpp
    src/mumlib2.cpp
    src/mumlib2_private.cpp
    src/Transport.cpp
    src/VarInt.cpp
)

set(MUMLIB2_HEADERS
    include/mumlib2.h 
    include/mumlib2/callback.h
    include/mumlib2/constants.h
    include/mumlib2/enums.h
    include/mumlib2/exceptions.h
    include/mumlib2/logger.h
    include/mumlib2/structs.h

    include/mumlib2_private/audio_decoder.h
    include/mumlib2_private/audio_decoder_session.h
    include/mumlib2_private/audio_encoder.h
    include/mumlib2_private/audio_packet.h
    include/mumlib2_private/crypto_state.h
    include/mumlib2_private/mumlib2_private.h
    include/mumlib2_private/transport.h
    include/mumlib2_private/transport_ssl_context.h
    include/mumlib2_private/varint.h
)

target_sources(mumlib2 PRIVATE 
    ${MUMLIB2_SOURCES}
    ${MUMLIB2_SOURCES_PROTO}
    ${MUMLIB2_HEADERS}
    ${MUMLIB2_HEADERS_PROTO}
)


if(MSVC)
    target_compile_options(mumlib2 PUBLIC /wd4244 /wd4251 /wd6001 /wd6031 /wd6255 /wd6258 /wd6385 /wd6386 /wd6387 /wd26439 /wd26451 /wd26495 /wd26812 /wd26819)
endif()


target_compile_definitions(mumlib2 PUBLIC _USE_MATH_DEFINES)
if(WIN32)
    target_compile_definitions(mumlib2 PUBLIC _WIN32_WINNT=0x0601)
    target_compile_definitions(mumlib2 PUBLIC _CRT_SECURE_NO_WARNINGS)
endif()


target_include_directories(mumlib2 
	PUBLIC
		"$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
        "$<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>"
		"$<INSTALL_INTERFACE:include>"
)

target_link_libraries(mumlib2 ${MUMLIB2_DEPS_VISIBLITY} asio::asio)
target_link_libraries(mumlib2 ${MUMLIB2_DEPS_VISIBLITY} OpenSSL::SSL)
target_link_libraries(mumlib2 ${MUMLIB2_DEPS_VISIBLITY} Opus::opus)
target_link_libraries(mumlib2 ${MUMLIB2_DEPS_VISIBLITY} protobuf::libprotobuf)
if(WIN32)
    target_link_libraries(mumlib2 ${MUMLIB2_DEPS_VISIBLITY} Crypt32)
endif()


set_target_properties(mumlib2 PROPERTIES CXX_STANDARD 20)
set_target_properties(mumlib2 PROPERTIES CXX_STANDARD_REQUIRED ON)

generate_export_header(mumlib2 EXPORT_FILE_NAME "mumlib2/export.h")


configure_package_config_file(
    cmake/mumlib2-config.cmake.in 
    mumlib2-config.cmake
	INSTALL_DESTINATION "${CMAKE_INSTALL_CMAKEDIR}"
)
write_basic_package_version_file(
    mumlib2-config-version.cmake
	VERSION ${SENTRY_VERSION}
	COMPATIBILITY SameMajorVersion
)


install(
    TARGETS mumlib2
    EXPORT mumlib2
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
)
install(
    FILES "include/mumlib2.h"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
install(
    DIRECTORY "include/mumlib2"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
install(
    FILES "${PROJECT_BINARY_DIR}/mumlib2/export.h"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/mumlib2"
)
install(
    EXPORT mumlib2 
    NAMESPACE mumlib2::
    FILE mumlib2-targets.cmake
    DESTINATION ${CMAKE_INSTALL_CMAKEDIR}
)
install(
    FILES 
        "${PROJECT_BINARY_DIR}/mumlib2-config.cmake"
        "${PROJECT_BINARY_DIR}/mumlib2-config-version.cmake"
    DESTINATION 
        "${CMAKE_INSTALL_CMAKEDIR}"
)

if(MSVC AND MUMLIB2_LIBRARY_TYPE STREQUAL "SHARED")
    install(
        FILES "$<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:$<TARGET_PDB_FILE:mumlib2>>"
        DESTINATION "bin"
    )
endif()



#
# Example
#

if(MUMLIB2_BUILD_EXAMPLE)
    add_executable(mumlib2_example)

    target_sources(mumlib2_example PRIVATE "src_example/mumlib2_example.cpp")

    target_link_libraries(mumlib2_example PRIVATE mumlib2)

    set_target_properties(mumlib2_example PROPERTIES CXX_STANDARD 20)
    set_target_properties(mumlib2_example PROPERTIES CXX_STANDARD_REQUIRED ON)
    

    install(
        TARGETS mumlib2_example
        RUNTIME DESTINATION "bin"
        ARCHIVE DESTINATION "lib"
        LIBRARY DESTINATION "lib"
    )

    if(MSVC)
        install(
            FILES "$<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:$<TARGET_PDB_FILE:mumlib2_example>>"
            DESTINATION "bin"
        )
    endif()
endif()
