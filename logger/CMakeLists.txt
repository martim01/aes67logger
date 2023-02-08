cmake_minimum_required(VERSION 3.5 FATAL_ERROR)
project(logger LANGUAGES CXX C VERSION 1.0.0)

execute_process(COMMAND ${CMAKE_COMMAND} -DNAMESPACE=${PROJECT_NAME} -DMAJOR=${PROJECT_VERSION_MAJOR} -DMINOR=${PROJECT_VERSION_MINOR} -DPATCH=${PROJECT_VERSION_PATCH} -P ${CMAKE_SOURCE_DIR}/version.cmake)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
include_directories(${DIR_AOIP}/include)
include_directories("include")
include_directories("../common/include")
include_directories(${DIR_LOG}/include)
include_directories(${DIR_ASIO}/asio/include)
include_directories(${DIR_ASIO}/include)
include_directories(${DIR_JSON}/dist)

link_directories(${CMAKE_SOURCE_DIR}/lib)


add_executable(logger "../common/src/jsonconsts.cpp" "../common/src/inimanager.cpp" "../common/src/inisection.cpp" "src/jsonwriter.cpp" "${DIR_JSON}/dist/jsoncpp.cpp"
 "src/jsonlog.cpp"  "src/loggerapp.cpp" "src/main.cpp" "src/soundfile.cpp" "src/asiosession.cpp" ${CMAKE_BINARY_DIR}/src/${PROJECT_NAME}_version.cpp)

list(APPEND flags "-fPIC" "-Wall" "-fpermissive"  "-std=c++17")
if(CMAKE_BUILD_TYPE MATCHES Release)
    list(APPEND flags "-O3")
else()
list(APPEND flags "-g")
endif()
target_compile_options(logger PRIVATE ${flags})

list(APPEND definitions "NO_OPENSSL" "__GNU__")
target_compile_definitions(logger PUBLIC ${definitions})
target_link_libraries(logger optimized pml_log aoip debug pml_logd aoipd)
target_link_libraries(logger sndfile pthread)

set_target_properties(logger PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/")

#linux specific
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
install(TARGETS logger RUNTIME DESTINATION /usr/local/bin)
install(CODE "execute_process(COMMAND setcap cap_net_bind_service+ep /usr/local/bin/logger WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})")
endif()
