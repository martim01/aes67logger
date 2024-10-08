cmake_minimum_required(VERSION 3.5 FATAL_ERROR)
include(FetchContent)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
project(aes67logger LANGUAGES CXX C VERSION 1.1.0)

SET(DIR_BASE $ENV{HOME} CACHE STRING "base location for external libraries' source")
SET(DIR_AOIP ${DIR_BASE}/pml-aoip CACHE STRING "location of pml-aoip library")
SET(DIR_LOG ${DIR_BASE}/log CACHE STRING "location of pml log")
SET(DIR_JSON ${DIR_BASE}/jsoncpp CACHE STRING "location of jsoncpp")
SET(DIR_RESTGOOSE ${DIR_BASE}/Restgoose CACHE STRING "location of Restgoose")
SET(DIR_ASIO ${DIR_BASE}/asio CACHE STRING "location of Asio")
SET(RESTGOOSE_BRANCH "main" CACHE STRING "Restgoose branch to use")
SET(DIR_JWT ${DIR_BASE}/jwt-cpp CACHE STRING "location of jwt-cpp")
SET(DIR_BACKTRACE ${DIR_BASE}/libbacktrace CACHE STRING "location of libbacktrace")
SET(BUILD_LOG OFF)

execute_process(COMMAND ${CMAKE_COMMAND} -DNAMESPACE=${PROJECT_NAME} -DMAJOR=${PROJECT_VERSION_MAJOR} -DMINOR=${PROJECT_VERSION_MINOR} -DPATCH=${PROJECT_VERSION_PATCH} -P ${CMAKE_SOURCE_DIR}/version.cmake)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

