/*
 * Network Stack Core functions
 *
 * Copyright (C) 2019-2024, HENSOLDT Cyber GmbH
 * 
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "network/OS_NetworkStackTypes.h"
#include "network/OS_SocketTypes.h"

#include "lib_macros/Check.h"

#include "network_stack_config.h"
#include "network_stack_core.h"
#include "network_stack_pico.h"

#include <stdlib.h>
#include <stdint.h>

#define SOCKET_FREE   0
#define SOCKET_IN_USE 1
#define CONNECTED 1

// TODO: The implementation for this function is provided by the implementing
// NetworkStack component. This should be reworked so that we do not have this
// close coupling here and move the whole state management to the implementing
// component.
extern OS_NetworkStack_State_t networkStack_getState();

// network stack state
static NetworkStack_t instance = {0};

//------------------------------------------------------------------------------
const NetworkStack_CamkesConfig_t*
config_get_handlers(void)
{
    const NetworkStack_CamkesConfig_t* handlers = instance.camkes_cfg;

    Debug_ASSERT( NULL != handlers );

    return handlers;
}


//------------------------------------------------------------------------------
OS_Error_t
networkStack_rpc_socket_create(
    const int  domain,
    const int  socket_type,
    int* const pHandle)
{
    CHECK_IS_RUNNING(networkStack_getState());

    CHECK_PTR_NOT_NULL(pHandle);

    return network_stack_pico_socket_create(
               domain,
               socket_type,
               pHandle,
               get_client_id(),
               get_client_id_buf(),
               get_client_id_buf_size());
}


//------------------------------------------------------------------------------
OS_Error_t
networkStack_rpc_socket_close(
    const int handle)
{
    CHECK_IS_RUNNING(networkStack_getState());

    NetworkStack_SocketResources_t* socket = get_socket_from_handle(handle);

    CHECK_SOCKET(socket, handle);

    CHECK_CLIENT_ID(socket);

    return network_stack_pico_socket_close(handle, get_client_id());
}


//------------------------------------------------------------------------------
OS_Error_t
networkStack_rpc_socket_connect(
    const int                     handle,
    const OS_Socket_Addr_t* const dstAddr)
{
    CHECK_IS_RUNNING(networkStack_getState());

    NetworkStack_SocketResources_t* socket = get_socket_from_handle(handle);

    CHECK_SOCKET(socket, handle);

    CHECK_SOCKET_TYPE(socket, OS_SOCK_STREAM);

    CHECK_CLIENT_ID(socket);

    CHECK_PTR_NOT_NULL(dstAddr);

    CHECK_STR_IS_NUL_TERMINATED(dstAddr->addr, 16);

    return network_stack_pico_socket_connect(handle, dstAddr);
}


//------------------------------------------------------------------------------
OS_Error_t
networkStack_rpc_socket_bind(
    const int                     handle,
    const OS_Socket_Addr_t* const localAddr)
{
    CHECK_IS_RUNNING(networkStack_getState());

    NetworkStack_SocketResources_t* socket = get_socket_from_handle(handle);

    CHECK_SOCKET(socket, handle);

    CHECK_CLIENT_ID(socket);

    CHECK_PTR_NOT_NULL(localAddr);

    CHECK_STR_IS_NUL_TERMINATED(localAddr->addr, 16);

    return network_stack_pico_socket_bind(handle, localAddr);
}


//------------------------------------------------------------------------------
OS_Error_t
networkStack_rpc_socket_listen(
    const int handle,
    const int backlog)
{
    CHECK_IS_RUNNING(networkStack_getState());

    NetworkStack_SocketResources_t* socket = get_socket_from_handle(handle);

    CHECK_SOCKET(socket, handle);

    CHECK_SOCKET_TYPE(socket, OS_SOCK_STREAM);

    CHECK_CLIENT_ID(socket);

    return network_stack_pico_socket_listen(handle, backlog);
}


//------------------------------------------------------------------------------
// For server wait on accept until client connects. Not much useful for client
// as we cannot accept incoming connections
OS_Error_t
networkStack_rpc_socket_accept(
    const int               handle,
    int* const              pClient_handle,
    OS_Socket_Addr_t* const srcAddr)
{
    CHECK_IS_RUNNING(networkStack_getState());

    NetworkStack_SocketResources_t* socket = get_socket_from_handle(handle);

    CHECK_SOCKET(socket, handle);

    CHECK_SOCKET_TYPE(socket, OS_SOCK_STREAM);

    CHECK_CLIENT_ID(socket);

    CHECK_PTR_NOT_NULL(pClient_handle);

    CHECK_PTR_NOT_NULL(srcAddr);

    return network_stack_pico_socket_accept(handle, pClient_handle, srcAddr);
}


//------------------------------------------------------------------------------
OS_Error_t
networkStack_rpc_socket_write(
    const int     handle,
    size_t* const pLen)
{
    CHECK_IS_RUNNING(networkStack_getState());

    NetworkStack_SocketResources_t* socket = get_socket_from_handle(handle);

    CHECK_SOCKET(socket, handle);

    CHECK_SOCKET_TYPE(socket, OS_SOCK_STREAM);

    CHECK_SOCKET_CONNECTED(socket, handle);

    CHECK_CLIENT_ID(socket);

    CHECK_PTR_NOT_NULL(pLen);

    // If the requested length exceeds the dataport size, reduce it to the size
    // of the dataport.
    if (*pLen > get_client_id_buf_size())
    {
        *pLen = get_client_id_buf_size();
    }

    return network_stack_pico_socket_write(handle, pLen);
}

//------------------------------------------------------------------------------
OS_Error_t
networkStack_rpc_socket_read(
    const int     handle,
    size_t* const pLen)
{
    CHECK_IS_RUNNING(networkStack_getState());

    NetworkStack_SocketResources_t* socket = get_socket_from_handle(handle);

    CHECK_SOCKET(socket, handle);

    CHECK_SOCKET_TYPE(socket, OS_SOCK_STREAM);

    CHECK_SOCKET_CONNECTED(socket, handle);

    CHECK_CLIENT_ID(socket);

    CHECK_PTR_NOT_NULL(pLen);

    // If the requested length exceeds the dataport size, reduce it to the size
    // of the dataport.
    if (*pLen > get_client_id_buf_size())
    {
        *pLen = get_client_id_buf_size();
    }

    return network_stack_pico_socket_read(handle, pLen);
}

//------------------------------------------------------------------------------
OS_Error_t
networkStack_rpc_socket_sendto(
    const int                     handle,
    size_t* const                 pLen,
    const OS_Socket_Addr_t* const dstAddr)
{
    CHECK_IS_RUNNING(networkStack_getState());

    NetworkStack_SocketResources_t* socket = get_socket_from_handle(handle);

    CHECK_SOCKET(socket, handle);

    CHECK_SOCKET_TYPE(socket, OS_SOCK_DGRAM);

    CHECK_CLIENT_ID(socket);

    CHECK_PTR_NOT_NULL(pLen);

    CHECK_PTR_NOT_NULL(dstAddr);

    // If the requested length exceeds the dataport size, reduce it to the size
    // of the dataport.
    if (*pLen > get_client_id_buf_size())
    {
        *pLen = get_client_id_buf_size();
    }

    return network_stack_pico_socket_sendto(handle, pLen, dstAddr);
}

//------------------------------------------------------------------------------
OS_Error_t
networkStack_rpc_socket_recvfrom(
    const int               handle,
    size_t* const           pLen,
    OS_Socket_Addr_t* const srcAddr)
{
    CHECK_IS_RUNNING(networkStack_getState());

    NetworkStack_SocketResources_t* socket = get_socket_from_handle(handle);

    CHECK_SOCKET(socket, handle);

    CHECK_SOCKET_TYPE(socket, OS_SOCK_DGRAM);

    CHECK_CLIENT_ID(socket);

    CHECK_PTR_NOT_NULL(pLen);

    CHECK_PTR_NOT_NULL(srcAddr);

    // If the requested length exceeds the dataport size, reduce it to the size
    // of the dataport.
    if (*pLen > get_client_id_buf_size())
    {
        *pLen = get_client_id_buf_size();
    }

    return network_stack_pico_socket_recvfrom(handle, pLen, srcAddr);
}

//------------------------------------------------------------------------------
OS_NetworkStack_State_t
networkStack_rpc_socket_getStatus(
    void)
{
    return networkStack_getState();
}

//------------------------------------------------------------------------------
OS_Error_t
networkStack_rpc_socket_getPendingEvents(
    const size_t  maxRequestedSize,
    size_t* const pNumberOfEvents)
{
    CHECK_IS_RUNNING(networkStack_getState());

    CHECK_PTR_NOT_NULL(pNumberOfEvents);

    if (maxRequestedSize < sizeof(OS_Socket_Evt_t))
    {
        Debug_LOG_ERROR("Received invalid buffer size");
        return OS_ERROR_BUFFER_TOO_SMALL;
    }

    const int clientId = get_client_id();
    const int clientIndex = get_client_index_from_clientId(clientId);
    if (clientIndex < 0)
    {
        Debug_LOG_ERROR("Failed to get client index from clientId %d", clientId);
        return OS_ERROR_ABORTED;
    }

    uint8_t* const clientDataport = get_client_id_buf();
    const size_t clientDataportSize = get_client_id_buf_size();

    int maxSocketsWithEvents;

    if (maxRequestedSize <= clientDataportSize)
    {
        maxSocketsWithEvents = ((maxRequestedSize) / sizeof(OS_Socket_Evt_t));
    }
    else
    {
        maxSocketsWithEvents = ((clientDataportSize) / sizeof(OS_Socket_Evt_t));
    }

    int offset = 0;
    int socketsWithEvents = 0;

    do
    {
        int i = instance.clients[clientIndex].head;

        if (instance.sockets[i].clientId == clientId)
        {
            if (instance.sockets[i].eventMask)
            {
                socketsWithEvents++;
                OS_Socket_Evt_t event;

                internal_network_stack_thread_safety_mutex_lock();
                event.eventMask = instance.sockets[i].eventMask;
                event.socketHandle = i;
                event.parentSocketHandle = instance.sockets[i].parentHandle;
                event.currentError = instance.sockets[i].current_error;
                // Unmask all events that require no follow up communication
                // with the NetworkStack and should only inform the client about
                // specific events.
                instance.sockets[i].eventMask &= ~(OS_SOCK_EV_CONN_EST
                                                   | OS_SOCK_EV_WRITE
                                                   | OS_SOCK_EV_ERROR);
                internal_network_stack_thread_safety_mutex_unlock();

                memcpy(&clientDataport[offset], &event, sizeof(event));
                offset += sizeof(event);
            }
        }

        instance.clients[clientIndex].head++;

        if (instance.clients[clientIndex].head == instance.number_of_sockets)
        {
            instance.clients[clientIndex].head = 0;
        }
    }
    while ((instance.clients[clientIndex].head !=
            instance.clients[clientIndex].tail)
           && (socketsWithEvents < maxSocketsWithEvents));

    // The loop was exited due to the fact that it reached the maximum number of
    // events that were requested by the caller. Signal the caller with the next
    // tick, that events might still be left.
    if ((socketsWithEvents >= maxSocketsWithEvents)
        && (instance.clients[clientIndex].head != instance.clients[clientIndex].tail))
    {
        instance.clients[clientIndex].needsToBeNotified = true;
    }

    instance.clients[clientIndex].tail = instance.clients[clientIndex].head;

    *pNumberOfEvents = socketsWithEvents;

    return OS_SUCCESS;
}

//------------------------------------------------------------------------------
// get implementation socket from a given handle
void*
get_implementation_socket_from_handle(
    const int handle)
{
    if (handle < 0 || handle >= instance.number_of_sockets)
    {
        Debug_LOG_ERROR("Trying to use invalid handle");
        return NULL;
    }
    return instance.sockets[handle].implementation_socket;
}

//------------------------------------------------------------------------------
// get socket from a given handle
void*
get_socket_from_handle(
    const int handle)
{
    if (handle < 0 || handle >= instance.number_of_sockets)
    {
        Debug_LOG_ERROR("Trying to use invalid handle");
        return NULL;
    }
    return &instance.sockets[handle];
}

//------------------------------------------------------------------------------
// get handle from a given socket
int
get_handle_from_implementation_socket(
    void* impl_sock)
{
    int handle = -1;
    for (int i = 0; i < instance.number_of_sockets; i++)
        if (instance.sockets[i].implementation_socket == impl_sock)
        {
            handle = i;
            break;
        }
    return handle;
}

//------------------------------------------------------------------------------
// get client index from a given clientId
int
get_client_index_from_clientId(
    const int clientId)
{
    if (clientId < 0)
    {
        Debug_LOG_ERROR("Invalid clientId %d", clientId);
        return -1;
    }

    for (int i = 0; i < instance.number_of_clients; i++)
    {
        if (clientId == instance.clients[i].clientId && instance.clients[i].inUse)
        {
            Debug_LOG_TRACE("Found client index %d for clientId %d", i, clientId);
            return i;
        }
    }

    Debug_LOG_ERROR("Could not find any client index for clientId %d", clientId);

    return -1;
}

//------------------------------------------------------------------------------
// get client from a given clientId
void*
get_client_from_clientId(
    const int clientId)
{
    const int clientIndex = get_client_index_from_clientId(clientId);
    if (clientIndex < 0)
    {
        Debug_LOG_ERROR("Failed to get client index from clientId %d", clientId);
        return NULL;
    }

    if (!instance.clients[clientIndex].inUse)
    {
        Debug_LOG_ERROR("Unused client %d", clientIndex);
        return NULL;
    }

    return &instance.clients[clientIndex];
}

//------------------------------------------------------------------------------
// Reserve a free handle
int
reserve_handle(
    void* impl_sock,
    int clientId)
{
    const int clientIndex = get_client_index_from_clientId(clientId);
    if (clientIndex < 0)
    {
        Debug_LOG_ERROR("Failed to get client index from clientId %d", clientId);
        return -1;
    }

    if (!instance.clients[clientIndex].inUse)
    {
        Debug_LOG_ERROR("Unused client %d", clientId);
        return -1;
    }

    internal_socket_control_block_mutex_lock();

    if (instance.clients[clientIndex].currentSocketsInUse >=
        instance.clients[clientIndex].currentSocketsInUse);

    if (instance.clients[clientIndex].currentSocketsInUse >=
        instance.clients[clientIndex].socketQuota)
    {
        Debug_LOG_ERROR("No free sockets available for client %d", clientIndex);
        internal_socket_control_block_mutex_unlock();
        return -1;
    }

    int handle = -1;

    for (int i = 0; i < instance.number_of_sockets; i++)
    {
        if (instance.sockets[i].status == SOCKET_FREE)
        {
            instance.sockets[i].status = SOCKET_IN_USE;
            instance.sockets[i].implementation_socket = impl_sock;
            instance.sockets[i].parentHandle = -1;
            instance.sockets[i].current_error = OS_SUCCESS;
            instance.sockets[i].clientId = clientId;
            instance.sockets[i].pendingConnections = 0;
            instance.sockets[i].socketType = 0;
            instance.sockets[i].connected = false;
            handle = i;
            break;
        }
    }
    internal_socket_control_block_mutex_unlock();

    if (handle == -1)
    {
        Debug_LOG_ERROR("No free sockets available");
    }
    else
    {
        Debug_LOG_DEBUG("Reserved socket handle %d", handle);
        instance.clients[clientIndex].currentSocketsInUse++;
    }

    return handle;
}

//------------------------------------------------------------------------------
// free a handle
void
free_handle(
    const int handle,
    const int clientId)
{
    if (handle < 0 || handle >= instance.number_of_sockets)
    {
        Debug_LOG_ERROR("Trying to free invalid handle");
        return;
    }

    const int clientIndex = get_client_index_from_clientId(clientId);
    if (clientIndex < 0)
    {
        Debug_LOG_ERROR("Failed to get client index from clientId %d", clientId);
        return;
    }

    if (!instance.clients[clientIndex].inUse)
    {
        Debug_LOG_ERROR("Trying to free handle for unused client %d", clientId);
        return;
    }

    if (instance.sockets[handle].clientId != clientId)
    {
        Debug_LOG_ERROR("Trying to free handle that does not belong to client");
        return;
    }

    internal_socket_control_block_mutex_lock();

    instance.clients[clientIndex].currentSocketsInUse--;

    instance.sockets[handle].status = SOCKET_FREE;
    instance.sockets[handle].implementation_socket = NULL;
    instance.sockets[handle].parentHandle = -1;
    instance.sockets[handle].clientId = -1;
    instance.sockets[handle].pendingConnections = 0;
    instance.sockets[handle].eventMask = 0;
    instance.sockets[handle].current_error = 0;
    instance.sockets[handle].socketType = 0;
    instance.sockets[handle].connected = false;
    internal_socket_control_block_mutex_unlock();

    Debug_LOG_DEBUG("Freed socket handle %d", handle);
}

//------------------------------------------------------------------------------
// assign an accepted handle to its listening parent socket
void
set_parent_handle(
    const int handle,
    const int parentHandle)
{
    if (handle < 0 || handle >= instance.number_of_sockets)
    {
        Debug_LOG_ERROR("set_parent_handle: Invalid handle");
        return;
    }
    if (parentHandle < 0 || parentHandle >= instance.number_of_sockets)
    {
        Debug_LOG_ERROR("set_parent_handle: Invalid parent handle");
        return;
    }
    internal_socket_control_block_mutex_lock();
    instance.sockets[handle].parentHandle = parentHandle;
    instance.sockets[handle].clientId = instance.sockets[parentHandle].clientId;
    internal_socket_control_block_mutex_unlock();
}

//------------------------------------------------------------------------------
// get dataport for handle
const OS_Dataport_t*
get_dataport_for_handle(
    const int handle)
{
    if (handle < 0 || handle >= instance.number_of_sockets)
    {
        Debug_LOG_ERROR("get_dataport_for_handle: Invalid handle");
        return NULL;
    }
    return &(instance.sockets[handle].buf);
}

//------------------------------------------------------------------------------
// notify any client that has pending socket events
static void
notify_clients_about_pending_events(
    void)
{
    // loop through all clients
    for (int i = 0; i < instance.number_of_clients; i++)
    {
        if (instance.clients[i].inUse)
        {
            // check for sockets with old pending events
            if (!instance.clients[i].needsToBeNotified)
            {
                for (int j = 0; j < instance.number_of_sockets; j++)
                {
                    if ((instance.sockets[j].status == SOCKET_IN_USE)
                        && (instance.sockets[j].eventMask != 0)
                        && (instance.sockets[j].clientId == instance.clients[i].clientId))
                    {
                        instance.clients[i].needsToBeNotified = true;
                    }
                }
            }

            // send out notifications to all client with pending events
            if (instance.clients[i].needsToBeNotified)
            {
                if (NULL != instance.clients[i].eventNotify)
                {
                    Debug_LOG_TRACE("Notify client %d, clientId: %d", i,
                                    instance.clients[i].clientId);
                    instance.clients[i].eventNotify();
                }
                else
                {
                    Debug_LOG_ERROR("Found empty function pointer. "
                                    "Cannot signal Client %d", i);
                }
                instance.clients[i].needsToBeNotified = false;
            }
        }
    }
}

//------------------------------------------------------------------------------
OS_Error_t
NetworkStack_init(
    const NetworkStack_CamkesConfig_t* const camkes_config,
    const OS_NetworkStack_AddressConfig_t* const config)
{
    if ((NULL == camkes_config) || (NULL == config))
    {
        Debug_LOG_ERROR("%s: cannot accept NULL arguments", __func__);
        return OS_ERROR_INVALID_PARAMETER;
    }

    instance.camkes_cfg = camkes_config;
    instance.cfg        = config;

    instance.sockets    = instance.camkes_cfg->internal.sockets;
    instance.number_of_sockets
        = camkes_config->internal.number_of_sockets;
    instance.number_of_clients
        = camkes_config->internal.number_of_clients;
    instance.clients    = instance.camkes_cfg->internal.clients;

    NetworkStack_Interface_t network_stack = network_stack_pico_get_config();

    // initialize Network Stack and set API functions
    network_stack.stack_init();
    // initialize NIC
    OS_Error_t err = network_stack.nic_init(instance.cfg);
    if (err != OS_SUCCESS)
    {
        Debug_LOG_ERROR("initialize_nic() failed, error %d", err);
        return OS_ERROR_GENERIC;
    }

    return OS_SUCCESS;
}


//------------------------------------------------------------------------------
// CAmkES run()
OS_Error_t
NetworkStack_run(void)
{
    if ((NULL == instance.camkes_cfg) || (NULL == instance.cfg))
    {
        Debug_LOG_ERROR("%s: cannot run on missing or failed initialization", __func__);
        return OS_ERROR_NOT_INITIALIZED;
    }

    NetworkStack_Interface_t network_stack = network_stack_pico_get_config();

    // enter endless loop processing events
    for (;;)
    {
        // wait for event ( 1 sec tick, write, read)
        wait_network_event();

        internal_network_stack_thread_safety_mutex_lock();
        // let stack process the event
        network_stack.stack_tick();
        notify_clients_about_pending_events();
        internal_network_stack_thread_safety_mutex_unlock();
    }

    Debug_LOG_WARNING("network_stack_event_loop() terminated gracefully");

    return OS_SUCCESS;
}
