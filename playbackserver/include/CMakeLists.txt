cmake_minimum_required(VERSION 3.5 FATAL_ERROR)
project(playbackserver LANGUAGES CXX C VERSION 1.0.0)

execute_process(COMMAND ${CMAKE_COMMAND} -DNAMESPACE=${PROJECT_NAME} -DMAJOR=${PROJECT_VERSION_MAJOR} -DMINOR=${PROJECT_VERSION_MINOR} -DPATCH=${PROJECT_VERSION_PATCH} -P ${CMAKE_SOURCE_DIR}/version.cmake)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)


include_directories("include")
include_directories("../common/include")
include_directories("../common/uuid")
include_directories(${DIR_LOG}/include)
include_directories(${DIR_JSON}/dist)
include_directories(${DIR_OBSERVER}/include)
include_directories(${DIR_RESTGOOSE}/include)
include_directories(${DIR_ASIO}/asio/include)
include_directories(${DIR_ASIO}/include)
include_directories(${DIR_JWT}/include)

link_directories("../lib")


add_executable(playbackserver "../common/src/jsonconsts.cpp" "../common/src/inimanager.cpp" "../common/src/aes67utils.cpp" "../common/src/inisection.cpp" "${DIR_JSON}/dist/jsoncpp.cpp" "../common/src/server.cpp" "../common/src/ntpstatus.cpp" "../common/src/proccheck.cpp" "../common/src/sysinfomanager.cpp" "../common/src/session.cpp" "src/main.cpp" "src/loggermanager.cpp" "src/loggerobserver.cpp" "src/playbackserver.cpp" "../common/uuid/guid.cpp" "../common/uuid/md5.cpp" ${CMAKE_BINARY_DIR}/src/${PROJECT_NAME}_version.cpp)

list(APPEND flags "-fPIC" "-Wall" "-fpermissive"  "-std=c++17")
if(CMAKE_BUILD_TYPE MATCHES Release)
    list(APPEND flags "-g")
else()
list(APPEND flags "-g")
endif()
target_compile_options(playbackserver PRIVATE ${flags})

list(APPEND definitions "NO_OPENSSL" "__GNU__")
target_compile_definitions(playbackserver PUBLIC ${definitions})
target_link_libraries(playbackserver pml_log restgoose pml_observer pthread crypto ssl)
#target_link_libraries(playbackserver optimized pml_log restgoose debug pml_logd restgoosed)

set_target_properties(playbackserver PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/")

#linux specific
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
install(TARGETS playbackserver RUNTIME DESTINATION /usr/local/bin)
install(CODE "execute_process(COMMAND setcap cap_net_bind_service+ep /usr/local/bin/playbackserver WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})")
endif()
