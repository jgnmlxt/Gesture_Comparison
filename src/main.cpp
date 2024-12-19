// This project is done by Phakphum Artkaew, Tianyu Gao, and Luna Liu
// Please read README.md in root directory

#include "mbed.h"
#include "drivers/LCD_DISCO_F429ZI.h"

#define CTRL_REG1 0x20
#define CTRL_REG1_CONFIG 0b01'10'1'1'1'1
#define CTRL_REG4 0x23
#define CTRL_REG4_CONFIG 0b0'0'01'0'00'0
#define CTRL_REG3 0x22
#define CTRL_REG3_CONFIG 0b0'0'0'0'1'000
#define OUT_X_L 0x28
#define SPI_FLAG 1
#define DATA_READY_FLAG 2

#define SCALING_FACTOR (17.5f * 0.0174532925199432957692236907684886f / 1000.0f)
#define ALPHA 0.2f  // Smoothing factor: lower = more smoothing

EventFlags flags;

void spi_cb(int event) {
    flags.set(SPI_FLAG);
}

void data_cb() {
    flags.set(DATA_READY_FLAG);
}

uint16_t raw_gx, raw_gy, raw_gz;
float gx, gy, gz;

//LCD
LCD_DISCO_F429ZI lcd;

//Button
InterruptIn user_button(PA_0,PullDown);

SPI spi(PF_9, PF_8, PF_7, PC_1, use_gpio_ssel);
uint8_t write_buf[32], read_buf[32];

//Recording duration
static const int RECORD_DURATION=3000;
//Duration per sample
static const int SAMPLE_INTERVAL_MS=100;
//Maximum samples
static const int MAX_SAMPLES=RECORD_DURATION/SAMPLE_INTERVAL_MS;

float key_gesture_x[MAX_SAMPLES], key_gesture_y[MAX_SAMPLES];
float test_gesture_x[MAX_SAMPLES], test_gesture_y[MAX_SAMPLES];
float filtered_gx = 0.0f, filtered_gy = 0.0f, filtered_gz = 0.0f;

//Workflow states
enum State{
    WAIT_FOR_RECORD,
    RECORDING,
    WAIT_FOR_RECOGNIZE,
    RECOGNIZING,
    RESULT_DISPLAY
};

void init_gyro(){
    spi.format(8, 3);
    spi.frequency(1'000'000);

    write_buf[0] = CTRL_REG1; write_buf[1] = CTRL_REG1_CONFIG;
    spi.transfer(write_buf, 2, read_buf, 2, spi_cb);
    flags.wait_all(SPI_FLAG);

    write_buf[0] = CTRL_REG4; write_buf[1] = CTRL_REG4_CONFIG;
    spi.transfer(write_buf, 2, read_buf, 2, spi_cb);
    flags.wait_all(SPI_FLAG);

    write_buf[0] = CTRL_REG3; write_buf[1] = CTRL_REG3_CONFIG;
    spi.transfer(write_buf, 2, read_buf, 2, spi_cb);
    flags.wait_all(SPI_FLAG);
}
// Low-pass filter function
float low_pass_filter(float raw, float prev_filtered) {
    return ALPHA * raw + (1 - ALPHA) * prev_filtered;
}
void read_gyro(){
    write_buf[0] = OUT_X_L | 0x80 | 0x40;
    spi.transfer(write_buf, 7, read_buf, 7, spi_cb);
    flags.wait_all(SPI_FLAG);

    raw_gx = (read_buf[2] << 8) | read_buf[1];
    raw_gy = (read_buf[4] << 8) | read_buf[3];
    raw_gz = (read_buf[6] << 8) | read_buf[5];

    gx = raw_gx * SCALING_FACTOR;
    gy = raw_gy * SCALING_FACTOR;
    gz = raw_gz * SCALING_FACTOR;
    // Apply low-pass filter to smooth the data
    filtered_gx = low_pass_filter(gx, filtered_gx);
    filtered_gy = low_pass_filter(gy, filtered_gy);
    filtered_gz = low_pass_filter(gz, filtered_gz);

    gx = filtered_gx;
    gy = filtered_gy;
    gz = filtered_gz;
}

