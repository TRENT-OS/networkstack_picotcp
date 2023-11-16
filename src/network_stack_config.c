/*
 * Network Stack Config wrapper
 *
 * Copyright (C) 2019-2024, HENSOLDT Cyber GmbH
 * 
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "OS_Dataport.h"
#include "OS_Types.h"

#include "lib_debug/Debug.h"

#include "network_stack_config.h"
#include "network_stack_core.h"

#include <stddef.h>

//------------------------------------------------------------------------------
void
wait_network_event(void)
{
    Debug_LOG_TRACE("wait_network_event for handle");

    const NetworkStack_CamkesConfig_t* handlers = config_get_handlers();

    event_wait_func_t do_wait = handlers->wait_loop_event;
    if (!do_wait)
    {
        Debug_LOG_WARNING("wait_loop_event not set");
        return;
    }

    do_wait();
}


//------------------------------------------------------------------------------
void
internal_notify_main_loop(void)
{
    Debug_LOG_TRACE("internal_notify_main_loop for handle");

    const NetworkStack_CamkesConfig_t* handlers = config_get_handlers();

    event_notify_func_t do_notify = handlers->internal.notify_loop;
    if (!do_notify)
    {
        Debug_LOG_WARNING("internal.notify_main_loop not set");
        return;
    }

    do_notify();
}

//------------------------------------------------------------------------------
OS_Error_t
nic_dev_get_mac_address(
    uint8_t* buf)
{
    const NetworkStack_CamkesConfig_t* handlers = config_get_handlers();

    Debug_ASSERT( NULL != handlers->drv_nic.rpc.get_mac_address );

    uint64_t mac;
    OS_Error_t err = handlers->drv_nic.rpc.get_mac_address(&mac);
    if (err != OS_SUCCESS)
    {
        return err;
    }

    // Big endian integer 0x0000aabbccddeeff to MAC string aa:bb:cc:dd:ee:ff
    for (int i = 5; i >= 0; i--)
    {
        buf[i] = (uint8_t)mac;
        mac >>= 8;
    }
    return OS_SUCCESS;
}

//------------------------------------------------------------------------------
void
nic_dev_notify_send(void)
{
    Debug_LOG_TRACE("nic_dev_notify_send for handle");

    const NetworkStack_CamkesConfig_t* handlers = config_get_handlers();

    event_notify_func_t do_notify = handlers->drv_nic.notify_send;
    if (!do_notify)
    {
        Debug_LOG_WARNING("drv_nic.notify_send not set");
        return;
    }

    do_notify();
}

//------------------------------------------------------------------------------
void
internal_socket_control_block_mutex_lock(void)
{
    const NetworkStack_CamkesConfig_t* handlers = config_get_handlers();

    mutex_lock_func_t lock_mutex = handlers->internal.socketCB_lock;
    if (!lock_mutex)
    {
        Debug_LOG_WARNING("socket_control_block_mutex_lock not set");
        return;
    }

    Debug_LOG_TRACE("%s", __func__);
    lock_mutex();
}

//------------------------------------------------------------------------------
void internal_socket_control_block_mutex_unlock(void)
{
    const NetworkStack_CamkesConfig_t* handlers = config_get_handlers();

    mutex_unlock_func_t unlock_mutex = handlers->internal.socketCB_unlock;
    if (!unlock_mutex)
    {
        Debug_LOG_WARNING("socket_control_block_mutex_unlock not set");
        return;
    }

    Debug_LOG_TRACE("%s", __func__);
    unlock_mutex();
}

//------------------------------------------------------------------------------
void internal_network_stack_thread_safety_mutex_lock(void)
{
    const NetworkStack_CamkesConfig_t* handlers = config_get_handlers();

    mutex_lock_func_t lock_mutex = handlers->internal.stackTS_lock;
    if (!lock_mutex)
    {
        Debug_LOG_WARNING("socket_thread_safety_mutex_lock not set");
        return;
    }

    Debug_LOG_TRACE("%s", __func__);
    lock_mutex();
}

//------------------------------------------------------------------------------
void internal_network_stack_thread_safety_mutex_unlock(void)
{
    const NetworkStack_CamkesConfig_t* handlers = config_get_handlers();

    mutex_unlock_func_t unlock_mutex = handlers->internal.stackTS_unlock;
    if (!unlock_mutex)
    {
        Debug_LOG_WARNING("socket_thread_safety_mutex_unlock not set");
        return;
    }

    Debug_LOG_TRACE("%s", __func__);
    unlock_mutex();
}
