#include <Arduino.h>
#include <lvgl.h>
#include <ui.h>
#include "Arduino_GFX_Library.h"
#include "TouchDrvCHSC5816.hpp"
#include "pin_config.h"
#include "knob.h"

/*---------------------------------------------------------------------------------------------------------------------*/
// Display Settings
static lv_disp_draw_buf_t draw_buf;
static lv_color_t *disp_draw_buf;
static lv_disp_drv_t disp_drv;

// Display-specific configuration
Arduino_DataBus *bus = new Arduino_ESP32QSPI(LCD_CS, LCD_SCLK, LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3);
Arduino_GFX *gfx = new Arduino_SH8601(bus, LCD_RST, 0, false, LCD_WIDTH, LCD_HEIGHT);

// Touch settings
TouchDrvCHSC5816 touch;
TouchDrvInterface *pTouch;

// Initialization for CHSC5816 Touch Driver
void CHSC5816_Initialization() {
    touch.setPins(TOUCH_RST, TOUCH_INT);
    if (!touch.begin(Wire, CHSC5816_SLAVE_ADDRESS, IIC_SDA, IIC_SCL)) {
        Serial.println("Failed to find CHSC5816 - check your wiring!");
        while (1) delay(1000);
    }
    Serial.println("Touch device initialized successfully!");
}

// Display flushing
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;

#if (LV_COLOR_16_SWAP != 0)
    gfx->draw16bitBeRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
#else
    gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
#endif
    lv_disp_flush_ready(disp);
}

// Read the touchpad
void my_touchpad_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data) {
    int16_t Touch_x[2], Touch_y[2];
    uint8_t touchpad = touch.getPoint(Touch_x, Touch_y);

    data->state = touchpad > 0 ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
    data->point.x = Touch_x[0];
    data->point.y = Touch_y[0];
}

// LVGL initialization
void lvgl_initialization() {
    lv_init();

    // Set display dimensions
    uint16_t lcdWidth = gfx->width();
    uint16_t lcdHeight = gfx->height();

    // Allocate draw buffer
    disp_draw_buf = (lv_color_t *)malloc(sizeof(lv_color_t) * lcdWidth * 40);
    if (!disp_draw_buf) {
        Serial.println("LVGL draw buffer allocation failed!");
        return;
    }

    lv_disp_draw_buf_init(&draw_buf, disp_draw_buf, NULL, lcdWidth * 40);

    // Initialize display driver
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = lcdWidth;
    disp_drv.ver_res = lcdHeight;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    // Initialize input (touch) driver
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);
}

/*---------------------------------------------------------------------------------------------------------------------*/

// Functions prototype 
void set_hand_positions();
void animate_clock_hands_on_load();
void update_clock_hands();
void initializeWatchScreen();

// Constants and Variables
#define TOTAL_MENU_ICONS 6
int menuIndex = 0;
int currentIconIndex = 0;
bool previousKnobKeyState = LOW;
unsigned long lastKnobKeyPressTime = 0;
const unsigned long debounceDelay = 200;

#define MAX_BRIGHTNESS 255
#define MIN_BRIGHTNESS 0
int brightnessLevel = 255;  // Start with a medium brightness level

// Enum to define available screens
enum Screen {
    WATCH,
    MENU,
    BRIGHTNESS,
    WEATHER 
};

// Current Screen
Screen currentScreen = WATCH;

// Screen Management Functions
void showWatchScreen() {
    lv_scr_load(ui_screen_watch);
    initializeWatchScreen();
}

void showMenuScreen() { lv_scr_load(ui_screen_menu); }
void showBrightnessScreen() { lv_scr_load(ui_screen_brightness); }
void showWeatherScreen() { lv_scr_load(ui_screen_weather); }

void switchScreen(Screen screen) {
    currentScreen = screen;
    switch (currentScreen) {
        case WATCH: showWatchScreen(); break;
        case MENU: showMenuScreen(); break;
        case BRIGHTNESS: showBrightnessScreen(); break;
        case WEATHER: showWeatherScreen(); break;
    }
}

