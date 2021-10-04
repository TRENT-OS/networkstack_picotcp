/*
 * OS Network Stack CAmkES wrapper
 *
 * Copyright (C) 2021, HENSOLDT Cyber GmbH
 */

#include "OS_Dataport.h"
#include "OS_Error.h"
#include "OS_Network.h"
#include "OS_NetworkStack.h"

#include "TimeServer.h"
#include "lib_macros/Array.h"
#include "lib_debug/Debug.h"

#include <camkes.h>
#include <string.h>

// TODO: Update comment with constraints when we have the final form of the
// interface.
#define MAX_CLIENTS_NUM 8

static const if_OS_Timer_t timer =
    IF_OS_TIMER_ASSIGN(internal_timeServer_rpc, internal_timeServer_notify);

#if !defined(NetworkStack_PicoTcp_USE_HARDCODED_IPADDR)
static OS_NetworkStack_AddressConfig_t ipAddrConfig;
#endif
static const OS_NetworkStack_AddressConfig_t* pIpAddrConfig = NULL;

volatile static OS_NetworkStack_State_t currentState = UNINITIALIZED;

// TODO: Until all the relevant connector functions are declared in the CAmkES
// header, we need to declare the following function here in order to use it.
seL4_Word
networkStack_rpc_get_sender_id(void);

// TODO: With the current implementation these function definitions are exported
// to the library so it can make use of them. Therefore they cannot be set to
// static. This should reworked into a nicer solution that allows for a cleaner
// separation of the state management from the underlying library.
OS_NetworkStack_State_t
networkStack_getState(void)
{
    return currentState;
}

OS_Error_t
networkStack_setState(OS_NetworkStack_State_t state)
{
    switch (state)
    {
    case UNINITIALIZED:
        __attribute__ ((fallthrough));
    case INITIALIZED:
        __attribute__ ((fallthrough));
    case RUNNING:
        __attribute__ ((fallthrough));
    case FATAL_ERROR:
        currentState = state;
        return OS_SUCCESS;
    default:
        return OS_ERROR_INVALID_PARAMETER;
    }
}

int
get_client_id(void)
{
    return networkStack_rpc_get_sender_id();
}

uint8_t*
get_client_id_buf(void)
{
    return networkStack_rpc_buf(networkStack_rpc_get_sender_id());
}

int
get_client_id_buf_size(void)
{
    return networkStack_rpc_buf_size(networkStack_rpc_get_sender_id());
}

bool isValidIp4Address(const char* ipAddress)
{
    struct sockaddr_in sa;
    int result = inet_pton(AF_INET, ipAddress, &(sa.sin_addr));
    return result != 0;
}

//------------------------------------------------------------------------------
// Network stack's PicTCP OS adaption layer calls this.
uint64_t
Timer_getTimeMs(void)
{
    OS_Error_t err;
    uint64_t   ms;

    if ((err = TimeServer_getTime(&timer, TimeServer_PRECISION_MSEC, &ms)) !=
        OS_SUCCESS)
    {
        Debug_LOG_ERROR("TimeServer_getTime() failed with %d", err);
        ms = 0;
    }

    return ms;
}

//------------------------------------------------------------------------------
void
pre_init(void)
{
    // TODO: Currently intentionally left blank. Check if needed after we
    // defined the final form of the connector.
}

void
post_init(void)
{
    // TODO: Currently intentionally left blank. Check if needed after we
    // defined the final form of the connector.
}

OS_Error_t
if_config_rpc_configIpAddr(
    const OS_NetworkStack_AddressConfig_t* pConfig)
{
#if !defined(NetworkStack_PicoTcp_USE_HARDCODED_IPADDR)
    OS_NetworkStack_State_t currentState = networkStack_getState();
    if (currentState != UNINITIALIZED)
    {
        return OS_ERROR_INVALID_STATE;
    }

    if (!isValidIp4Address(pConfig->dev_addr) ||
        !isValidIp4Address(pConfig->gateway_addr) ||
        !isValidIp4Address(pConfig->subnet_mask))
    {
        return OS_ERROR_INVALID_PARAMETER;
    }
    memcpy(&ipAddrConfig, pConfig, sizeof(ipAddrConfig));
    pIpAddrConfig = &ipAddrConfig;
    return OS_SUCCESS;
#else
    return OS_ERROR_OPERATION_DENIED;
#endif
}

