/*
 * Network Stack NIC level functions for picoTCP
 *
 * Copyright (C) 2020-2024, HENSOLDT Cyber GmbH
 * 
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "OS_Error.h"
#include "OS_Dataport.h"

#include "network/OS_NetworkStackTypes.h"
#include "network/OS_SocketTypes.h"

#include "network_stack_core.h"
#include "network_stack_config.h"
#include "pico_device.h"
#include "pico_stack.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

// The dataport include defines a separate PACKED
#undef PACKED
#include <camkes/dataport.h>

#define ENCODE_DMA_ADDRESS(buf) ({ \
    dataport_ptr_t wrapped_ptr = dataport_wrap_ptr(buf); \
    assert(wrapped_ptr.id < (1 << 8) && wrapped_ptr.offset < (1 << 24)); \
    void *new_buf = (void *)(((uintptr_t)wrapped_ptr.id << 24) | ((uintptr_t)wrapped_ptr.offset)); \
    new_buf; })

#define DECODE_DMA_ADDRESS(buf) ({\
    dataport_ptr_t wrapped_ptr = {.id = ((uintptr_t)buf >> 24), .offset = (uintptr_t)buf & MASK(24)}; \
    void *ptr = dataport_unwrap_ptr(wrapped_ptr); \
    ptr; })

// currently we support only one NIC
static struct pico_device os_nic;

#define RX_ROBJ_QUEUE_LEN 256

typedef struct {
    virtqueue_ring_object_t buf[RX_ROBJ_QUEUE_LEN];
    size_t head;
    size_t len;
} rx_robj_queue_t;

static rx_robj_queue_t rx_robj_queue = {
    .head = 0,
    .len = 0,
};

//------------------------------------------------------------------------------
// Called by picoTCP to send one frame
static int
nic_send_frame(
    struct pico_device* dev,
    void*               buf,
    int                 buf_len)
{
    // currently we support only one NIC
    Debug_ASSERT(&os_nic == dev);

    virtqueue_device_t* vq_send = get_send_virtqueue();
    virtqueue_ring_object_t robj;
    int ok = virtqueue_get_available_buf(vq_send, &robj);
    if (!ok)
    {
        Debug_LOG_DEBUG("Send virtqueue: Not enough data in available, retrying.");
        return -1;
    }

    void* dma_addr;
    unsigned int out_len;
    vq_flags_t flag;
    ok = virtqueue_gather_available(vq_send, &robj, &dma_addr, &out_len, &flag);
    if (!ok)
    {
        Debug_LOG_ERROR("Send virtqueue: available empty");
        return -1;
    }
    void* out_buf = DECODE_DMA_ADDRESS(dma_addr);

    if (buf_len > out_len)
    {
        Debug_LOG_ERROR("Send buffer too small");
        return -1;
    }

    memcpy(out_buf, buf, buf_len);
    ok = virtqueue_add_used_buf(vq_send, &robj, buf_len);
    if (!ok)
    {
        Debug_LOG_DEBUG("Send virtqueue, used full");
        return -1;
    }

    nic_dev_notify_send();
    return buf_len;
}

//------------------------------------------------------------------------------
static void
nic_recv_free(
    uint8_t* buf)
{
    if (rx_robj_queue.len == 0) {
        Debug_LOG_ERROR("Receive ring object queue is empty, can't free buffer");
        return;
    }
    // Dequeue ring object.
    virtqueue_ring_object_t robj = rx_robj_queue.buf[rx_robj_queue.head];
    rx_robj_queue.head = (rx_robj_queue.head + 1) % RX_ROBJ_QUEUE_LEN;
    --rx_robj_queue.len;

    virtqueue_device_t* vq_recv = get_recv_virtqueue();
    int ok = virtqueue_add_used_buf(vq_recv, &robj, 0 /* length doesn't really matter */);
    if (!ok) {
        Debug_LOG_ERROR("Receive virtqueue: used empty");
        return;
    }
}

//------------------------------------------------------------------------------
// Called after notification from driver and regularly from picoTCP stack tick
static int
nic_poll_data(
    struct pico_device* dev,
    int                 loop_score)
{
    // currently we support only one NIC
    Debug_ASSERT(&os_nic == dev);

    virtqueue_device_t* vq_recv = get_recv_virtqueue();
    virtqueue_ring_object_t robj;
    while (loop_score > 0)
    {
        int ok = virtqueue_get_available_buf(vq_recv, &robj);
        if (!ok) {
            return loop_score;
        }

        void* buf;
        unsigned int buf_len;
        vq_flags_t flag;
        ok = virtqueue_gather_available(vq_recv, &robj, &buf, &buf_len, &flag);
        if (!ok) {
            Debug_LOG_ERROR("Receive virtqueue: available empty");
            return -1;
        }
        buf = DECODE_DMA_ADDRESS(buf);

        if (rx_robj_queue.len == RX_ROBJ_QUEUE_LEN) {
            Debug_LOG_ERROR("Receive ring object queue is full");
            return -1;
        }
        // Enqueue ring object.
        size_t idx = (rx_robj_queue.head + rx_robj_queue.len) % RX_ROBJ_QUEUE_LEN;
        rx_robj_queue.buf[idx] = robj;
        ++rx_robj_queue.len;

        int ret = pico_stack_recv_zerocopy_ext_buffer_notify(dev, buf, buf_len, nic_recv_free);
        if (ret <= 0) {
            Debug_LOG_ERROR("pico_stack_recv_zerocopy_ext_buffer_notify() failed, error %d", ret);
            return -1;
        }

        --loop_score;
    }
    if (loop_score == 0 && VQ_DEV_POLL(vq_recv)) {
        internal_notify_main_loop();
        Debug_LOG_TRACE("Loop score is 0 but there is still data in the NIC");
    }

    return loop_score;
}

