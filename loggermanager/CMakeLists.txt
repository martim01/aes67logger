cmake_minimum_required(VERSION 3.5 FATAL_ERROR)
project(loggermanager LANGUAGES CXX C VERSION 1.0.0)

execute_process(COMMAND ${CMAKE_COMMAND} -DNAMESPACE=${PROJECT_NAME} -DMAJOR=${PROJECT_VERSION_MAJOR} -DMINOR=${PROJECT_VERSION_MINOR} -DPATCH=${PROJECT_VERSION_PATCH} -P ${CMAKE_SOURCE_DIR}/version.cmake)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)



include_directories("include")
include_directories("../common/include")
include_directories("uuid")
include_directories(${DIR_LOG}/include)
include_directories(${DIR_JSON}/dist)
include_directories(${DIR_RESTGOOSE}/include)
include_directories(${DIR_ASIO}/asio/include)

link_directories("../lib")



add_executable(loggermanager "../common/src/jsonconsts.cpp" "../common/src/inimanager.cpp" "../common/src/aoiputils.cpp" "../common/src/inisection.cpp" "${DIR_JSON}/dist/jsoncpp.cpp"
 "src/launcher.cpp"  "src/session.cpp" "src/launchmanager.cpp" "src/main.cpp" "src/ntpstatus.cpp" "src/proccheck.cpp" "src/server.cpp" "src/sysinfomanager.cpp"
 "uuid/guid.cpp" "uuid/md5.cpp" ${CMAKE_BINARY_DIR}/src/${PROJECT_NAME}_version.cpp)

list(APPEND flags "-fPIC" "-Wall" "-fpermissive"  "-std=c++17")
if(CMAKE_BUILD_TYPE MATCHES Release)
    list(APPEND flags "-g")
else()
list(APPEND flags "-g")
endif()
target_compile_options(loggermanager PRIVATE ${flags})

list(APPEND definitions "NO_OPENSSL" "__GNU__")
target_compile_definitions(loggermanager PUBLIC ${definitions})
target_link_libraries(loggermanager pml_log restgoose pthread)
#target_link_libraries(loggermanager optimized pml_log restgoose debug pml_logd restgoosed)

set_target_properties(loggermanager PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/")

#linux specific
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
install(TARGETS loggermanager RUNTIME DESTINATION /usr/local/bin)
install(CODE "execute_process(COMMAND setcap cap_net_bind_service+ep /usr/local/bin/loggermanager WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})")
endif()