// Knob Press Detection with Debounce
bool isKnobKeyPressed() {
    bool isPressed = (digitalRead(KNOB_KEY) == LOW);
    unsigned long currentMillis = millis();

    // Only register the press if debounce time has passed
    if (isPressed && (currentMillis - lastKnobKeyPressTime >= debounceDelay)) {
        lastKnobKeyPressTime = currentMillis;  // Update last press time
        return true;
    }
    return false;
}

void playBuzzer() { 
        tone(BUZZER_DATA, 1000, 100);  // 1000 Hz for 100 ms
}

void updateIconFocus(int newIndex) {
    lv_obj_t* icons[] = {ui_Icon_1, ui_Icon_2, ui_Icon_3, ui_Icon_4, ui_Icon_5, ui_Icon_6};

    if (newIndex < 0 || newIndex >= TOTAL_MENU_ICONS) return;

    // Remove scaling from the previous icon without using focus states
    lv_img_set_zoom(icons[menuIndex], 256); // Reset the zoom to 256

    // Update the menu index to the new focused icon
    menuIndex = newIndex;

    // Apply scaling to the new focused icon without adding focus state
    lv_img_set_zoom(icons[menuIndex], 350); // Scale up

    // Uncomment these lines only if the scaling works, as they may cause conflicts
    lv_obj_invalidate(icons[menuIndex]); 
    lv_obj_scroll_to_view(icons[menuIndex], LV_ANIM_ON);
}

void handle_knob_for_menu() {
    if (KNOB_Trigger_Flag) {
        KNOB_Trigger_Flag = false;  // Reset flag

        if (KNOB_State_Flag == KNOB_State::KNOB_INCREMENT && menuIndex < TOTAL_MENU_ICONS - 1) {
            updateIconFocus(menuIndex + 1);
            Serial.printf("Menu Index (Right): %d\n", menuIndex);
        } else if (KNOB_State_Flag == KNOB_State::KNOB_DECREMENT && menuIndex > 0) {
            updateIconFocus(menuIndex - 1);
            Serial.printf("Menu Index (Left): %d\n", menuIndex);
        }
    }
}


// Separate function to handle knob key press actions within the MENU screen
void handle_knob_keypress_in_menu() {
    // Ensure function only runs in MENU screen
    if (currentScreen != MENU) return;

    // Feedback for debugging
    Serial.printf("Knob key action on MENU at index: %d\n", menuIndex);

    // Perform actions based on selected icon
    switch (menuIndex) {
        case 0:  // Icon 1 - Switch to BRIGHTNESS
            switchScreen(BRIGHTNESS);
            Serial.println("Switched to BRIGHTNESS screen from MENU screen");
            break;

        case 1:  // Icon 2 - Switch to WATCH
            switchScreen(WATCH);
            Serial.println("Switched to WATCH screen from MENU screen");
            break;

        case 4:  // Icon 5 - Switch to WEATHER
            switchScreen(WEATHER);
            Serial.println("Switched to WEATHER screen from MENU screen");
            break;

        default:
            // No action for other icons yet
            Serial.println("No action assigned for this menu icon");
            break;
    }
}

