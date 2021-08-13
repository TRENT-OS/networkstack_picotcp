/*
 * OS Network Stack CAmkES wrapper
 *
 * Copyright (C) 2021, HENSOLDT Cyber GmbH
 */

#include "system_config.h"

#include "OS_Dataport.h"
#include "OS_Error.h"
#include "OS_Network.h"
#include "OS_NetworkStack.h"
#include "TimeServer.h"
#include "lib_macros/Array.h"
#include "lib_debug/Debug.h"
#include "util/loop_defines.h"
#include <camkes.h>

// TODO: Update comment with constraints when we have the final form of the
// interface.
#define NUMBER_OF_CLIENTS 8

static const OS_NetworkStack_AddressConfig_t config =
{
    .dev_addr     = DEV_ADDR,
    .gateway_addr = GATEWAY_ADDR,
    .subnet_mask  = SUBNET_MASK
};

static const if_OS_Timer_t timer =
    IF_OS_TIMER_ASSIGN(timeServer_rpc, timeServer_notify);

static volatile bool initSuccessfullyCompleted = false;


// TODO: Until the connector functions are declared in the camkes header, we
// need to declare them here in order to use them.
unsigned int
networkStack_rpc_num_badges(void);

seL4_Word
networkStack_rpc_enumerate_badge(unsigned int i);

seL4_Word
networkStack_rpc_get_sender_id(void);

void*
networkStack_rpc_buf(seL4_Word client_id);

size_t
networkStack_rpc_buf_size(seL4_Word client_id);

int
get_client_id(void)
{
    return networkStack_rpc_get_sender_id() - networkStack_rpc_enumerate_badge(0);
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

//------------------------------------------------------------------------------
void
post_init(void)
{
    Debug_LOG_INFO("[NwStack '%s'] starting", get_instance_name());

    const unsigned int number_connected_clients = networkStack_rpc_num_badges();

    if (NUMBER_OF_CLIENTS < number_connected_clients)
    {
        Debug_LOG_ERROR(
            "[NwStack '%s'] is configured for %d clients, but %d clients are "
            "connected",
            get_instance_name(),
            NUMBER_OF_CLIENTS,
            number_connected_clients);
        return;
    }

    if (ARRAY_ELEMENTS(networkStack_config.clients) < number_connected_clients)
    {
        Debug_LOG_ERROR(
            "[NwStack '%s'] Configuration found for %d clients, but %d clients"
            " are connected",
            get_instance_name(),
            ARRAY_ELEMENTS(networkStack_config.clients),
            number_connected_clients);
        return;
    }

    int* max_client_sockets = calloc(sizeof(int), number_connected_clients);
    if (max_client_sockets == NULL)
    {
        Debug_LOG_ERROR(
            "[NwStack '%s'] Could not allocate resources.",
            get_instance_name());
        return;
    }

    int totalSocketsNeeded = 0;

    for (int i = 0; i < number_connected_clients; i++)
    {
        max_client_sockets[i] = networkStack_config.clients[i].socket_quota;
        totalSocketsNeeded += max_client_sockets[i]; // sockets needed for client i
    }
    // TODO: decide how the user configures this

// TODO: remove with the event rework
#define LOOP_ELEMENT                                                           \
    {                                                                          \
        .notify_write      = GEN_EMIT(e_write_),                               \
        .wait_write        = GEN_WAIT(c_write_),                               \
        .notify_read       = GEN_EMIT(e_read_),                                \
        .wait_read         = GEN_WAIT(c_read_),                                \
        .notify_connection = GEN_EMIT(e_conn_),                                \
        .wait_connection   = GEN_WAIT(c_conn_),                                \
        .accepted_handle   = -1,                                               \
    },

    // TODO: remove with the event rework
    static OS_NetworkStack_SocketResources_t socks[16] = {
#define LOOP_COUNT 16
#include "util/loop.h" // places LOOP_ELEMENT here for LOOP_COUNT times
    };

    const OS_NetworkStack_CamkesConfig_t camkes_config =
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

            .number_of_clients      = number_connected_clients,
            .number_of_sockets      = totalSocketsNeeded,
            .client_sockets_quota   = max_client_sockets,
            .sockets                = socks,
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

    OS_NetworkStack_CamkesConfig_t* p_camkes_config =
        malloc(sizeof(OS_NetworkStack_CamkesConfig_t));
    if (p_camkes_config == NULL)
    {
        Debug_LOG_ERROR(
            "[NwStack '%s'] Could not allocate resources.",
            get_instance_name());
        return;
    }

    *p_camkes_config = camkes_config;

    Debug_LOG_INFO("Clients connected %d", number_connected_clients);

    for (int i = 0; i < number_connected_clients; i++)
        Debug_LOG_INFO("Client badge #%d", networkStack_rpc_enumerate_badge(i));

    OS_Error_t ret;

    Debug_LOG_INFO("[NwStack '%s'] IP ADDR: %s", get_instance_name(), DEV_ADDR);
    Debug_LOG_INFO(
        "[NwStack '%s'] GATEWAY ADDR: %s",
        get_instance_name(),
        GATEWAY_ADDR);
    Debug_LOG_INFO(
        "[NwStack '%s'] SUBNETMASK: %s",
        get_instance_name(),
        SUBNET_MASK);

    ret = OS_NetworkStack_init(p_camkes_config, &config);
    if (ret != OS_SUCCESS)
    {
        Debug_LOG_FATAL(
            "[NwStack '%s'] OS_NetworkStack_init() failed, error %d",
            get_instance_name(),
            ret);
        return;
    }
    initSuccessfullyCompleted = true;
}

//------------------------------------------------------------------------------
int
run(void)
{
    if (!initSuccessfullyCompleted)
    {
        Debug_LOG_FATAL(
            "[NwStack '%s'] initialization failed",
            get_instance_name());
        return -1;
    }

    OS_Error_t ret = OS_NetworkStack_run();
    if (ret != OS_SUCCESS)
    {
        Debug_LOG_FATAL(
            "[NwStack '%s'] OS_NetworkStack_run() failed, error %d",
            get_instance_name(),
            ret);
        return -1;
    }

    // Actually, OS_NetworkStack_run() is not supposed to return with
    // OS_SUCCESS. We have to assume this is a graceful shutdown for some
    // reason.
    Debug_LOG_WARNING(
        "[NwStack '%s'] graceful termination",
        get_instance_name());

    return 0;
}
