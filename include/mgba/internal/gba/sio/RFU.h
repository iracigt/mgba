/* Copyright (c) 2017 Grant Iraci
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GBA_RFU_H
#define GBA_RFU_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/core/log.h>

mLOG_DECLARE_CATEGORY(GBA_RFU);

//Length of a 32 bit transfer in cycles
//2MHz clock => 1 Mbaud symbol rate => 1 Mbps data rate
//(32 bits/transfer) / (1 Mbits/s) * 16.78 Mcycles / s = 537
#define RFU_2MHZ_XFER_LEN 537

enum GBARFUState {
	RFU_INIT,
	RFU_READY,
	RFU_TRANS,
	RFU_RECV,
	RFU_WAITING,
	RFU_ERR
};

struct GBARFU {
	struct GBASIODriver d;

	struct mTimingEvent xferDoneEvent;

	enum GBARFUState state;
	bool xferPending;
	uint8_t currCmd;

};

void GBAHardwareInitRFU(struct GBA* gba);

CXX_GUARD_END

#endif
