// Copyright 2016 The Android Open Source Project
//
// This software is licensed under the terms of the GNU General Public
// License version 2, as published by the Free Software Foundation, and
// may be copied, distributed, and modified under those terms.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

#include "android/emulation/ClipboardPipe.h"

#include "android/clipboard-pipe.h"

namespace android {
namespace emulation {

ClipboardPipe* ClipboardPipe::Service::mPipeInstance = nullptr;
ClipboardPipe::GuestClipboardCallback ClipboardPipe::mGuestClipboardCallback =
    [](const uint8_t*, size_t){};

ClipboardPipe::ClipboardPipe(void* hwPipe, Service* svc) :
        AndroidPipe(hwPipe, svc) {}
unsigned ClipboardPipe::onGuestPoll() const {
    unsigned result = PIPE_POLL_OUT;
    if (mHostClipboardHasNewData) {
        result |= PIPE_POLL_IN;
    }
    return result;
}

int ClipboardPipe::processOperation(OperationType opType,
                                    ReadWriteState* state,
                                    const AndroidPipeBuffer* pipeBuffers,
                                    int numPipeBuffers,
                                    bool* opComplete) {
    // This indicates the number of bytes needed to be read from or written
    // to the pipe.
    uint32_t requiredBytes =
        state->sizeProcessed ?
            state->requiredBytes :
            sizeof(state->requiredBytes);

    // This will point to the buffer to be read from.
    const uint8_t* sourceBuffer = nullptr;

    // This will point to the buffer to be written into.
    uint8_t* targetBuffer = nullptr;

    const AndroidPipeBuffer* pipeBuffer = pipeBuffers;
    const AndroidPipeBuffer* endPipeBuffer = pipeBuffer + numPipeBuffers;

    // This will contain the return value of the function (number of bytes
    // read or written).
    int totalBytesProcessed = 0;

    // Indicates the offset within the pipe buffer that is currently being
    // processed.
    size_t pipeBufferOffset = 0;

    *opComplete = false;

    if (opType == OperationType::WriteToGuestClipboard) {
        // If we're writing to the guest clipboard, then the source
        // buffer is state.buffer (or state.requiredBytes, if we haven't
        // sent the length of the buffer yet).
        sourceBuffer =
            state->sizeProcessed ?
                &state->buffer[0] :
                reinterpret_cast<uint8_t*>(&(state->requiredBytes));
    } else if (opType == OperationType::ReadFromGuestClipboard) {
        // If we're reading from the guest clipboard, the target buffer
        // is the state's buffer (or state.requiredBytes, if we haven't
        // read the length of the buffer yet).
        targetBuffer =
            state->sizeProcessed ?
                &state->buffer[0] :
                reinterpret_cast<uint8_t*>(&(state->requiredBytes));
    }

    while (pipeBuffer != endPipeBuffer) {
        // Decide how many bytes need to be read/written during the current
        // iteration.
        int bytesToProcess =
            std::min(
                requiredBytes - state->processedBytes,
                static_cast<uint32_t>(pipeBuffer->size - pipeBufferOffset));

        if (opType == OperationType::WriteToGuestClipboard) {
            // const_cast is ok here since the contents of AndroidPipeBuffer
            // passed into the function were intended to be changed.
            targetBuffer =
                const_cast<uint8_t*>(pipeBuffer->data) + pipeBufferOffset;
            memcpy(
                targetBuffer,
                sourceBuffer + state->processedBytes,
                bytesToProcess);
        } else if (opType == OperationType::ReadFromGuestClipboard) {
            sourceBuffer =
                static_cast<const uint8_t*>(pipeBuffer->data) +
                    pipeBufferOffset;
            memcpy(
                targetBuffer + state->processedBytes,
                sourceBuffer,
                bytesToProcess);
        }
        state->processedBytes += bytesToProcess;
        totalBytesProcessed += bytesToProcess;
        pipeBufferOffset += bytesToProcess;
        if (pipeBufferOffset >= pipeBuffer->size) {
            ++pipeBuffer;
            pipeBufferOffset = 0;
        }

        if (state->processedBytes == requiredBytes && !state->sizeProcessed) {
            // We have either sent or received the length of clipboard data
            // in bytes. Now it is time to send/receive the buffer itself.
            state->sizeProcessed = true;
            state->processedBytes = 0;
            requiredBytes = state->requiredBytes;
            if (opType == OperationType::ReadFromGuestClipboard) {
                // If we're reading from guest clipboard,
                // make sure the buffer on our side has enough space.
                state->buffer.resize(state->requiredBytes);

                // Make sure next iterations write to state's buffer.
                targetBuffer = &state->buffer[0];
            } else if (opType == OperationType::WriteToGuestClipboard) {
                // If we're writing to the guest clipboard, just
                // make sure next iterations read from state's buffer.
                sourceBuffer = &state->buffer[0];
            }
        }
    }
    if (state->processedBytes == state->requiredBytes && state->sizeProcessed) {
        *opComplete = true;
    }
    return totalBytesProcessed;
}

int ClipboardPipe::onGuestRecv(AndroidPipeBuffer* buffers, int numBuffers) {
    int result = PIPE_ERROR_AGAIN;
    if (mHostClipboardHasNewData) {
        bool opComplete = false;
        result = processOperation(OperationType::WriteToGuestClipboard,
                                  &mGuestClipboardWriteState,
                                  buffers,
                                  numBuffers,
                                  &opComplete);
        if (opComplete) {
            mHostClipboardHasNewData = false;
        }
    }
    return result;
}

int ClipboardPipe::onGuestSend(
        const AndroidPipeBuffer* buffers,
        int numBuffers) {
    bool opComplete = false;
    int result = processOperation(OperationType::ReadFromGuestClipboard,
                                  &mGuestClipboardReadState,
                                  buffers,
                                  numBuffers,
                                  &opComplete);
    if (opComplete) {
        mGuestClipboardCallback(
            &mGuestClipboardReadState.buffer[0],
            mGuestClipboardReadState.processedBytes);
        mGuestClipboardReadState.sizeProcessed = false;
        mGuestClipboardReadState.requiredBytes = 0;
        mGuestClipboardReadState.processedBytes = 0;
    }
    return result;
}

void registerClipboardPipeService() {
    android::AndroidPipe::Service::add(new ClipboardPipe::Service());
}

void ClipboardPipe::setGuestClipboardCallback(
        ClipboardPipe::GuestClipboardCallback cb) {
    mGuestClipboardCallback = cb ? cb : [](const uint8_t*, size_t){};
}

void ClipboardPipe::setGuestClipboardContents(const uint8_t* buf, size_t len) {
    mGuestClipboardWriteState.buffer.resize(len);
    memcpy(&mGuestClipboardWriteState.buffer[0], buf, len);
    mGuestClipboardWriteState.sizeProcessed = false;
    mGuestClipboardWriteState.requiredBytes = len;
    mGuestClipboardWriteState.processedBytes = 0;
    mHostClipboardHasNewData = true;
    if (mWakeOnRead) {
        signalWake(PIPE_WAKE_READ);
    }
}

}
}

void android_init_clipboard_pipe(void) {
    android::emulation::registerClipboardPipeService();
}