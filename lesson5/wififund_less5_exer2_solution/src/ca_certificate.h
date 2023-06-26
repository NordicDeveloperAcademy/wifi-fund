/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __CA_CERTIFICATE_H__
#define __CA_CERTIFICATE_H__

#define CA_CERTIFICATE_TAG 1

static const unsigned char ca_certificate[] = {
#include "globalsign_r1.der.inc"
};

#endif /* __CA_CERTIFICATE_H__ */
