/* Copyright (c) 2017 Grant Iraci
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QUEUE_H
#define QUEUE_H

#include <mgba-util/common.h>

CXX_GUARD_START

#define DECLARE_QUEUE(NAME, TYPE) \
    struct NAME ## Node { \
        TYPE data; \
        struct NAME ## Node* next; \
        struct NAME ## Node* prev; \
    }; \
	struct NAME { \
		struct NAME ## Node* front; \
        struct NAME ## Node* back; \
	}; \
	void NAME ## Init(struct NAME* queue); \
	void NAME ## Deinit(struct NAME* queue); \
    void NAME ## Enqueue(struct NAME* queue, TYPE item); \
	TYPE NAME ## Dequeue(struct NAME* queue); \
    void NAME ## Clear(struct NAME* queue); \
    bool NAME ## Empty(struct NAME* queue); \


#define DEFINE_QUEUE(NAME, TYPE) \
	void NAME ## Init(struct NAME* queue) { \
		queue->front = 0; \
        queue->back = 0; \
	} \
	void NAME ## Deinit(struct NAME* queue) { \
        NAME ## Clear(queue); \
	} \
    bool NAME ## Empty(struct NAME* queue) { \
        return (queue->front == 0); \
    } \
	TYPE NAME ## Dequeue(struct NAME* queue) { \
		TYPE data = queue->front->data; \
        struct NAME ## Node* node = queue->front; \
        queue->front = node->prev; \
        free(node); \
        return data; \
	} \
    TYPE NAME ## Peek(struct NAME* queue) { \
        return queue->front->data; \
	} \
	void NAME ## Enqueue(struct NAME* queue, TYPE item) { \
        struct NAME ## Node* node = malloc(sizeof(struct NAME ## Node)); \
        node->data = item; \
        node->next = queue->back; \
        node->prev = 0; \
        queue->back = node; \
        if (queue->front == 0) { \
            queue->front = node; \
        } \
	} \
	void NAME ## Clear(struct NAME* queue) { \
        struct NAME ## Node* curNode = queue->front; \
        while(curNode) { \
            struct NAME ## Node* node = curNode; \
            curNode = node->next; \
            free(node); \
        } \
        queue->front = 0; \
        queue->back = 0; \
	} \

CXX_GUARD_END

#endif
