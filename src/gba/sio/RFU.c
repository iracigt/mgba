/* Copyright (c) 2017 Grant Iraci
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <mgba/internal/gba/sio.h>
#include <mgba/internal/gba/io.h>
#include <mgba/internal/gba/gba.h>

#include <mgba/internal/gba/sio/rfu.h>

mLOG_DEFINE_CATEGORY(GBA_RFU, "GBA RFU");

uint16_t _RFUSioWriteRegister(struct GBASIODriver* driver, uint32_t address, uint16_t value);
bool _RFUReset(struct GBASIODriver* driver);
uint32_t _RFUTransferData(struct GBASIO* gbaSIO, uint32_t sioData);
void _RFUSendDataToGBA(struct GBA* gba, uint32_t data);
void _RFUTransferCallback();

void GBAHardwareInitRFU(struct GBA* gba) {

	gba->memory.hw.rfu.d.init = 0;
	gba->memory.hw.rfu.d.deinit = 0;
	gba->memory.hw.rfu.d.load = _RFUReset;
	gba->memory.hw.rfu.d.unload = 0;

	gba->memory.hw.rfu.d.writeRegister = _RFUSioWriteRegister;

	gba->memory.hw.rfu.xferDoneEvent.context = &gba->memory.hw.rfu;
	gba->memory.hw.rfu.xferDoneEvent.name = "GBA SIO Wireless Adapter";
	gba->memory.hw.rfu.xferDoneEvent.callback = _RFUTransferCallback;
	gba->memory.hw.rfu.xferDoneEvent.priority = 0x80;

	gba->memory.hw.rfu.xferPending = false;

	GBASIOSetDriver(&gba->sio, &gba->memory.hw.rfu.d, SIO_NORMAL_32);
}

bool _RFUReset(struct GBASIODriver* driver) {
	driver->p->p->memory.hw.rfu.state = RFU_INIT;
	return true; //????
}

 enum GBASIOMode _getSIOMode(struct GBASIO* sio) {
	unsigned mode = ((sio->rcnt & 0xC000) | (sio->siocnt & 0x3000)) >> 12;
	 if (mode < 8) {
		 return (enum GBASIOMode) (mode & 0x3);
	 } else {
		 return (enum GBASIOMode) (mode & 0xC);
	 }
 }

//PC = driver->p->p->cpu->gprs[ARM_PC]
uint16_t _RFUSioWriteRegister(struct GBASIODriver* driver, uint32_t address, uint16_t value) {
	switch (address) {
			case REG_SIOCNT:
				mLOG(GBA_RFU, DEBUG, "REG_SIOCNT Write: 0x%04x SIODATA: 0x%08X", value, (driver->p->p->memory.io[REG_SIODATA32_HI >> 1] << 16) | (driver->p->p->memory.io[REG_SIODATA32_LO >> 1]));

                union SIOCNTUnion val;
                val.siocnt = value;

				//Acknowledge procedure ??
				if (val.normalControl.idleSo) {
					val.normalControl.si = 0;
				} else {
					val.normalControl.si = 1;
				}

				//Fix for Mario Golf, force the clock speed to 2 MHz. Why is this needed?
				if (val.normalControl.sc && !val.normalControl.si) {
					val.normalControl.internalSc = 1;
				}

				if (val.normalControl.start && !driver->p->p->memory.hw.rfu.xferPending) {
					mLOG(GBA_RFU, INFO, "RFU RX: 0x%08X", (driver->p->p->memory.io[REG_SIODATA32_HI >> 1] << 16) | (driver->p->p->memory.io[REG_SIODATA32_LO >> 1]));
					uint32_t xferData = _RFUTransferData(driver->p, (driver->p->p->memory.io[REG_SIODATA32_HI >> 1] << 16) | (driver->p->p->memory.io[REG_SIODATA32_LO >> 1]));
					_RFUSendDataToGBA(driver->p->p, xferData);
				}

                value = val.siocnt;
				// Caller does this
				//driver->p->siocnt = value;
				break;
		case REG_SIODATA32_HI:
			mLOG(GBA_RFU, DEBUG, "REG_SIODATA32_HI Write: 0x%04X", value);
			driver->p->p->memory.io[REG_SIODATA32_HI >> 1] = value;
			break;
		case REG_SIODATA32_LO:
			mLOG(GBA_RFU, DEBUG, "REG_SIODATA32_LO Write: 0x%04X", value);
			driver->p->p->memory.io[REG_SIODATA32_LO >> 1] = value;
			break;
		case REG_RCNT:
			mLOG(GBA_RFU, DEBUG, "REG_RCNT Write: 0x%04X", value);
			break;
		default:
			mLOG(GBA_RFU, STUB, "Stub SIO Reg 0x%08X Write: 0x%04X", address, value);
	}

	return value;
}

uint32_t _RFUTransferData(struct GBASIO* sio, uint32_t sioData) {

    //End of init sequence
	if (sioData == 0xb0bb8001)
	{
        sio->p->memory.hw.rfu.state = RFU_READY;
		mLOG(GBA_RFU, INFO, "Adapter initialized!");
        sio->normalControl.si = 0;
	}

	return ((sioData & 0xFFFF) << 16) | (sioData >> 16);
}

void _RFUSendDataToGBA(struct GBA* gba, uint32_t data) {
	gba->memory.io[REG_SIODATA32_HI >> 1] = (uint16_t) (data >> 16);
	gba->memory.io[REG_SIODATA32_LO >> 1] = (uint16_t) data;
	mLOG(GBA_RFU, INFO, "RFU TX: 0x%08X", data);

	//Schedule IRQ
	gba->memory.hw.rfu.xferPending = true;
	mTimingSchedule(&gba->timing, &gba->memory.hw.rfu.xferDoneEvent, 256);

}


void _RFUTransferCallback(struct mTiming* timing, void* user, uint32_t cyclesLate) {

	UNUSED(timing);
	//UNUSED(cyclesLate);

	mLOG(GBA_RFU, DEBUG, "RFU XFER COMPLETE (%d cycles late)", cyclesLate);

	struct GBARFU* rfu = user;

    if (rfu->d.p->normalControl.irq) {
		GBARaiseIRQ(rfu->d.p->p, IRQ_SIO);
	}

	rfu->xferPending = false;
    rfu->d.p->normalControl.start = 0;

}
