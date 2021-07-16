#
# Network Stack PicoTcp component
#
# Copyright (C) 2021, HENSOLDT Cyber GmbH
#

cmake_minimum_required(VERSION 3.18)


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

    #---------------------------------------------------------------------------
    DeclareCAmkESComponent(${name}
        SOURCES
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/NwStack/network_stack.c
        C_FLAGS
            -Wall
            -Werror
            ${NETWORKSTACK_EXTRA_C_FLAGS}
        INCLUDES
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/
        LIBS
            os_network_lib
            os_core_api
            lib_debug
            lib_macros
            lib_server
            TimeServer_client
            syslogger_client
            system_config
    )

endfunction()
