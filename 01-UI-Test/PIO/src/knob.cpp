// knob.cpp
#include "knob.h"
#include "pin_config.h"

// Initialize global variables
int32_t KNOB_Data = 0;
bool KNOB_Trigger_Flag = false;
uint8_t KNOB_State_Flag = KNOB_State::KNOB_NULL;
uint8_t KNOB_Previous_Logical = 0B00000000;
size_t KNOB_CycleTime = 0;

// Initialize the knob (called in setup)
void KNOB_Init()
{
    pinMode(KNOB_DATA_A, INPUT_PULLUP);
    pinMode(KNOB_DATA_B, INPUT_PULLUP);
}

// Scan the knob's current state
void KNOB_Logical_Scan_Loop()
{
    uint8_t KNOB_Logical_Scan = 0B00000000;

    if (digitalRead(KNOB_DATA_A) == 1)
    {
        KNOB_Logical_Scan |= 0B00000010;
    }
    else
    {
        KNOB_Logical_Scan &= 0B11111101;
    }

    if (digitalRead(KNOB_DATA_B) == 1)
    {
        KNOB_Logical_Scan |= 0B00000001;
    }
    else
    {
        KNOB_Logical_Scan &= 0B11111110;
    }

    if (KNOB_Previous_Logical != KNOB_Logical_Scan)
    {
        if (KNOB_Logical_Scan == 0B00000000 || KNOB_Logical_Scan == 0B00000011)
        {
            KNOB_Previous_Logical = KNOB_Logical_Scan;
            KNOB_Trigger_Flag = true;
        }
        else
        {
            if (KNOB_Logical_Scan == 0B00000010)
            {
                switch (KNOB_Previous_Logical)
                {
                case 0B00000000:
                    KNOB_State_Flag = KNOB_State::KNOB_INCREMENT;
                    break;
                case 0B00000011:
                    KNOB_State_Flag = KNOB_State::KNOB_DECREMENT;
                    break;
                default:
                    break;
                }
            }
            if (KNOB_Logical_Scan == 0B00000001)
            {
                switch (KNOB_Previous_Logical)
                {
                case 0B00000000:
                    KNOB_State_Flag = KNOB_State::KNOB_DECREMENT;
                    break;
                case 0B00000011:
                    KNOB_State_Flag = KNOB_State::KNOB_INCREMENT;
                    break;
                default:
                    break;
                }
            }
        }
    }
}