void handle_knob_for_brightness() {
    if (KNOB_Trigger_Flag) {
        KNOB_Trigger_Flag = false;  // Reset flag after handling

        if (KNOB_State_Flag == KNOB_State::KNOB_INCREMENT && brightnessLevel < MAX_BRIGHTNESS) {
            brightnessLevel += 5;  // Increase brightness in steps of 5
            if (brightnessLevel > MAX_BRIGHTNESS) brightnessLevel = MAX_BRIGHTNESS;  // Ensure max cap

        } else if (KNOB_State_Flag == KNOB_State::KNOB_DECREMENT && brightnessLevel > MIN_BRIGHTNESS) {
            brightnessLevel -= 5;  // Decrease brightness in steps of 5
            if (brightnessLevel < MIN_BRIGHTNESS) brightnessLevel = MIN_BRIGHTNESS;  // Ensure min cap
        }

        // Map brightness level from 0-255 to 0-100 for the UI arc
        int arcValue = map(brightnessLevel, MIN_BRIGHTNESS, MAX_BRIGHTNESS, 0, 100);
        
        // Update ui_Arc_brightness and display brightness
        lv_arc_set_value(ui_Arc_brightness, arcValue);  // Update LVGL arc
        gfx->Display_Brightness(brightnessLevel);       // Adjust hardware brightness
        
        // Update the label to show the current brightness percentage
        static char labelBuffer[4];
        snprintf(labelBuffer, sizeof(labelBuffer), "%d", arcValue);
        lv_label_set_text(ui_Label_Brightness, labelBuffer);  // Update the label text

        Serial.printf("Brightness Level: %d (Arc: %d%%)\n", brightnessLevel, arcValue);
    }
}

// Setup and Main Loop
void setup() {
    Serial.begin(115200);
    Serial.println("Ciallo");

    pinMode(LCD_VCI_EN, OUTPUT);
    digitalWrite(LCD_VCI_EN, HIGH);

    pinMode(KNOB_KEY, INPUT_PULLUP);
    pinMode(BUZZER_DATA, OUTPUT);

    CHSC5816_Initialization();

    gfx->begin(40000000);
    gfx->fillScreen(BLACK);

    for (int i = 0; i <= 255; i++) {
        gfx->Display_Brightness(i);
        delay(3);
    }

    lvgl_initialization();
    ui_init();
    KNOB_Init();

    switchScreen(WATCH);
}

void loop() {
    lv_timer_handler();  // LVGL handler
    delay(5);

    KNOB_Logical_Scan_Loop();

    // Handle knob rotation in MENU screen
    if (currentScreen == MENU) {
        handle_knob_for_menu();
    }

    // Handle knob rotation in BRIGHTNESS screen
    if (currentScreen == BRIGHTNESS) {
        handle_knob_for_brightness();
    }

    // Handle knob key press action for each screen
    if (isKnobKeyPressed()) {
        playBuzzer();  // Feedback sound for key press
        Serial.printf("Knob key pressed on screen: %d\n", currentScreen);

        switch (currentScreen) {
            case WATCH:
                switchScreen(MENU);
                Serial.println("Switched to MENU screen from WATCH screen");
                break;

            case MENU:
                handle_knob_keypress_in_menu();
                break;

            case WEATHER:
                switchScreen(MENU);
                Serial.println("Switched to MENU screen from WEATHER screen");
                break;

            case BRIGHTNESS:
                switchScreen(MENU);
                Serial.println("Switched to MENU screen from BRIGHTNESS screen");
                break;

            default:
                break;
        }
    }
}


// Mock time variables (initialize to 10:15:30 for example)
int mock_hour = 10;
int mock_minute = 15;
int mock_second = 30;
bool is_first_load = true; // Flag to track if the animation should play
lv_timer_t* update_timer = NULL; // Static timer for updating hands

// Function to set the initial position of clock hands based on mock time
void set_hand_positions() {
    int hour_angle = (mock_hour % 12) * 30 + (mock_minute / 2);  // Hour hand angle (0-360)
    int minute_angle = mock_minute * 6;                          // Minute hand angle (0-360)
    int second_angle = mock_second * 6;                          // Second hand angle (0-360)

    // Set the angles of each hand without animation
    lv_img_set_angle(ui_hour_hand, hour_angle * 10);   // LVGL expects angles in 0.1 degrees
    lv_img_set_angle(ui_minute_hand, minute_angle * 10);
    lv_img_set_angle(ui_second_hand, second_angle * 10);
}