OS_Error_t
initializeNetworkStack(void)
{
    Debug_LOG_INFO("[NwStack '%s'] starting", get_instance_name());

    const unsigned int numberConnectedClients = networkStack_rpc_num_badges();

    if (MAX_CLIENTS_NUM < numberConnectedClients)
    {
        Debug_LOG_ERROR(
            "[NwStack '%s'] is configured for %d clients, but %d clients are "
            "connected",
            get_instance_name(),
            MAX_CLIENTS_NUM,
            numberConnectedClients);
        return OS_ERROR_OUT_OF_BOUNDS;
    }

    if (ARRAY_ELEMENTS(networkStack_config.clients) < numberConnectedClients)
    {
        Debug_LOG_ERROR(
            "[NwStack '%s'] Configuration found for %d clients, but %d clients"
            " are connected",
            get_instance_name(),
            ARRAY_ELEMENTS(networkStack_config.clients),
            numberConnectedClients);
        return OS_ERROR_OUT_OF_BOUNDS;;
    }

    static OS_NetworkStack_SocketResources_t sockets[OS_NETWORK_MAXIMUM_SOCKET_NO];

    static const event_notify_func_t notifications[MAX_CLIENTS_NUM] =
    {
        networkStack_1_event_notify_emit,
        networkStack_2_event_notify_emit,
        networkStack_3_event_notify_emit,
        networkStack_4_event_notify_emit,
        networkStack_5_event_notify_emit,
        networkStack_6_event_notify_emit,
        networkStack_7_event_notify_emit,
        networkStack_8_event_notify_emit
    };

    static OS_NetworkStack_Client_t clients[MAX_CLIENTS_NUM] = {0};

    for (int i = 0; i < numberConnectedClients; i++)
    {
        clients[i].needsToBeNotified = false;
        clients[i].inUse = true;
        clients[i].clientId = networkStack_rpc_enumerate_badge(i);
        clients[i].socketQuota = networkStack_config.clients[i].socket_quota;
        clients[i].currentSocketsInUse = 0;
        clients[i].tail = 0;
        clients[i].head = 0;
        clients[i].eventNotify = notifications[i];
    }

    static const OS_NetworkStack_CamkesConfig_t camkesConfig =
    {
        .wait_loop_event         = event_tick_or_data_wait,

        .internal =
        {
            .notify_loop        = event_internal_emit,

            .allocator_lock     = allocatorMutex_lock,
            .allocator_unlock   = allocatorMutex_unlock,

            .nwStack_lock       = nwstackMutex_lock,
            .nwStack_unlock     = nwstackMutex_unlock,

            .socketCB_lock      = socketControlBlockMutex_lock,
            .socketCB_unlock    = socketControlBlockMutex_unlock,

            .stackTS_lock       = stackThreadSafeMutex_lock,
            .stackTS_unlock     = stackThreadSafeMutex_unlock,

            .number_of_clients  = MAX_CLIENTS_NUM,
            .number_of_sockets  = OS_NETWORK_MAXIMUM_SOCKET_NO,

            .sockets            = sockets,
            .clients            = clients
        },

        .drv_nic =
        {
            .from = OS_DATAPORT_ASSIGN_SIZE(nic_from_port, NIC_DRIVER_RINGBUFFER_NUMBER_ELEMENTS),
            .to   = OS_DATAPORT_ASSIGN(nic_to_port),

            .rpc =
            {
                .dev_read       = nic_rpc_rx_data,
                .dev_write      = nic_rpc_tx_data,
                .get_mac        = nic_rpc_get_mac_address,
            }
        }
    };

    Debug_LOG_DEBUG("Clients connected: %d", numberConnectedClients);

    for (int i = 0; i < numberConnectedClients; i++)
    {
        Debug_LOG_DEBUG("Client[%d] badge #%d",
                        i, networkStack_rpc_enumerate_badge(i));
    }

    Debug_LOG_INFO("[NwStack '%s'] IP ADDR: %s",
                   get_instance_name(),
                   pIpAddrConfig->dev_addr);
    Debug_LOG_INFO(
        "[NwStack '%s'] GATEWAY ADDR: %s",
        get_instance_name(),
        pIpAddrConfig->gateway_addr);
    Debug_LOG_INFO(
        "[NwStack '%s'] SUBNETMASK: %s",
        get_instance_name(),
        pIpAddrConfig->subnet_mask);

    OS_Error_t ret = OS_NetworkStack_init(&camkesConfig, pIpAddrConfig);
    if (ret != OS_SUCCESS)
    {
        Debug_LOG_FATAL(
            "[NwStack '%s'] OS_NetworkStack_init() failed, error %d",
            get_instance_name(),
            ret);
    }
    return ret;
}


//------------------------------------------------------------------------------
int
run(void)
{
    networkStack_setState(UNINITIALIZED);
#if defined(NetworkStack_PicoTcp_USE_HARDCODED_IPADDR)
    static const OS_NetworkStack_AddressConfig_t config =
    {
        .dev_addr     = DEV_ADDR,
        .gateway_addr = GATEWAY_ADDR,
        .subnet_mask  = SUBNET_MASK
    };
    pIpAddrConfig = &config;
#endif

    while (NULL == pIpAddrConfig)
        // Not yet ready, user has not yet configured the nwStack.
    {
        seL4_Yield();
    }

    OS_Error_t ret = initializeNetworkStack();
    if (ret != OS_SUCCESS)
    {
        goto err;
    }
    // Set the INITIALIZED state to have a clean transition even though it would
    // be possible to directly transition to RUNNING here.
    networkStack_setState(INITIALIZED);

    networkStack_setState(RUNNING);
    ret = OS_NetworkStack_run();
    if (ret != OS_SUCCESS)
    {
        goto err;
    }

    // Actually, OS_NetworkStack_run() is not supposed to return with
    // OS_SUCCESS. We have to assume this is a graceful shutdown for some
    // reason.
    Debug_LOG_WARNING(
        "[NwStack '%s'] graceful termination",
        get_instance_name());

    // Set FATAL_ERROR to let connected clients know that the component will not
    // come back up again.
    networkStack_setState(FATAL_ERROR);

    return 0;

err:
    networkStack_setState(FATAL_ERROR);
    Debug_LOG_FATAL(
        "[NwStack '%s'] OS_NetworkStack_run() failed, error %d",
        get_instance_name(),
        ret);
    return -1;
}