//Display a message
void display_message(const char* msg, uint32_t color){
    lcd.Clear(LCD_COLOR_BLACK);
    lcd.SetBackColor(LCD_COLOR_BLACK);
    lcd.SetTextColor(color);
    lcd.SetFont(&Font16);
    lcd.DisplayStringAt(0, LINE(5), (uint8_t*)msg, CENTER_MODE);
}
void DrawChristmasTree() {
    // Clear the screen
    lcd.Clear(LCD_COLOR_BLACK);

    // Tree parameters
    uint16_t trunkWidth = 20;
    uint16_t trunkHeight = 40;
    uint16_t trunkX = lcd.GetXSize() / 2 - trunkWidth / 2;
    uint16_t trunkY = lcd.GetYSize() - trunkHeight - 60;  // Adjusted for text space

    uint16_t treeBaseWidth = 120;
    uint16_t treeHeight = 150;
    uint16_t treeX = lcd.GetXSize() / 2;
    uint16_t treeY = trunkY - treeHeight;

    // Draw the trunk
    lcd.SetTextColor(LCD_COLOR_BROWN);
    lcd.FillRect(trunkX, trunkY, trunkWidth, trunkHeight);

    // Draw the tree (triangle)
    lcd.SetTextColor(LCD_COLOR_GREEN);
    for (int h = 0; h <= treeHeight; h++) {
        int width = (treeBaseWidth * h) / treeHeight;
        lcd.DrawHLine(treeX - width / 2, treeY + h, width);
    }

    // Draw the star on top of the tree
    uint16_t starX = treeX;
    uint16_t starY = treeY - 10;
    lcd.SetTextColor(LCD_COLOR_YELLOW);
    lcd.FillCircle(starX, starY, 8);

    // Display "Success" message below the tree
    lcd.SetTextColor(LCD_COLOR_WHITE);
    lcd.SetBackColor(LCD_COLOR_BLACK);
    lcd.DisplayStringAt(0, lcd.GetYSize() - 40, (uint8_t *)"SUCCESS!", CENTER_MODE);
}
//DTW function
//We only detect movements along x-y plane
float dtw_distance(const float* seq1x, const float* seq1y, int len1, const float* seq2x, const float* seq2y, int len2){
    float* dtw= new float[(len1+1)*(len2+1)];
    for (int i=0; i<=len1; i++){
        for(int j=0; j<=len2; j++){
            dtw[i*(len2+1)+j]= INFINITY;
        }
    }
    dtw[0*(len2+1)+0]= 0;

    //Calculating the costs
    for (int i=1; i<=len1; i++){
        for (int j=1; j<=len2; j++){
            float dist= sqrtf((seq1x[i-1]-seq2x[j-1])*(seq1x[i-1]-seq2x[j-1])+(seq1y[i-1]-seq2y[j-1])*(seq1y[i-1]-seq2y[j-1]));
            float cost= dist+fminf(fminf(dtw[(i-1)*(len2+1)+j],dtw[i*(len2+1)+j-1]),dtw[(i-1)*(len2+1)+(j-1)]);
            dtw[i*(len2+1)+j]=cost;
        }
    }

    float final_dist= dtw[len1*(len2+1)+len2];
    delete[] dtw;
    return final_dist;
}

//Cross correlation
float cross_correlation(const float* seq1, const float* seq2, int length){
    //Mean
    float mean1=0, mean2=0;
    for (int i=0; i<length; i++){
        mean1+= seq1[i];
        mean2+= seq2[i];
    }
    mean1/=length;
    mean2/=length;

    //MSE
    float numerator= 0.0f, denom1= 0.0f, denom2= 0.0f;
    for (int i=0; i<length; i++){
        float a= seq1[i]-mean1;
        float b= seq2[i]-mean2;
        numerator+=a*b;
        denom1+=a*a;
        denom2+=b*b;
    }

    float denom= sqrtf(denom1*denom2);
    if (denom<1e-9) return 0.0f;
    return numerator/denom;
}