#Get log
message(STATUS "Find pml::log")
find_path(TEMP_DIR NAMES "CMakeLists.txt" PATHS ${DIR_LOG} NO_CACHE)
if((NOT TEMP_DIR) OR (NOT EXISTS ${TEMP_DIR}))
	message(STATUS "log not found - clone from GitHub")
	execute_process(COMMAND git clone https://github.com/martim01/log/ ${DIR_LOG})

    find_path(TEMP_DIR2 NAMES "CMakeLists.txt" PATHS ${DIR_LOG} NO_CACHE)
    if((NOT TEMP_DIR2) OR (NOT EXISTS ${TEMP_DIR2}))
        message(FATAL_ERROR "Failed to clone pml::Log")
    endif()
else()
    message(STATUS "log found - update to latest version")
    execute_process(COMMAND git -C ${DIR_LOG} pull --no-rebase)
endif()


#Get JsonCpp
find_path(RG_TEMP_DIR NAMES "amalgamate.py" PATHS ${DIR_JSON} NO_CACHE)
if((NOT RG_TEMP_DIR) OR (NOT EXISTS ${RG_TEMP_DIR}))
	message("jsoncpp not found - clone from GitHub")
	execute_process(COMMAND git clone https://github.com/open-source-parsers/jsoncpp ${DIR_JSON})

	find_path(RG_TEMP_DIR2 NAMES "amalgamate.py" PATHS ${DIR_JSON} NO_CACHE)
	if((NOT RG_TEMP_DIR2) OR (NOT EXISTS ${RG_TEMP_DIR2}))
		message(FATAL_ERROR "Failed to clone jsoncpp")
	endif()
	unset(RG_TEMP_DIR2)
else()
	message(STATUS "jsoncpp found - update to latest version")
    execute_process(COMMAND git -C ${DIR_JSON} pull --no-rebase)
endif()
unset(RG_TEMP_DIR)


#Amalgamate JsonCpp
find_path(RG_TEMP_DIR3 NAMES "jsoncpp.cpp" PATHS ${DIR_JSON}/dist NO_CACHE)
if((NOT RG_TEMP_DIR3) OR (NOT EXISTS ${RG_TEMP_DIR3}))
	message("amalgamate jsoncpp")
	execute_process(COMMAND python3 amalgamate.py WORKING_DIRECTORY ${DIR_JSON})
endif()
unset(RG_TEMP_DIR3)

find_path(RG_TEMP_DIR4 NAMES "jsoncpp.cpp" PATHS ${DIR_JSON}/dist NO_CACHE)
if((NOT RG_TEMP_DIR4) OR (NOT EXISTS ${RG_TEMP_DIR4}))
	message(FATAL_ERROR "Failed to amalgamate jsoncpp")
endif()
unset(RG_TEMP_DIR4)

#Get asio
message(STATUS "Find asio")
find_path(TEMP_DIRA NAMES "asio.hpp" PATHS ${DIR_ASIO}/asio/include NO_CACHE)
if((NOT TEMP_DIRA) OR (NOT EXISTS ${TEMP_DIRA}))
	message(STATUS "asio not found - clone from GitHub")
	execute_process(COMMAND git clone https://github.com/chriskohlhoff/asio/ ${DIR_ASIO})

    find_path(TEMP_DIRA2 NAMES "asio.hpp" PATHS ${DIR_ASIO}/asio/include NO_CACHE)
    if((NOT TEMP_DIRA2) OR (NOT EXISTS ${TEMP_DIRA2}))
        message(FATAL_ERROR "Failed to clone asio")
    endif()
else()
    message(STATUS "asio found - update to latest version")
    execute_process(COMMAND git -C ${DIR_ASIO} pull --no-rebase)
endif()

#Get Restgoose
find_path(EPI_TEMP_DIR NAMES "CMakeLists.txt" PATHS ${DIR_RESTGOOSE})
if((NOT EPI_TEMP_DIR) OR (NOT EXISTS ${EPI_TEMP_DIR}))
	message(STATUS "Restgoose not found - clone from GitHub")
	execute_process(COMMAND git clone https://github.com/martim01/Restgoose ${DIR_RESTGOOSE})
    execute_process(COMMAND git -C ${DIR_RESTGOOSE} checkout ${RESTGOOSE_BRANCH})
    
    find_path(EPI_TEMP_DIR2 NAMES "CMakeLists.txt" PATHS ${DIR_RESTGOOSE})
    if((NOT EPI_TEMP_DIR2) OR (NOT EXISTS ${EPI_TEMP_DIR2}))
	    message(FATAL_ERROR "Failed to clone Restgoose")
    endif()
else()
    message(STATUS "Restgoose found - update to latest version")
    execute_process(COMMAND git -C ${DIR_RESTGOOSE} checkout ${RESTGOOSE_BRANCH})
    execute_process(COMMAND git -C ${DIR_RESTGOOSE} pull)
endif()
unset(EPI_TEMP_DIR2)

#Get aoip
find_path(AOIP_TEMP_DIR NAMES "CMakeLists.txt" PATHS ${DIR_AOIP})
if((NOT AOIP_TEMP_DIR) OR (NOT EXISTS ${AOIP_TEMP_DIR}))
	message(STATUS "Aoip not found - clone from GitHub")
	execute_process(COMMAND git clone https://github.com/martim01/pml-aoip ${DIR_AOIP})
    
    find_path(AOIP_TEMP_DIR2 NAMES "CMakeLists.txt" PATHS ${DIR_AOIP})
    if((NOT AOIP_TEMP_DIR2) OR (NOT EXISTS ${AOIP_TEMP_DIR2}))
	    message(FATAL_ERROR "Failed to clone aoip")
    endif()
else()
    message(STATUS "aoip found - update to latest version")
    execute_process(COMMAND git -C ${DIR_AOIP} pull)
endif()
unset(AOIP_TEMP_DIR2)


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

#Get jwt-cpp
message(STATUS "Find jwt-cpp")
find_path(TEMP_JWT NAMES "CMakeLists.txt" PATHS ${DIR_JWT})
if((NOT TEMP_JWT) OR (NOT EXISTS ${TEMP_JWT}))
	message(STATUS "jwt-cpp not found - clone from GitHub")
	execute_process(COMMAND git clone https://github.com/Thalhammer/jwt-cpp ${DIR_JWT})

    find_path(TEMP_JWT2 NAMES "CMakeLists.txt" PATHS ${DIR_JWT})
    if((NOT TEMP_JWT2) OR (NOT EXISTS ${TEMP_JWT}))
        message(FATAL_ERROR "Failed to clone jwt-cpp")
    endif()
else()
    message(STATUS "jwt-cpp found - update to latest version")
    execute_process(COMMAND git -C ${DIR_JWT} pull)
endif()



#Get libbacktrace
find_path(BACKTRACE_TEMP_DIR NAMES "backtrace.h" PATHS ${DIR_BACKTRACE})
if((NOT BACKTRACE_TEMP_DIR) OR (NOT EXISTS ${BACKTRACE_TEMP_DIR}))
	message(STATUS "libbacktrace not found - clone from GitHub")
	execute_process(COMMAND git clone https://github.com/ianlancetaylor/libbacktrace.git ${DIR_BACKTRACE})
    
    find_path(BACKTRACE_TEMP_DIR2 NAMES "backtrace.h" PATHS ${DIR_BACKTRACE})
    if((NOT BACKTRACE_TEMP_DIR2) OR (NOT EXISTS ${BACKTRACE_TEMP_DIR2}))
	    message(FATAL_ERROR "Failed to clone libbacktrace")
    endif()
endif()
message(STATUS "libbacktrace found - update to latest version")
execute_process(COMMAND git -C ${DIR_BACKTRACE} pull)
message(STATUS "libbacktrace - build")
execute_process(COMMAND ${DIR_BACKTRACE}/configure)
execute_process(COMMAND make -f ${DIR_BACKTRACE}/Makefile)

unset(BACKTRACE_TEMP_DIR2)


message(STATUS "Configure pml::log")
add_subdirectory(${DIR_LOG} ${CMAKE_SOURCE_DIR}/build/log)
message(STATUS "Configure Restgoose")
add_subdirectory(${DIR_RESTGOOSE} ${DIR_RESTGOOSE}/build)
message(STATUS "Configure aoip")
add_subdirectory(${DIR_AOIP} ${DIR_AOIP}/build)






#add_subdirectory(logger)
#add_subdirectory(loggingserver)

add_subdirectory(encodingserver)
add_subdirectory(opusencoder)
add_subdirectory(flacencoder)

add_subdirectory(playbackserver)

add_subdirectory(webserver)

