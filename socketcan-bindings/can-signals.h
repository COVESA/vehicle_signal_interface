#ifndef __CAN_SIGNALS_H
#define __CAN_SIGNALS_H

#include <linux/can.h>
#include <stdint.h>
#include <stdbool.h>

enum CanSignalType {
    CAN_SIGNAL_BOOL = 0,
    CAN_SIGNAL_UINT8,
    CAN_SIGNAL_UINT16
};

struct CanSignal {
    canid_t canId;
    uint32_t sigId;
    const char *sigName;
    uint8_t start;
    uint8_t end;
    uint32_t min;
    uint32_t max;
    enum CanSignalType type;
};

typedef void (*signalUInt8Clbk)(const char *name, uint32_t id, uint8_t value);
typedef void (*signalUInt16Clbk)(const char *name, uint32_t id, uint16_t value);
typedef void (*signalBoolClbk)(const char *name, uint32_t id, bool value);
typedef void (*siglog_t)(int priority, const char* format, ...);

bool initCanSignals(const struct CanSignal *sig, uint32_t signals_cnt, siglog_t siglog, signalBoolClbk boolClbk, signalUInt8Clbk uint8Clbk, signalUInt16Clbk uint16Clbk);
void processCanFrame(const struct can_frame* frame);

#endif /* !__CAN_SIGNALS_H */