int main() {
    lcd.Clear(LCD_COLOR_BLACK);
    lcd.SetBackColor(LCD_COLOR_BLACK);
    lcd.SetTextColor(LCD_COLOR_WHITE);

    InterruptIn int2(PA_2, PullDown);
    int2.rise(&data_cb);

    init_gyro();
    //Default screen
    State state=WAIT_FOR_RECORD;
    display_message("Push to Record", LCD_COLOR_WHITE);

    Timer timer;

    while (true){
        switch (state){
            case WAIT_FOR_RECORD:
                if (user_button.read()==1){
                    //Recording the key
                    display_message("Recording", LCD_COLOR_YELLOW);
                    timer.reset();
                    timer.start();
                    int sampleCount= 0;

                    while (timer.read_ms()< RECORD_DURATION && sampleCount< MAX_SAMPLES){
                        flags.wait_all(DATA_READY_FLAG, 100, true); 
                        read_gyro();
                        //Store
                        key_gesture_x[sampleCount]= gx;
                        key_gesture_y[sampleCount]= gy;
                        sampleCount++;
                        thread_sleep_for(SAMPLE_INTERVAL_MS);
                    }

                    timer.stop();
                    //Save ready
                    display_message("Push to Recognize", LCD_COLOR_WHITE);
                    state=WAIT_FOR_RECOGNIZE;
                }
                break;

            case WAIT_FOR_RECOGNIZE:
                if (user_button.read()== 1) {
                    //Start record password
                    display_message("Recognizing", LCD_COLOR_YELLOW);
                    timer.reset();
                    timer.start();
                    int sampleCount=0;

                    while (timer.read_ms() < RECORD_DURATION && sampleCount<MAX_SAMPLES){
                        flags.wait_all(DATA_READY_FLAG, 100, true);
                        read_gyro();
                        test_gesture_x[sampleCount]= gx;
                        test_gesture_y[sampleCount]= gy;
                        sampleCount++;
                        thread_sleep_for(SAMPLE_INTERVAL_MS);
                    }
                    
                    timer.stop();

                    //Compares
                    float distance= dtw_distance(key_gesture_x, key_gesture_y, MAX_SAMPLES, test_gesture_x, test_gesture_y, MAX_SAMPLES);
                    float corr_x= cross_correlation(key_gesture_x, test_gesture_x, MAX_SAMPLES);
                    float corr_y= cross_correlation(key_gesture_y, test_gesture_y, MAX_SAMPLES);

                    //Sensitivities
                    bool success= (distance < 300.0f) && (corr_x > 0.1f) && (corr_y > 0.1f);
                    
                    //Display DTW and correlations
                    char buffer[15];
                    sprintf(buffer, "DTW: %.2f", distance);
                    display_message(buffer, LCD_COLOR_CYAN);
                    thread_sleep_for(1000);
                    
                    sprintf(buffer, "Corr X: %.2f", corr_x);
                    display_message(buffer, LCD_COLOR_CYAN);
                    thread_sleep_for(1000);
                    
                    sprintf(buffer, "Corr Y: %.2f", corr_y);
                    display_message(buffer, LCD_COLOR_CYAN);
                    thread_sleep_for(1000);

                    if (success) {
                        DrawChristmasTree();
                        thread_sleep_for(3000);
                        display_message("Push to Recognize", LCD_COLOR_WHITE);
                        state=WAIT_FOR_RECOGNIZE;
                    } else {
                        display_message("Failed",LCD_COLOR_RED);
                        thread_sleep_for(3000);
                        display_message("Push to Recognize",LCD_COLOR_WHITE);
                        state=WAIT_FOR_RECOGNIZE;
                    }
                }
                break;

            default:
                break;
        }
        thread_sleep_for(50);
    }
}
