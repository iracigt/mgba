/* Copyright (c) 2017 Grant Iraci
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <mgba/internal/gba/sio.h>
#include <mgba/internal/gba/io.h>
#include <mgba/internal/gba/gba.h>

#include <mgba/internal/gba/sio/rfu.h>

mLOG_DEFINE_CATEGORY(GBA_RFU, "GBA RFU");

uint16_t _RFUSioWriteRegister(struct GBASIODriver* driver, uint32_t address, uint16_t value);
bool _RFUReset(struct GBASIODriver* driver);
uint32_t _RFUTransferData(struct GBASIO* gbaSIO, uint32_t sioData);
void _RFUSendDataToGBA(struct GBA* gba, uint32_t data, bool delay);
void _RFUTransferCallback();
void _RFUExecCommand(struct GBARFU* rfu);
void _RFUSwitchState(struct GBARFU* rfu, enum GBARFUState state);

void _RFUBroadcastData(uint32_t* data, uint8_t len);

void GBAHardwareRFUInit(struct GBA* gba) {

	gba->memory.hw.rfu.d.init = _RFUReset;
	gba->memory.hw.rfu.d.deinit = 0;
	gba->memory.hw.rfu.d.load = 0;//_RFUReset;
	gba->memory.hw.rfu.d.unload = 0;

	gba->memory.hw.rfu.d.writeRegister = _RFUSioWriteRegister;

	gba->memory.hw.rfu.xferDoneEvent.context = &gba->memory.hw.rfu;
	gba->memory.hw.rfu.xferDoneEvent.name = "GBA SIO Wireless Adapter";
	gba->memory.hw.rfu.xferDoneEvent.callback = _RFUTransferCallback;
	gba->memory.hw.rfu.xferDoneEvent.priority = 0x80;

	gba->memory.hw.rfu.xferPending = false;
	//gba->memory.hw.rfu.doRevPolarity = false;
	gba->memory.hw.rfu.hosting = false;

	RFUClientInit(&gba->memory.hw.rfu.net);

	GBASIOSetDriver(&gba->sio, &gba->memory.hw.rfu.d, SIO_NORMAL_32);
}

void GBAHardwareRFUUpdate(struct GBA* gba) {

	struct GBARFU* rfu = &gba->memory.hw.rfu;

	RFUClientUpdate(&rfu->net);

	if ((rfu->state == RFU_WAITING) && RFUClientMessageAvailable(&rfu->net)) {
		rfu->polarityReversed = true;
		_RFUSendDataToGBA(gba, 0x99660028, false);
		_RFUSwitchState(rfu, RFU_READY);
		//rfu->polarityReversed = false;
	}
}

bool _RFUReset(struct GBASIODriver* driver) {
	_RFUSwitchState(&driver->p->p->memory.hw.rfu, RFU_INIT);
	mLOG(GBA_RFU, INFO, "Adapter reset");
	printf("%s","Adapter reset\n");
	return true; //????
}

void _RFUSwitchState(struct GBARFU* rfu, enum GBARFUState state) {

	switch (state) {
		case RFU_INIT:
			//fallthrough
			rfu->polarityReversed = false;
		case RFU_READY:

			//DON'T fallthrough, resetting index messes up TRANS
			//Makes trans always send index 0 as last index
			break;
		case RFU_TRANS:
		case RFU_RECV:
			rfu->xferIndex = 0;
			//fallthrough
		case RFU_WAITING:
		case RFU_ERR:
			break;
	}

	rfu->state = state;
}

//PC = driver->p->p->cpu->gprs[ARM_PC]
uint16_t _RFUSioWriteRegister(struct GBASIODriver* driver, uint32_t address, uint16_t value) {
	switch (address) {
			case REG_SIOCNT:
				;
				struct GBARFU* rfu = &driver->p->p->memory.hw.rfu;
                union GBASIOCNTUnion val;
                val.siocnt = value;

				uint32_t rxData = (driver->p->p->memory.io[REG_SIODATA32_HI >> 1] << 16) | (driver->p->p->memory.io[REG_SIODATA32_LO >> 1]);
				//mLOG(GBA_RFU, DEBUG, "REG_SIOCNT Write: 0x%04x PC: 0x%08X Rev: %d", value, driver->p->p->cpu->gprs[ARM_PC], rfu->polarityReversed);//(driver->p->p->memory.io[REG_SIODATA32_HI >> 1] << 16) | (driver->p->p->memory.io[REG_SIODATA32_LO >> 1]));

				if (val.normalControl.idleSo) {
					val.normalControl.si = rfu->polarityReversed;
				} else {
					val.normalControl.si = !rfu->polarityReversed;
				}


				//Fix for Mario Golf, force the clock speed to 2 MHz. Why is this needed?
				if (val.normalControl.sc && !val.normalControl.si) {
					val.normalControl.internalSc = 1;
				}

				//Transfer with internal clock, used for most commands
				if (val.normalControl.start && !rfu->xferPending && val.normalControl.sc) {

					//mLOG(GBA_RFU, INFO, "RFU RX: 0x%08X", rxData);
					//printf("RFU RX: 0x%08X\n", rxData);

					uint32_t xferData = _RFUTransferData(driver->p, rxData);

					_RFUSendDataToGBA(driver->p->p, xferData, true);

				//Transfer with external clock, used for blocking commands
			} else if (val.normalControl.start && !rfu->xferPending && !val.normalControl.sc) { //  && (rfu->state != RFU_INIT)
					//mLOG(GBA_RFU, DEBUG, "Ext clock transfer: 0x%08X", rxData);
					//printf("RFU ERX: 0x%08X\n", rxData);
					if (rxData == 0x996600A8) {
						//Read GBA's ack of 0x28
						_RFUSendDataToGBA(driver->p->p, 0x80000000, true);
						_RFUSwitchState(rfu, RFU_READY);

					} else if (rxData == 0x80000000) {
						_RFUSwitchState(rfu, RFU_WAITING);
					} else {
						//Error
						mLOG(GBA_RFU, INFO, "Invalid ext clock transfer: 0x%08X", rxData);
						_RFUSwitchState(rfu, RFU_INIT);

					}
				}

                value = val.siocnt;
				// Caller does this
				//driver->p->siocnt = value;
				break;
		case REG_SIODATA32_HI:
			//mLOG(GBA_RFU, DEBUG, "REG_SIODATA32_HI Write: 0x%04X", value);
			driver->p->p->memory.io[REG_SIODATA32_HI >> 1] = value;
			break;
		case REG_SIODATA32_LO:
			//mLOG(GBA_RFU, DEBUG, "REG_SIODATA32_LO Write: 0x%04X", value);
			driver->p->p->memory.io[REG_SIODATA32_LO >> 1] = value;
			break;
		case REG_RCNT:
			//mLOG(GBA_RFU, DEBUG, "REG_RCNT Write: 0x%04X", value);
			break;
		default:
			mLOG(GBA_RFU, STUB, "Stub SIO Reg 0x%08X Write: 0x%04X", address, value);
	}

	return value;
}

uint32_t _RFUTransferData(struct GBASIO* sio, uint32_t sioData) {

	struct GBARFU* rfu = &sio->p->memory.hw.rfu;

	switch (rfu->state) {

		case RFU_INIT:
			//End of init sequence
			if (sioData == 0xB0BB8001)
			{
			    rfu->state = RFU_READY;
				mLOG(GBA_RFU, INFO, "Adapter initialized!");
			    sio->normalControl.si = 0;
			}

			return ((sioData & 0xFFFF) << 16) | (sioData >> 16);

		case RFU_READY:
			//Command format: 0x9966llcc
			// ll = length of params in words
			// cc = command number

			//All commands start with 0x9966
			if (sioData >> 16 != 0x9966) {
				mLOG(GBA_RFU, WARN, "Invalid RFU command string: 0x%08X", sioData);
				break;
			}

			rfu->currCmd = sioData & 0xFF; //cc
			rfu->xferLen = (sioData >> 8) & 0xFF; //ll

			mLOG(GBA_RFU, DEBUG, "RFU command 0x%02X of length 0x%02X", rfu->currCmd, rfu->xferLen);

			if (!rfu->xferLen) {
				_RFUExecCommand(rfu);
			} else {
				_RFUSwitchState(rfu, RFU_RECV);
			}

			return 0x80000000;

		case RFU_TRANS:

			if (sioData != 0x80000000) {
				//Error
				break;
			}

			if (rfu->xferIndex == rfu->xferLen) {
				_RFUSwitchState(rfu, RFU_READY);
			}

			return rfu->xferBuf[rfu->xferIndex++];

		case RFU_RECV:
			//Note that a xferLen of 0 means send 1 word
			//xferLen refers to the number of words beyond the acknowledge
			rfu->xferBuf[rfu->xferIndex] = sioData;

			rfu->xferIndex++;
			if (rfu->xferIndex == rfu->xferLen) {
				_RFUExecCommand(rfu);
			}

			return 0x80000000;

		case RFU_WAITING:
		case RFU_ERR:
			 break;

		default:
			mLOG(GBA_RFU, FATAL, "Invalid RFU state: 0x%02X", rfu->state);
	}

	mLOG(GBA_RFU, WARN, "RFU Err in state: 0x%02X", rfu->state);
	_RFUSwitchState(rfu, RFU_ERR);
	return 0x996600EE;
}

void _RFUExecCommand(struct GBARFU* rfu) {
	switch (rfu->currCmd) {
			case 0x10: // Initialize
			case 0x3D: // Initialize

				rfu->hosting = false;

				rfu->xferLen = 0;
				break;

			case 0x11: // Signal strength? Num connections?

				//TODO: What does real adapter do?
				//Perhaps each byte is signal strength of corresponding client
				//0x44332211, where 0x00 is weakest and 0xFF is strongest
				//TODO: base signal strength on ping?
				rfu->xferBuf[1] = 0xFFFFFFFF;
				rfu->xferLen = 1;

				break;

			case 0x14: // Something to do with error checking and client joining?
			//VBAM does wierd stuff here. Bugs?
			case 0x13: // Error checking? Get adapter ID?
				//TODO: Start broadcasting room name?
				//TODO: What does real adapter uses as lower half?
				//>800,000 copies sold in Japan alone, so not a true UUID, can't be unique
				//rfu->xferBuf[1] = (rfu->hosting ? 0x100 : RFUClientGetClientID(&rfu->net) << 16) | ((playerNum << 3) + 0x61f1); // VBAM uses 0x61f1
				rfu->xferLen = 1;
				rfu->xferBuf[1] = 0x02000000 | RFUClientGetClientID(&rfu->net);
				break;

			case 0x16: // Broadcast data
				RFUClientSendBroadcastData(&rfu->net, rfu->xferBuf, rfu->xferLen);

				rfu->xferLen = 0;
				break;

			case 0x17: // Setup
				rfu->xferLen = 0;
				break;

			case 0x19: // Listen for client to join
				//TODO: Start broadcasting room name

				rfu->hosting = true;
				//rfu->clientID = 0;

				rfu->xferLen = 0;
				break;

			case 0x1A: // SERVER: Check for connection from a client
				//Returns ID of adapter (maybe?) if connection availible, nothing otherwise
				;
				uint32_t const* clients = RFUClientGetClientList(&rfu->net, &rfu->xferLen);

				for (int i = 0; i < rfu->xferLen; i++) {
					rfu->xferBuf[i+1] = clients[i];
					//mLOG(GBA_RFU, DEBUG, "CONN READ 0x%08X", rfu->xferBuf[i+1]);
				}

				break;

			case 0x1B: // SERVER: Resets something without causing client disconnect
				//TODO: ??? clear broadcast data something ???

				rfu->xferLen = 0;
				break;

			case 0x1C: // Clear internal buffers
				//TODO: Clear network buffers

				rfu->hosting = false;

				rfu->xferLen = 0;
				break;

			case 0x1E: //Also reset some stuff??
				//TODO: Reset stuff
			case 0x1D: // Read broadcast data
				;
				uint32_t const* bcastData = RFUClientGetBroadcastData(&rfu->net, &rfu->xferLen);
				memcpy(rfu->xferBuf+1, bcastData, rfu->xferLen * sizeof(uint32_t));

				break;

			case 0x1F: //Connect to server
				mLOG(GBA_RFU, INFO, "Connecting to server 0x%08X", rfu->xferBuf[0]);
				RFUClientConnectToServer(&rfu->net, rfu->xferBuf[0]);

				rfu->xferLen = 0;
				break;

			case 0x20: // I have no idea what this does, related to 0x1F, probably implemented wrong
			case 0x21: // I have no idea what this does, related to 0x1F, probably implemented wrong
				//TODO: What does real adapter do? More research needed
				//Guessing here, maybe connection ACK?
				//Game repeats this then after a while says the trainer is not available
				rfu->xferBuf[1] = 0x00000000;

				rfu->xferLen = 1;
				break;

			case 0x25: //Send data and block until data available
				//Actual blocking is triggered by starting xfer w/ ext clock
			case 0x24: //Send data to connected adapters
				RFUClientSendDataToConnected(&rfu->net, rfu->xferBuf, rfu->xferLen);

				rfu->xferLen = 0;
				break;

			case 0x26: //Receive data. Returns pending data
				RFUClientReadMessage(&rfu->net, rfu->xferBuf+1, &rfu->xferLen);
				break;

			case 0x27: // Block until data is available
				//Actual blocking is triggered by starting xfer w/ ext clock
				rfu->xferLen = 0;
				break;

			case 0x30: // Disconnect
				RFUClientDisconnectAll(&rfu->net);
				rfu->xferLen = 0;
				break;

			case 0xEE:
				mLOG(GBA_RFU, WARN, "RFU Rx'd Err in state: 0x%02X", rfu->state);
				_RFUSwitchState(rfu, RFU_INIT);
				return;
			default:
				mLOG(GBA_RFU, WARN, "Unimplemented command: 0x%02X", rfu->currCmd);
	}
	//Acknowledge command
	rfu->xferBuf[0] = 0x99660080 | rfu->xferLen << 8 | rfu->currCmd;
	_RFUSwitchState(rfu, RFU_TRANS);
	rfu->polarityReversed = false;
}

void _RFUSendDataToGBA(struct GBA* gba, uint32_t data, bool delay) {
	gba->memory.io[REG_SIODATA32_HI >> 1] = (uint16_t) (data >> 16);
	gba->memory.io[REG_SIODATA32_LO >> 1] = (uint16_t) data;
	//mLOG(GBA_RFU, INFO, "RFU TX: 0x%08X", data);
	//printf("RFU TX: 0x%08X POL: %d\n", data, gba->memory.hw.rfu.polarityReversed);

	//Schedule IRQ
	if (delay) {
		gba->memory.hw.rfu.xferPending = true;
		mTimingSchedule(&gba->timing, &gba->memory.hw.rfu.xferDoneEvent, 256);
	} else {
		_RFUTransferCallback(0, &gba->memory.hw.rfu, 0);
	}
}

void _RFUTransferCallback(struct mTiming* timing, void* user, uint32_t cyclesLate) {

	UNUSED(timing);
	UNUSED(cyclesLate);

	struct GBARFU* rfu = user;

	//mLOG(GBA_RFU, DEBUG, "RFU XFER COMPLETE (%d cycles late) IRQ: %d", cyclesLate, rfu->d.p->normalControl.irq);

	//Should be only if rfu->state != RFU_INIT?
	// if (rfu->d.p->normalControl.idleSo) {
	// 	rfu->d.p->normalControl.si = rfu->polarityReversed;
	// } else {
	// 	rfu->d.p->normalControl.si = !rfu->polarityReversed;
	// }
	//rfu->polarityReversed = !(rfu->d.p->siocnt & 1);
	rfu->d.p->normalControl.si = !rfu->polarityReversed;
	//rfu->d.p->siocnt |= (rfu->d.p->siocnt & 1) << 2;


	rfu->xferPending = false;
	rfu->d.p->normalControl.start = 0;

    if (rfu->d.p->normalControl.irq) {
		GBARaiseIRQ(rfu->d.p->p, IRQ_SIO);
	}
}
