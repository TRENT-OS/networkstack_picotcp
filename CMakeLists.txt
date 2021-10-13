#
# Network Stack PicoTcp component
#
# Copyright (C) 2021, HENSOLDT Cyber GmbH
#

cmake_minimum_required(VERSION 3.18)

project(networkStack_PicoTcp C)

add_library(${PROJECT_NAME} INTERFACE)

target_include_directories(${PROJECT_NAME}
    INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/client/include
)


# Include the picoTCP project, but do not build any targets from it unless they
# are explicitly included.
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/3rdParty/picotcp EXCLUDE_FROM_ALL)


#-------------------------------------------------------------------------------
#
# Declare Network Stack PicoTcp CAmkES Component
#
# Parameters:
#
#   <name>
#     component instance name. The server will be called "<name>" and the
#     corresponding client library will be called "<name>_client"
#
function(NetworkStack_PicoTcp_DeclareCAmkESComponent
    name
)

    # Let caller append to any of the build options with his own variables
    cmake_parse_arguments(
        PARSE_ARGV
        1
        NETWORKSTACK_EXTRA
        ""
        ""
        "SOURCES;C_FLAGS;INCLUDES;LIBS"
    )

    #---------------------------------------------------------------------------
    DeclareCAmkESComponent(${name}
        SOURCES
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/src/NetworkStack_PicoTcp.c
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/src/network_stack_core.c
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/src/network_config.c
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/src/network_stack_pico.c
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/src/network_stack_pico_nic.c
            ${NETWORKSTACK_EXTRA_SOURCES}
        C_FLAGS
            -Wall
            -Werror
            ${NETWORKSTACK_EXTRA_C_FLAGS}
        INCLUDES
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/include/
            ${NETWORKSTACK_EXTRA_INCLUDES}
        LIBS
            picotcp
            os_core_api
            lib_debug
            lib_macros
            lib_server
            TimeServer_client
            system_config
            ${NETWORKSTACK_EXTRA_LIBS}
    )

    DeclareCAmkESComponent(
        Ticker
        SOURCES
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/src/NetworkStack_Ticker.c
        C_FLAGS
            -Wall
            -Werror
        LIBS
            os_core_api
            lib_debug
    )
endfunction()