// Function to animate clock hands to the initial positions on first screen load
void animate_clock_hands_on_load() {
    if (!is_first_load) return; // Skip animation if not the first load

    int hour_angle = (mock_hour % 12) * 30 + (mock_minute / 2);
    int minute_angle = mock_minute * 6;
    int second_angle = mock_second * 6;

    // Animate hour hand with a delay and smooth easing
    lv_anim_t anim_hour;
    lv_anim_init(&anim_hour);
    lv_anim_set_var(&anim_hour, ui_hour_hand);
    lv_anim_set_values(&anim_hour, 0, hour_angle * 10);  // Convert to 0.1 degrees for LVGL
    lv_anim_set_time(&anim_hour, 1000);  // 1-second animation duration
    lv_anim_set_delay(&anim_hour, 200);
    lv_anim_set_path_cb(&anim_hour, lv_anim_path_ease_out);  // Smooth easing function
    lv_anim_set_exec_cb(&anim_hour, (lv_anim_exec_xcb_t)lv_img_set_angle);
    lv_anim_start(&anim_hour);

    // Animate minute hand with a delay and smooth easing
    lv_anim_t anim_minute;
    lv_anim_init(&anim_minute);
    lv_anim_set_var(&anim_minute, ui_minute_hand);
    lv_anim_set_values(&anim_minute, 0, minute_angle * 10);
    lv_anim_set_time(&anim_minute, 1000);
    lv_anim_set_delay(&anim_minute, 100);
    lv_anim_set_path_cb(&anim_minute, lv_anim_path_ease_out);  // Smooth easing function
    lv_anim_set_exec_cb(&anim_minute, (lv_anim_exec_xcb_t)lv_img_set_angle);
    lv_anim_start(&anim_minute);

    // Animate second hand with no delay and smooth easing
    lv_anim_t anim_second;
    lv_anim_init(&anim_second);
    lv_anim_set_var(&anim_second, ui_second_hand);
    lv_anim_set_values(&anim_second, 0, second_angle * 10);
    lv_anim_set_time(&anim_second, 1000);
    lv_anim_set_delay(&anim_second, 0);
    lv_anim_set_path_cb(&anim_second, lv_anim_path_ease_out);  // Smooth easing function
    lv_anim_set_exec_cb(&anim_second, (lv_anim_exec_xcb_t)lv_img_set_angle);
    lv_anim_start(&anim_second);

    is_first_load = false; // Mark that the initial animation has been played
}

// Function to update the clock hands every second in real-time
void update_clock_hands() {
    mock_second++;
    if (mock_second >= 60) {
        mock_second = 0;
        mock_minute++;
        if (mock_minute >= 60) {
            mock_minute = 0;
            mock_hour = (mock_hour + 1) % 12;
        }
    }

    int hour_angle = (mock_hour % 12) * 30 + (mock_minute / 2);
    int minute_angle = mock_minute * 6;
    int second_angle = mock_second * 6;

    lv_img_set_angle(ui_hour_hand, hour_angle * 10);
    lv_img_set_angle(ui_minute_hand, minute_angle * 10);
    lv_img_set_angle(ui_second_hand, second_angle * 10);
}

// Main function to initialize the WATCH screen, set hand positions, and set up animations
void initializeWatchScreen() {
    set_hand_positions(); // Ensure initial positions are set without animation
    if (is_first_load) {
        // Animate the first time the screen is loaded
        animate_clock_hands_on_load();

        // Create the timer to update the clock hands, initially inactive
        update_timer = lv_timer_create([](lv_timer_t* timer) {
            update_clock_hands();
        }, 1000, NULL);  // 1000 ms = 1 second
        lv_timer_pause(update_timer); // Pause the timer initially

        // Schedule the timer to start after the initial animation delay
        lv_timer_t* start_timer = lv_timer_create([](lv_timer_t* timer) {
            lv_timer_resume(update_timer); // Start the clock hand update timer
            lv_timer_del(timer); // Delete the start timer
        }, 1200, NULL); // Delay of 1200 ms to allow animation to complete
    } else {
        // Set positions directly for subsequent loads
        set_hand_positions();
    }
}
