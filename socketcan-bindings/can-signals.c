#include "can-signals.h"
#include <syslog.h>

static const struct CanSignal* sig;
static uint32_t sig_cnt;
static signalBoolClbk sigBoolClbk;
static signalUInt8Clbk sigUInt8Clbk;
static signalUInt16Clbk sigUInt16Clbk;
static bool initialized = false;
static siglog_t syslogger;

bool initCanSignals(const struct CanSignal* signals, uint32_t signals_cnt, siglog_t siglog, signalBoolClbk boolClbk, signalUInt8Clbk uint8Clbk, signalUInt16Clbk uint16Clbk)
{
    if (signals && siglog) {
        initialized = true;
        sig = signals;
        sig_cnt = signals_cnt;
        syslogger = siglog;
        sigBoolClbk = boolClbk;
        sigUInt8Clbk = uint8Clbk;
        sigUInt16Clbk = uint16Clbk;
    }

    syslogger(LOG_INFO, "Signal extraction initialization (%hhu)", initialized);

    return initialized;
}

static uint32_t bitMask[] = {
    0x00000001,
    0x00000003,
    0x00000007,
    0x0000000f,
    0x0000001f,
    0x0000003f,
    0x0000007f,
    0x000000ff,
    0x000001ff,
    0x000003ff,
    0x000007ff,
    0x00000fff,
    0x00001fff,
    0x00003fff,
    0x00007fff,
    0x0000ffff,
};

static void processBoolSignal(const struct can_frame* frame, const struct CanSignal* sigDesc)
{
    uint8_t byteNum = sigDesc->start / 8;
    uint8_t valShift = sigDesc->start % 8;
    uint32_t mask = bitMask[sigDesc->end - sigDesc->start];
    uint8_t val;

    if (sigBoolClbk) {
        val = frame->data[byteNum];
        val >>= valShift;
        val = val & mask;

        if ((val >= sigDesc->min) && (val <= sigDesc->max)) {
            sigBoolClbk(sigDesc->sigName, sigDesc->sigId, (bool)val);
        } else {
            syslogger(LOG_WARNING, "%s(%u): value (%hhu) out of bounds min: %u, max: %u",
                sigDesc->sigName, sigDesc->sigId, val, sigDesc->min, sigDesc->max);
        }
    }
}

static void processUInt8Signal(const struct can_frame* frame, const struct CanSignal* sigDesc)
{
    uint8_t byteNum = sigDesc->start / 8;
    uint8_t valShift = sigDesc->start % 8;
    uint32_t mask = bitMask[sigDesc->end - sigDesc->start];
    uint8_t val;

    if (sigUInt8Clbk) {
        val = frame->data[byteNum];
        val >>= valShift;
        val = val & mask;

        if ((val >= sigDesc->min) && (val <= sigDesc->max)) {
            sigUInt8Clbk(sigDesc->sigName, sigDesc->sigId, val);
        } else {
            syslogger(LOG_WARNING, "%s(%d): value (%hhu) out of bounds min: %u, max: %u",
                sigDesc->sigName, sigDesc->sigId, val, sigDesc->min, sigDesc->max);
        }
    }
}

static void processUInt16Signal(const struct can_frame* frame, const struct CanSignal* sigDesc)
{
    uint8_t byteNum = sigDesc->start / 8;
    uint8_t valShift = sigDesc->start % 8;
    uint32_t mask = bitMask[sigDesc->end - sigDesc->start];
    uint16_t val;

    if (sigUInt16Clbk) {
        val = *((uint16_t*)&frame->data[byteNum]);
        val >>= valShift;
        val = val & mask;

        if ((val >= sigDesc->min) && (val <= sigDesc->max)) {
            sigUInt16Clbk(sigDesc->sigName, sigDesc->sigId, val);
        } else {
            syslogger(LOG_WARNING, "%s(%d): value (%hu) out of bounds min: %u, max: %u",
                sigDesc->sigName, sigDesc->sigId, val, sigDesc->min, sigDesc->max);
        }
    }
}

void processCanFrame(const struct can_frame* frame)
{
    uint32_t i = 0;

    syslogger(LOG_DEBUG, "Can frame processing started can_id: %x, dlc: %hhu", frame->can_id, frame->can_dlc);

    if (initialized) {
        for (i = 0; i < sig_cnt; ++i) {
            if (frame->can_id == sig[i].canId) {
                syslogger(LOG_DEBUG, "Processing signal [%u/%u], %s(%u), s: %hhu, e: %hhu, min: %u, max: %u, type: %d",
                    i + 1, sig_cnt, sig[i].sigName, sig[i].sigId, sig[i].start, sig[i].end, sig[i].min, sig[i].max, sig[i].type);

                /* Check if DLC is big enough */
                if (frame->can_dlc * 8 >= sig[i].end) {
                    switch (sig[i].type) {

                    case CAN_SIGNAL_BOOL:
                        processBoolSignal(frame, &sig[i]);
                        break;

                    case CAN_SIGNAL_UINT8:
                        processUInt8Signal(frame, &sig[i]);
                        break;

                    case CAN_SIGNAL_UINT16:
                        processUInt16Signal(frame, &sig[i]);
                        break;

                    default:
                        syslogger(LOG_WARNING, "Signal type (%d) not supported", sig[i].type);
                        break;
                    }

                } else {
                    syslogger(LOG_WARNING, "DLC(%hhu) too small to process the signal", frame->can_dlc);
                }
            }
        }
    } else {
        syslogger(LOG_WARNING, "Signal extraction not initialized");
    }
}
