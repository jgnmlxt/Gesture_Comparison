#include "mbed.h"
#include "drivers/LCD_DISCO_F429ZI.h"
#include <cmath>

//Gyroscope registers
#define CTRL_REG1 0x20
#define CTRL_REG1_CONFIG 0b01101111

#define CTRL_REG4 0x23
#define CTRL_REG4_CONFIG 0b00010000

#define CTRL_REG3 0x22
#define CTRL_REG3_CONFIG 0b00001000

#define OUT_X_L 0x28

// Scaling Factor for data conversion dps --> rps
#define SCALING_FACTOR (17.5f*(M_PI/180.0f)/1000.0f)

//LCD
LCD_DISCO_F429ZI lcd;

// EventFlags Object Declaration
EventFlags flags;

// Callback function for SPI Transfer Completion
void spi_cb(int event) {
    flags.set(1);
}

// Callback function for Data Ready Interrupt
void data_cb() {
    flags.set(2);
}

uint16_t raw_gx, raw_gy, raw_gz;
float gx, gy, gz;

//Blue button
#define BLUE_BUTTON_PIN PA_0
InterruptIn blue_button(BLUE_BUTTON_PIN);
Timer timer,circular_timer;

//Recording state flag
volatile bool is_recording=false;

//Accumulated duration of circular movements
float accumulated_duration=0.0f;



//Display "Press Button"
void display_press_button(){
    lcd.Clear(LCD_COLOR_BLACK);
    lcd.SetBackColor(LCD_COLOR_BLACK);
    lcd.SetTextColor(LCD_COLOR_WHITE);
    lcd.DisplayStringAt(0,LINE(5),(uint8_t*)"Press Button",CENTER_MODE);
}

// Display "Recording"
void display_recording(){
    lcd.Clear(LCD_COLOR_BLACK);
    lcd.SetBackColor(LCD_COLOR_BLACK);
    lcd.SetTextColor(LCD_COLOR_YELLOW);
    lcd.DisplayStringAt(0,LINE(5),(uint8_t*)"Recording",CENTER_MODE);
}

// Display "Unlocked"
void display_unlocked() {
    lcd.Clear(LCD_COLOR_BLACK);
    lcd.SetBackColor(LCD_COLOR_BLACK);
    lcd.SetTextColor(LCD_COLOR_GREEN);
    lcd.DisplayStringAt(0,LINE(5),(uint8_t*)"Unlocked",CENTER_MODE);
}

// Display "Unlock Failed"
void display_unlock_failed(){
    lcd.Clear(LCD_COLOR_BLACK);
    lcd.SetBackColor(LCD_COLOR_BLACK);
    lcd.SetTextColor(LCD_COLOR_RED);
    lcd.DisplayStringAt(0,LINE(5),(uint8_t*)"Unlock Failed",CENTER_MODE);
}

//When button is pressed
void on_button_press(){
    if (!is_recording){
        //Start recording
        display_recording();
        timer.reset();
        timer.start();
        circular_timer.reset();
        circular_timer.start();
        accumulated_duration=0.0f;
        is_recording=true;
    }
}

int main(){
    // SPI
    SPI spi(PF_9, PF_8, PF_7, PC_1, use_gpio_ssel); // MOSI, MISO, SCLK, CS
    uint8_t write_buf[32], read_buf[32];

    // Interrupt
    InterruptIn int2(PA_2, PullDown);
    int2.rise(&data_cb);

    // SPI Data transmission format and frequency
    spi.format(8, 3);
    spi.frequency(1'000'000);

    //Gyroscope initialization
    write_buf[0] = CTRL_REG1;
    write_buf[1] = CTRL_REG1_CONFIG;
    spi.transfer(write_buf, 2, read_buf, 2, spi_cb);
    flags.wait_all(1);

    write_buf[0] = CTRL_REG4;
    write_buf[1] = CTRL_REG4_CONFIG;
    spi.transfer(write_buf, 2, read_buf, 2, spi_cb);
    flags.wait_all(1);

    write_buf[0] = CTRL_REG3;
    write_buf[1] = CTRL_REG3_CONFIG;
    spi.transfer(write_buf, 2, read_buf, 2, spi_cb);
    flags.wait_all(1);

    //Start by displaying "Press Button"
    display_press_button();

    blue_button.fall(&on_button_press);

    //Time when start recording
    float last_time=0.0f;

    while (1){
        if (is_recording){
            flags.wait_all(2, 0xFF, true);

            // Read GYRO Data using SPI transfer --> 6 bytes!
            write_buf[0] = OUT_X_L | 0x80 | 0x40;
            spi.transfer(write_buf, 7, read_buf, 7, spi_cb);
            flags.wait_all(1);

            // Extract raw 16-bit gyroscope data for X, Y
            raw_gx = (read_buf[2] << 8) | read_buf[1];
            raw_gy = (read_buf[4] << 8) | read_buf[3];

            // Convert raw data to radians per second
            gx = raw_gx * SCALING_FACTOR;
            gy = raw_gy * SCALING_FACTOR;

            //Check for circular motion in x-y plane
            float magnitude_xy=sqrt(gx*gx+gy*gy);
            //Set the threshold
            bool circular_motion=magnitude_xy>0.65f;

            //Update accumulated duration
            float current_time=circular_timer.read();
            if (circular_motion){
                accumulated_duration+=current_time-last_time;
            }
            last_time=current_time;

            // Check recording timeout
            if (timer.read()>=5.0f){
                timer.stop();
                circular_timer.stop();

                if (accumulated_duration>=2.1f){
                    display_unlocked();
                }
                else{
                    display_unlock_failed();
                }

                thread_sleep_for(5000);
                is_recording=false;
                display_press_button();
            }
        }
    }
}
