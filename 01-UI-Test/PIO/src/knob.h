#ifndef KNOB_H
#define KNOB_H

#include <Arduino.h>

// Enumeration for knob states
enum KNOB_State
{
    KNOB_NULL,
    KNOB_INCREMENT,
    KNOB_DECREMENT,
};

// Function prototypes
void KNOB_Init();
void KNOB_Logical_Scan_Loop();

// External variables for accessing knob data in the main program
extern int32_t KNOB_Data;
extern bool KNOB_Trigger_Flag;
extern uint8_t KNOB_State_Flag;
extern size_t KNOB_CycleTime;  // Add this line

#endif // KNOB_H
