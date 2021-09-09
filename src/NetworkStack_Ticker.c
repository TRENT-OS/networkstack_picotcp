/*
 *  Network Stack Internal Ticker
 *
 *  Copyright (C) 2019-2021, HENSOLDT Cyber GmbH
 *
 */

#include "lib_debug/Debug.h"
#include "OS_Error.h"
#include <camkes.h>


//------------------------------------------------------------------------------
int run(void)
{
    Debug_LOG_INFO("ticker running");

    // set up a tick every second
    int ret = timeServer_rpc_periodic(0, NS_IN_S);
    if (0 != ret)
    {
        Debug_LOG_ERROR("timeServer_rpc_periodic() failed, code %d", ret);
        return -1;
    }

    for(;;)
    {
        timeServer_notify_wait();

        event_tick_emit();
    }
}

OS_Error_t
proxy_timeServer_rpc_time(uint64_t* ns)
{
    return timeServer_rpc_time(ns);
}

OS_Error_t
proxy_timeServer_rpc_oneshot_relative(int id, uint64_t ns)
{
    return OS_ERROR_NOT_IMPLEMENTED;
}

OS_Error_t
proxy_timeServer_rpc_oneshot_absolute(int id, uint64_t ns)
{
    return OS_ERROR_NOT_IMPLEMENTED;
}

OS_Error_t
proxy_timeServer_rpc_periodic(int id, uint64_t ns)
{
    return OS_ERROR_NOT_IMPLEMENTED;
}

OS_Error_t
proxy_timeServer_rpc_stop(int id)
{
    return OS_ERROR_NOT_IMPLEMENTED;
}

OS_Error_t
proxy_timeServer_rpc_completed(uint32_t* tmr)
{
    return OS_ERROR_NOT_IMPLEMENTED;
}
