/* Copyright (c) 2017 Grant Iraci
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GBA_RFU_CLIENT_H
#define GBA_RFU_CLIENT_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba-util/socket.h>

enum RFUClientRXState {
    RFUCLIENT_LEN,
    RFUCLIENT_CMD,
    RFUCLIENT_CID,
    RFUCLIENT_DATA,
    RFUCLIENT_ERR
};

struct RFUClient {

    Socket socket;

    enum RFUClientRXState state;

	uint8_t netLen;
	uint32_t netIndex;
	uint8_t rxBuf[1024];
    uint8_t txBuf[1024];
    uint8_t bcastLen;
    uint32_t bcastData[255];
	uint8_t netCmd;
	uint32_t clientID;

};

void RFUClientInit(struct RFUClient* client);
void RFUClientDeInit(struct RFUClient* client);

void RFUClientUpdate(struct RFUClient* client);

void RFUClientSendBroadcastData(struct RFUClient* client, uint32_t* data, uint8_t len);

uint32_t* RFUClientGetBroadcastData(struct RFUClient* client, uint8_t* len);
uint32_t RFUClientGetClientID(struct RFUClient* client);


CXX_GUARD_END

#endif
