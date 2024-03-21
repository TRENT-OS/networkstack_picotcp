/*
 * Network Stack NIC level functions for picoTCP
 *
 * Copyright (C) 2020-2024, HENSOLDT Cyber GmbH
 * 
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "OS_Error.h"
#include "OS_Types.h"

OS_Error_t
pico_nic_initialize(
    const OS_NetworkStack_AddressConfig_t* config);
