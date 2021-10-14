/*
 * Network Stack Config wrapper
 *
 * Copyright (C) 2019-2021, HENSOLDT Cyber GmbH
 */

#pragma once

#include "OS_Dataport.h"
#include "OS_Types.h"

#include "network/OS_Network_types.h"

#include <stddef.h>

typedef OS_Error_t (*nic_initialize_func_t)(
    const OS_NetworkStack_AddressConfig_t* config);
typedef OS_Error_t (*stack_initialize_func_t)(void);
typedef void (*stack_tick_func_t)(void);

typedef struct
{
    // The following variables are written from one thread (control thread) and
    // read from another (RPC thread) therefore volatile is needed to tell the
    // compiler that variable content can change outside of its control.
    volatile bool needsToBeNotified;
    volatile int currentSocketsInUse;

    int clientId;
    bool inUse;
    int socketQuota;

    // Use head and tail per client to circulate through the pending events
    // whenever _getPendingEvents() is called.
    int head;
    int tail;

    event_notify_func_t eventNotify;
} NetworkStack_Client_t;

typedef struct
{
    // The following variables are written from one thread (control thread) and
    // read from another (RPC thread) therefore volatile is needed to tell the
    // compiler that variable content can change outside of its control.
    volatile int status;
    volatile int parentHandle;
    volatile uint16_t eventMask;
    volatile OS_Error_t current_error;
    volatile int pendingConnections;

    int clientId;

    void* buf_io;
    OS_Dataport_t buf;

    void* implementation_socket;
} NetworkStack_SocketResources_t;

typedef struct
{
    event_wait_func_t wait_loop_event;

    struct
    {
        event_notify_func_t notify_loop; // -> wait_event

        NetworkStack_SocketResources_t* sockets;

        NetworkStack_Client_t* clients;

        int number_of_sockets;
        int number_of_clients;
        int* client_sockets_quota;

        mutex_lock_func_t allocator_lock;
        mutex_unlock_func_t allocator_unlock;

        mutex_lock_func_t nwStack_lock;
        mutex_unlock_func_t nwStack_unlock;

        mutex_lock_func_t socketCB_lock;
        mutex_unlock_func_t socketCB_unlock;

        mutex_lock_func_t stackTS_lock;
        mutex_unlock_func_t stackTS_unlock;
    } internal;

    struct
    {
        OS_Dataport_t from; // NIC -> stack
        OS_Dataport_t to;   // stack -> NIC
        struct
        {
            OS_Error_t (*dev_read)(size_t* len, size_t* frames_available);
            OS_Error_t (*dev_write)(size_t* len);
            OS_Error_t (*get_mac)(void);
            // API extension: OS_Error_t (*get_link_state)(void);
        } rpc;
    } drv_nic;

} NetworkStack_CamkesConfig_t;

typedef struct
{
    nic_initialize_func_t nic_init;
    stack_initialize_func_t stack_init;
    stack_tick_func_t stack_tick;
} NetworkStack_Interface_t;

typedef struct
{
    const NetworkStack_CamkesConfig_t* camkes_cfg;
    const OS_NetworkStack_AddressConfig_t* cfg;
    NetworkStack_SocketResources_t* sockets;
    NetworkStack_Client_t* clients;

    int number_of_sockets;
    int number_of_clients;
} NetworkStack_t;

const NetworkStack_CamkesConfig_t* config_get_handlers(void);

//------------------------------------------------------------------------------
// System interface
//------------------------------------------------------------------------------

void wait_network_event(void);

void internal_notify_main_loop(void);

const OS_Dataport_t* get_nic_port_from(void);
const OS_Dataport_t* get_nic_port_to(void);

OS_Error_t
nic_dev_read(
    size_t* pLen,
    size_t* frameRemaining);

OS_Error_t
nic_dev_write(
    size_t* pLen);

OS_Error_t
nic_dev_get_mac_address(void);

void internal_socket_control_block_mutex_lock(void);
void internal_socket_control_block_mutex_unlock(void);

void internal_network_stack_thread_safety_mutex_lock(void);
void internal_network_stack_thread_safety_mutex_unlock(void);