//------------------------------------------------------------------------------
static void
nic_destroy(
    struct pico_device* dev)
{
    // currently we support only one NIC
    Debug_ASSERT(&os_nic == dev);

    memset(dev, 0, sizeof(*dev));
}

//------------------------------------------------------------------------------
OS_Error_t
pico_nic_initialize(const OS_NetworkStack_AddressConfig_t* config)
{
    int ret;

    // currently we support only one NIC
    struct pico_device* dev = &os_nic;

    memset(dev, 0, sizeof(*dev));

    dev->send    = nic_send_frame;
    dev->poll    = nic_poll_data;
    dev->destroy = nic_destroy;

    //---------------------------------------------------------------
    // get MAC from NIC driver
    uint8_t mac[6];
    OS_Error_t err = nic_dev_get_mac_address(mac);
    if (err != OS_SUCCESS)
    {
        Debug_LOG_ERROR("nic_dev_get_mac_address() failed, error %d", err);
        nic_destroy(dev);
        return OS_ERROR_GENERIC;
    }

    Debug_LOG_INFO("MAC: %02x:%02x:%02x:%02x:%02x:%02x",
                   mac[0], mac[1], mac[2], mac[3], mac[4], mac[5] );

    static const char* drv_name  = "trentos_nic_driver";
    ret = pico_device_init(pico_stack_ctx, dev, drv_name, mac);
    if (ret != 0)
    {
        Debug_LOG_ERROR("pico_device_init() failed, error %d", ret);
        nic_destroy(dev);
        return OS_ERROR_GENERIC;
    }

    Debug_LOG_INFO("picoTCP Device created: %s", drv_name);

    //---------------------------------------------------------------
    // Assign IPv4 configuration. Unfortunately, the structures are declared
    // packed, so we cannot have pico_string_to_ipv4() write the fields
    // directly

    uint32_t ip_addr;
    ret = pico_string_to_ipv4(config->dev_addr, &ip_addr);
    if (ret != 0)
    {
        Debug_LOG_ERROR("pico_string_to_ipv4() failed translating IP address '%s', error %d",
                        config->dev_addr, ret);
        nic_destroy(dev);
        return OS_ERROR_GENERIC;
    }

    uint32_t netmask_addr;
    ret = pico_string_to_ipv4(config->subnet_mask, &netmask_addr);
    if (ret != 0)
    {
        Debug_LOG_ERROR("pico_string_to_ipv4() failed translating netmask '%s', error %d",
                        config->subnet_mask, ret);
        nic_destroy(dev);
        return OS_ERROR_GENERIC;
    }

    uint32_t gateway_addr;
    ret = pico_string_to_ipv4(config->gateway_addr, &gateway_addr);
    if (ret != 0)
    {
        Debug_LOG_ERROR("pico_string_to_ipv4() failed translating gateway address '%s', error %d",
                        config->gateway_addr, ret);
        nic_destroy(dev);
        return OS_ERROR_GENERIC;
    }

    // assign IP address and netmask
    ret = pico_ipv4_link_add(
            pico_stack_ctx,
            dev,
            (struct pico_ip4){ .addr = ip_addr },
            (struct pico_ip4){ .addr = netmask_addr });
    if (ret != 0)
    {
        Debug_LOG_ERROR("pico_ipv4_link_add() failed, error %d", ret);
        nic_destroy(dev);
        return OS_ERROR_GENERIC;
    }

    // add default route via gateway
    ret = pico_ipv4_route_add(
            pico_stack_ctx,
            (struct pico_ip4){ .addr = 0 }, /* any address */
            (struct pico_ip4){ .addr = 0 }, /* no netmask */
            (struct pico_ip4){ .addr = gateway_addr },
            1,
            NULL);
    if (ret != 0)
    {
        Debug_LOG_ERROR("pico_ipv4_route_add() failed, error %d", ret);
        nic_destroy(dev);
        return OS_ERROR_GENERIC;
    }

    return OS_SUCCESS;
}
