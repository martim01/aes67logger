cmake_minimum_required(VERSION 3.5 FATAL_ERROR)
project(flacencoder LANGUAGES CXX C VERSION 1.0.0)

execute_process(COMMAND ${CMAKE_COMMAND} -DNAMESPACE=${PROJECT_NAME} -DMAJOR=${PROJECT_VERSION_MAJOR} -DMINOR=${PROJECT_VERSION_MINOR} -DPATCH=${PROJECT_VERSION_PATCH} -P ${CMAKE_SOURCE_DIR}/version.cmake)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
include_directories(${DIR_AOIP}/include)
include_directories("include")
include_directories("../common/include")
include_directories(${DIR_LOG}/include)
include_directories(${DIR_ASIO}/asio/include)
include_directories(${DIR_ASIO}/include)
include_directories(${DIR_JSON}/dist)
include_directories(${DIR_OBSERVER}/include)

link_directories(${CMAKE_SOURCE_DIR}/lib)


add_executable(flacencoder "../common/src/jsonconsts.cpp" "../common/src/inimanager.cpp" "../common/src/inisection.cpp" "../common/src/jsonwriter.cpp" "${DIR_JSON}/dist/jsoncpp.cpp" "../common/src/asiosession.cpp" "../common/src/encoder.cpp" "src/flacencoder.cpp" "src/main.cpp" "../common/src/soundfile.cpp" ${CMAKE_BINARY_DIR}/src/${PROJECT_NAME}_version.cpp)

list(APPEND flags "-fPIC" "-Wall" "-fpermissive"  "-std=c++17")
if(CMAKE_BUILD_TYPE MATCHES Release)
    list(APPEND flags "-O3")
else()
list(APPEND flags "-g")
endif()
target_compile_options(flacencoder PRIVATE ${flags})

list(APPEND definitions "NO_OPENSSL" "__GNU__")
target_compile_definitions(flacencoder PUBLIC ${definitions})
target_link_libraries(flacencoder pml_log pml_observer)
target_link_libraries(flacencoder sndfile pthread)

set_target_properties(flacencoder PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/")

#linux specific
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
install(TARGETS flacencoder RUNTIME DESTINATION /usr/local/bin)
install(CODE "execute_process(COMMAND setcap cap_net_bind_service+ep /usr/local/bin/flacencoder WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})")
endif()