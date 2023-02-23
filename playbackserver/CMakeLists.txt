cmake_minimum_required(VERSION 3.5 FATAL_ERROR)
project(playbackserver LANGUAGES CXX C VERSION 1.0.0)

execute_process(COMMAND ${CMAKE_COMMAND} -DNAMESPACE=${PROJECT_NAME} -DMAJOR=${PROJECT_VERSION_MAJOR} -DMINOR=${PROJECT_VERSION_MINOR} -DPATCH=${PROJECT_VERSION_PATCH} -P ${CMAKE_SOURCE_DIR}/version.cmake)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

SET(DIR_OBSERVER ${DIR_BASE}/observer CACHE STRING "location of Observer")

#Get pbserver
find_path(OBSERVER_TEMP_DIR NAMES "CMakeLists.txt" PATHS ${DIR_OBSERVER})
if((NOT OBSERVER_TEMP_DIR) OR (NOT EXISTS ${OBSERVER_TEMP_DIR}))
	message(STATUS "pml_observer not found - clone from GitHub")
	execute_process(COMMAND git clone https://github.com/martim01/observer ${DIR_OBSERVER})
    
    find_path(OBSERVER_TEMP_DIR2 NAMES "CMakeLists.txt" PATHS ${DIR_OBSERVER})
    if((NOT OBSERVER_TEMP_DIR2) OR (NOT EXISTS ${OBSERVER_TEMP_DIR2}))
	    message(FATAL_ERROR "Failed to clone pml_observer")
    endif()
else()
    message(STATUS "pml_observer found - update to latest version")
    execute_process(COMMAND git -C ${DIR_OBSERVER} pull)
endif()
unset(OBSERVER_TEMP_DIR2)

message(STATUS "Configure pml::observer")
add_subdirectory(${DIR_OBSERVER} ${CMAKE_SOURCE_DIR}/build/observer)

include_directories("include")
include_directories("../common/include")
include_directories("../common/uuid")
include_directories(${DIR_LOG}/include)
include_directories(${DIR_JSON}/dist)
include_directories(${DIR_OBSERVER}/include)
include_directories(${DIR_RESTGOOSE}/include)
include_directories(${DIR_ASIO}/asio/include)
include_directories(${DIR_ASIO}/include)

link_directories("../lib")


add_executable(playbackserver "../common/src/jsonconsts.cpp" "../common/src/inimanager.cpp" "../common/src/aoiputils.cpp" "../common/src/inisection.cpp" "${DIR_JSON}/dist/jsoncpp.cpp" "../common/src/session.cpp" "src/main.cpp" "src/loggermanager.cpp" "src/loggerobserver.cpp" "src/playbackserver.cpp" "../common/uuid/guid.cpp" "../common/uuid/md5.cpp" ${CMAKE_BINARY_DIR}/src/${PROJECT_NAME}_version.cpp)

list(APPEND flags "-fPIC" "-Wall" "-fpermissive"  "-std=c++17")
if(CMAKE_BUILD_TYPE MATCHES Release)
    list(APPEND flags "-g")
else()
list(APPEND flags "-g")
endif()
target_compile_options(playbackserver PRIVATE ${flags})

list(APPEND definitions "NO_OPENSSL" "__GNU__")
target_compile_definitions(playbackserver PUBLIC ${definitions})
target_link_libraries(playbackserver pml_log restgoose pml_observer pthread)
#target_link_libraries(playbackserver optimized pml_log restgoose debug pml_logd restgoosed)

set_target_properties(playbackserver PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/")

#linux specific
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
install(TARGETS playbackserver RUNTIME DESTINATION /usr/local/bin)
install(CODE "execute_process(COMMAND setcap cap_net_bind_service+ep /usr/local/bin/playbackserver WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})")
endif()
