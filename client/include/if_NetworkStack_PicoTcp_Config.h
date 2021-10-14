/*
 * Network Stack Configuration Interface
 *
 * Copyright (C) 2021, HENSOLDT Cyber GmbH
 */

#pragma once

#include "OS_Error.h"
#include "OS_Network.h"

typedef struct
{
    OS_Error_t (*configIpAddr)(
        const OS_NetworkStack_AddressConfig_t* config);
}
if_NetworkStack_PicoTcp_Config_t;

/**
 * Assigns the correct RPC function pointers to a struct supposed to hold them.
 */
#define if_NetworkStack_PicoTcp_Config_ASSIGN(_prefix_)                        \
{                                                                              \
    .configIpAddr = _prefix_##_rpc_configIpAddr                                \
}
