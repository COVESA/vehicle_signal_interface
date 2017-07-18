#include <stdint.h>
#include "can-signals.h"

struct CanSignal geniviDemoSignals[] = {
    {
        .canId = 0x111,
        .sigId = 9,
        .sigName = "vehicle.engine.oilpressure",
        .start = 0,
        .end = 7,
        .min = 0,
        .max = 160,
        .type = CAN_SIGNAL_UINT8
    },
    {
        .canId = 0x111,
        .sigId = 7,
        .sigName = "vehicle.engine.rpm",
        .start = 8,
        .end = 15,
        .min = 0,
        .max = 100,
        .type = CAN_SIGNAL_UINT8
    },
    {
        .canId = 0x111,
        .sigId = 8,
        .sigName = "vehicle.engine.temperature",
        .start = 16,
        .end = 23,
        .min = 0,
        .max = 120,
        .type = CAN_SIGNAL_UINT8
    },
    {
        .canId = 0x111,
        .sigId = 3,
        .sigName = "vehicle.turnsignal.right",
        .start = 24,
        .end = 24,
        .min = 0,
        .max = 1,
        .type = CAN_SIGNAL_BOOL
    },
    {
        .canId = 0x111,
        .sigId = 2,
        .sigName = "vehicle.turnsignal.left",
        .start = 25,
        .end = 25,
        .min = 0,
        .max = 1,
        .type = CAN_SIGNAL_BOOL
    },
    {
        .canId = 0x111,
        .sigId = 6,
        .sigName = "vehicle.battery",
        .start = 32,
        .end = 39,
        .min = 100,
        .max = 140,
        .type = CAN_SIGNAL_UINT8
    },
    {
        .canId = 0x111,
        .sigId = 10,
        .sigName = "vehicle.transmission.gear",
        .start = 40,
        .end = 43,
        .min = 0,
        .max = 8,
        .type = CAN_SIGNAL_UINT8
    },
    {
        .canId = 0x111,
        .sigId = 1,
        .sigName = "vehicle.ignition",
        .start = 44,
        .end = 47,
        .min = 0,
        .max = 8,
        .type = CAN_SIGNAL_UINT8
    },
    {
        .canId = 0x111,
        .sigId = 5,
        .sigName = "vehicle.fuel",
        .start = 48,
        .end = 55,
        .min = 0,
        .max = 100,
        .type = CAN_SIGNAL_UINT8
    },
    {
        .canId = 0x111,
        .sigId = 4,
        .sigName = "vehicle.speed",
        .start = 56,
        .end = 63,
        .min = 0,
        .max = 220,
        .type = CAN_SIGNAL_UINT8
    },
};

uint32_t geniviDemoSignals_cnt = sizeof(geniviDemoSignals)/sizeof(geniviDemoSignals[0]);
