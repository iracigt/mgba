/* Copyright (c) 2017 Grant Iraci
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */


 #include <mgba/internal/gba/sio/RFUClient.h>
 #include <mgba/core/log.h>
 #include <mgba/internal/gba/sio/rfu.h>

void _RFUClientExecCommand(struct RFUClient* client);

void RFUClientInit(struct RFUClient* client) {
    // Init socket subsystem

    client->netLen = 0;
    client->netIndex = 0;
    client->bcastLen = 0;
    client->netCmd = 0x00;
    client->clientID = 0x00000000;

    client->state = RFUCLIENT_ERR;

    SocketSubsystemInit();
    struct Address addr;
    addr.version = IPV4;
    addr.ipv4 = 0x7F000001;
    client->socket = SocketConnectTCP(3456, &addr);

    if (SOCKET_FAILED(client->socket)) {
        mLOG(GBA_RFU, WARN, "Failed to make socket connection");
        client->state = RFUCLIENT_ERR;
    } else {
        uint8_t b0, b1, b2, b3;
        SocketRecv(client->socket, &b0, 1);
        SocketRecv(client->socket, &b1, 1);
        SocketRecv(client->socket, &b2, 1);
        SocketRecv(client->socket, &b3, 1);

        client->clientID = (b0 << 24) | (b1 << 16) | (b2 << 8) | b3;
        client->state = RFUCLIENT_LEN;
        mLOG(GBA_RFU, INFO, "Got client ID: 0x%08X", client->clientID);
    }

}
void RFUClientDeInit(struct RFUClient* client) {
    SocketClose(client->socket);
    SocketSubsystemDeinit();
}

void RFUClientUpdate(struct RFUClient* client) {
    //Workhorse

    if (client->state == RFUCLIENT_ERR) {
        return;
    }

    //Check to see if data available
    Socket sock = client->socket;
    if (!SocketPoll(1, &sock, 0, 0, 1)) {
        return;
    }



    if (client->state == RFUCLIENT_LEN) {
        SocketRecv(client->socket, &client->netLen, 1);
        client->state = RFUCLIENT_CMD;
        mLOG(GBA_RFU, INFO, "Packet Length: %d", client->netLen);
    }

    sock = client->socket;
    if (!SocketPoll(1, &sock, 0, 0, 1)) {
        return;
    }

    if (client->state == RFUCLIENT_CMD) {
        SocketRecv(client->socket, &client->netCmd, 1);
        mLOG(GBA_RFU, INFO, "Packet Command: 0x%02X", client->netCmd);
        client->state = RFUCLIENT_CID;
    }

    sock = client->socket;
    if (!SocketPoll(1, &sock, 0, 0, 1)) {
        return;
    }

    if (client->state == RFUCLIENT_CID) {

        if (client->netIndex == 0) {
            SocketRecv(client->socket, client->rxBuf, 1);
            client->netIndex++;
            sock = client->socket;
            if (!SocketPoll(1, &sock, 0, 0, 1)) {
                return;
            }
        }

        SocketRecv(client->socket, client->rxBuf + 1, 1);
        client->netIndex = 0;

        mLOG(GBA_RFU, INFO, "Packet CID: 0x%04X", (client->rxBuf[0] << 8) | client->rxBuf[1]);

        if (client->netLen) {
            client->state = RFUCLIENT_DATA;
        } else {
            client->state = RFUCLIENT_LEN;
            _RFUClientExecCommand(client);
        }
    }

    if (client->state == RFUCLIENT_DATA) {

        while (true) {
            sock = client->socket;
            if (!SocketPoll(1, &sock, 0, 0, 1)) {
                return;
            }

            SocketRecv(client->socket, client->rxBuf + client->netIndex, 1);
            mLOG(GBA_RFU, INFO, "Got byte %d: %02X", client->netIndex, client->rxBuf[client->netIndex]);
            client->netIndex++;
            if (client->netIndex == client->netLen * 4) {
                client->netIndex = 0;
                client->state = RFUCLIENT_LEN;
                _RFUClientExecCommand(client);
                break;
            }
        }
    }
}

void _RFUClientExecCommand(struct RFUClient* client) {
    switch (client->netCmd) {
        case 0x1D:
            ;
            uint32_t* rxBuf = (uint32_t*)client->rxBuf;
            client->bcastLen = client->netLen;
            for (int i = 0; i < client->bcastLen; ++i) {
                client->bcastData[i] = ntohl(rxBuf[i]);
                mLOG(GBA_RFU, INFO, "BCAST RX \t0x%02X: 0x%08X", i, client->bcastData[i]);
            }
            break;

        default:
            mLOG(GBA_RFU, WARN, "Unknown network command: 0x%02X", client->netCmd);
    }
}

void RFUClientSendBroadcastData(struct RFUClient* client, uint32_t* data, uint8_t len) {
    //Send out data

    for (int i = 0; i < len; ++i) {
		mLOG(GBA_RFU, INFO, "BCAST \t0x%02X: 0x%08X", i, data[i]);
	}

    uint32_t txBuf = len << 24 | 0x00160000 | ((client->clientID >> 16) & 0xFFFF);

    txBuf = htonl(txBuf);
    SocketSend(client->socket, &txBuf, sizeof(txBuf));

    for (int i = 0; i < len; ++i) {
        txBuf = data[i];
        txBuf = htonl(txBuf);
        SocketSend(client->socket, &txBuf, sizeof(txBuf));
    }




}

uint32_t* RFUClientGetBroadcastData(struct RFUClient* client, uint8_t* len) {

    *len = client->bcastLen;
    return client->bcastData;

}

uint32_t RFUClientGetClientID(struct RFUClient* client) {
    return client->clientID;
}
