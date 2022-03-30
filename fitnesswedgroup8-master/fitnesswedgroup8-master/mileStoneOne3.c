/**********************************************************
 *
 * Milestone 1 ENCE361
 *
 * Code for milestone 1
 * Initializes and takes the initial x,y,z values and stores them
 * x,y,z are then stored in circular buffers and displayed to orbit LED panel
 * Button 1 (down) takes new inital x,y,z values for refrence
 * Button 2 Cycles through display types. Raw acc, Earth g's and acc in ms-2
 *
 *
 *    Grey H, Hamish A, Cameron B
 *    11 Mar 2020/
 *
 * Based of sample code provided by C Moore for ENCE361 labs 1/2/3
 *
 **********************************************************/
#include <stdio.h>
//#include "inc/hw_memmap.h"        // defines GPIO_PORTF_BASE
//#include "inc/tm4c123gh6pm.h"     // defines interrupt vectors

#include "stdint.h"
#include "stdbool.h"
#include <stdlib.h>
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "inc/hw_i2c.h"
#include "inc/hw_ints.h"
#include "driverlib/pin_map.h" //Needed for pin configure
#include "driverlib/systick.h"
#include "driverlib/sysctl.h"
#include "driverlib/gpio.h"
#include "driverlib/i2c.h"
#include "OrbitOLED/OrbitOLEDInterface.h"
#include "ustdlib.h"
#include "acc.h"
#include "i2c_driver.h"
#include "driverlib/adc.h"
#include "driverlib/pwm.h"
#include "driverlib/interrupt.h"
#include "driverlib/debug.h"
#include "circBufT.h"
#include "buttons4.h"


//#define DISPLAY_TIME_KEEPER 0
//#define BTTN2INTERUPT 0
//#define DISPLAY_SETTING 0 // 0 = raw, 1 = gravity, 2 = ms^-2,   ;Default to raw values
uint16_t display_setting = 0, display_time_keeper = 0, take_new_orientation_flag = 0;

static circBuf_t g_inBuffer_x;
static circBuf_t g_inBuffer_y;
static circBuf_t g_inBuffer_z;
//static uint32_t g_ulSampCnt;    // Counter for the interrupts


// struct to define values of x,y,z
typedef struct{
    int16_t x;
    int16_t y;
    int16_t z;
} vector3_t;


/**********************************************************
 * Constants
 **********************************************************/
// Systick configuration
#define SYSTICK_RATE_HZ 10
// Circular buffer length
#define BUF_SIZE 10
//Constants for display unit conversions
#define GRAV_CONSTANT 981
#define RAW_TO_ONE_G 260
#define RAW_TO_MS2 26

/*******************************************
 *      Local prototypes
 *******************************************/
void initClock (void);
void initDisplay (void);
void displayUpdate (char *str1, char *str2, int16_t num, uint8_t charLine);
void initAccl (void);
vector3_t getAcclData (void);

/***********************************************************
 * Interrupt handler functions
 ***********************************************************/
void ISR(void) // On button down being pushed set flag to prompt new orientation values to be taken
{
    take_new_orientation_flag = 1;
}

void ISR2(void) // On up button push change display settings : 0 = raw, 1 = gravity, 2 = ms^-2 :Default to raw values
{
   if (display_setting < 2) {
       display_setting++ ;
   } else {
       display_setting = 0;
   }
}

/***********************************************************
 * Initialisation functions: clock, SysTick, PWM, Store mean x,y,z
 ***********************************************************
 * Clock
 ***********************************************************/
void
initClock (void)
{
    // Set the clock rate to 20 MHz
    SysCtlClockSet (SYSCTL_SYSDIV_10 | SYSCTL_USE_PLL | SYSCTL_OSC_MAIN |
                   SYSCTL_XTAL_16MHZ);
}

/*********************************************************
 * initDisplay
 *********************************************************/
void
initDisplay (void)
{
    // Initialise the Orbit OLED display
    OLEDInitialise ();
}

// Takes a raw integer value from ADC and converts to ms^-2
//void
//rawToMS2(int16_t value, char* text_buffer, char* str1, char* str2)
//{
//    int16_t ms2_value = value / RAW_TO_MS2 * GRAV_CONSTANT;
//    int16_t first_num = ms2_value / 1000;
//    int16_t second_num = ms2_value % 1000;
//    //usnprintf(text_buffer, sizeof(text_buffer), "%s %s %2d . %2d", str1, str2, first_num, second_num);
//    usnprintf(text_buffer, sizeof(text_buffer), "%s %s %3d", str1, str2, value);
//
//}
//
//// Takes a raw integer value from ADC and converts to one earth gravity
//float
//rawToGravity (int16_t value, char* text_buffer)
//{
//    /*if (value > 200) {
//        return 1;
//    } else if (value < -200){
//        return -1;
//    } else {
//        return 0;
//    }*/
//    float float_value = value / RAW_TO_ONE_G;
//    return float_value;
//}

//*****************************************************************************
// Function to display a changing message on the display.
// The display has 4 rows of 16 characters, with 0, 0 at top left.
//*****************************************************************************
void
displayUpdate (char *str1, char *str2, int16_t num, uint8_t charLine)
{
    char text_buffer[17];           //Display fits 16 characters wide.

    // Form a new string for the line.  The maximum width specified for the
    //  number field ensures it is displayed right justified.
    if (display_setting == 0){
        //OLEDStringDraw ("                ", 0, charLine);
        usnprintf(text_buffer, sizeof(text_buffer), "%s %s %3d           ", str1, str2, num);
    } else if (display_setting == 1) {
        //float ms2 = rawToMS2(num);
        //rawToMS2(num, text_buffer, str1, str2);
        //usnprintf(text_buffer, sizeof(text_buffer), "%s %s %3d", str1, str2, num);
        int16_t ms2_value = num / RAW_TO_MS2 * GRAV_CONSTANT;
        int16_t first_num = ms2_value / 1000;
        int16_t second_num = ms2_value % 1000;
        if (ms2_value < 0) {            // dealing with negatives
            if (first_num > -1) {   // if LHS of decimal point is 0, need to give it a negative sign
                second_num = second_num * -1;
                usnprintf(text_buffer, sizeof(text_buffer), "%s %s -%2d.%2d", str1, str2, first_num, second_num);
            } else {
                second_num = second_num * -1;
                usnprintf(text_buffer, sizeof(text_buffer), "%s %s %3d.%2d", str1, str2, first_num, second_num);
            }
        } else {
            usnprintf(text_buffer, sizeof(text_buffer), "%s %s %3d.%2d", str1, str2, first_num, second_num);
        }
    } else {    // dealing with negatives
        //float gravity = rawToGravity(num);
        int16_t gravity = (num * 100) / RAW_TO_ONE_G;
        int16_t first_num = gravity / 100;
        int16_t second_num = gravity % 100;
        if (gravity < 0) {
            if (first_num > -1) {
                second_num = second_num * -1;
                usnprintf(text_buffer, sizeof(text_buffer), "%s %s -%2d.%2d", str1, str2, first_num, second_num);
            } else {
                second_num = second_num * -1;
                usnprintf(text_buffer, sizeof(text_buffer), "%s %s %3d.%2d", str1, str2, first_num, second_num);
            }
        } else {
            usnprintf(text_buffer, sizeof(text_buffer), "%s %s %3d.%2d", str1, str2, first_num, second_num);
        }
    }
    // "Undraw" the previous contents of the line to be updated.
    //OLEDStringDraw ("                ", 0, charLine);

    // Update line on display.
    OLEDStringDraw (text_buffer, 0, charLine);
}



// Displays the vector to the screen in format via displayUpdate function
void
displayGravity_values (vector3_t average_raw_vector3_t, vector3_t orientation_value)
{
    if (display_setting == 0) {                                                  //Changes to raw values
        displayUpdate ("Accl", "X", average_raw_vector3_t.x + orientation_value.x, 1);
        displayUpdate ("Accl", "Y", average_raw_vector3_t.y + orientation_value.y, 2);
        displayUpdate ("Accl", "Z", average_raw_vector3_t.z + orientation_value.z, 3);
    } else if (display_setting == 1){                                            //Change to multiples of gravity
        displayUpdate ("Grav", "X", (average_raw_vector3_t.x + orientation_value.x), 1);
        displayUpdate ("Grav", "Y", (average_raw_vector3_t.y + orientation_value.y), 2);
        displayUpdate ("Grav", "Z", (average_raw_vector3_t.z + orientation_value.z), 3);
    } else {                                                                     //change to ms^-2
        displayUpdate ("Ms-2", "X", (average_raw_vector3_t.x + orientation_value.x), 1);
        displayUpdate ("Ms-2", "Y", (average_raw_vector3_t.y + orientation_value.y), 2);
        displayUpdate ("Ms-2", "Z", (average_raw_vector3_t.z + orientation_value.z), 3);
    }
}

/*********************************************************
 * initAccl
 *********************************************************/
void
initAccl (void)
{
    char    toAccl[] = {0, 0};  // parameter, value

    /*
     * Enable I2C Peripheral
     */
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_I2C0);
    SysCtlPeripheralReset(SYSCTL_PERIPH_I2C0);

    /*
     * Set I2C GPIO pins
     */
    GPIOPinTypeI2C(I2CSDAPort, I2CSDA_PIN);
    GPIOPinTypeI2CSCL(I2CSCLPort, I2CSCL_PIN);
    GPIOPinConfigure(I2CSCL);
    GPIOPinConfigure(I2CSDA);

    /*
     * Setup I2C
     */
    I2CMasterInitExpClk(I2C0_BASE, SysCtlClockGet(), true);

    GPIOPinTypeGPIOInput(ACCL_INT2Port, ACCL_INT2);

    //Initialize ADXL345 Acceleromter

    // set +-2g, 10 bit resolution, active low interrupts
    toAccl[0] = ACCL_DATA_FORMAT;
    toAccl[1] = (ACCL_RANGE_2G | ACCL_FULL_RES);
    I2CGenTransmit(toAccl, 1, WRITE, ACCL_ADDR);

    toAccl[0] = ACCL_PWR_CTL;
    toAccl[1] = ACCL_MEASURE;
    I2CGenTransmit(toAccl, 1, WRITE, ACCL_ADDR);


    toAccl[0] = ACCL_BW_RATE;
    toAccl[1] = ACCL_RATE_100HZ;
    I2CGenTransmit(toAccl, 1, WRITE, ACCL_ADDR);

    toAccl[0] = ACCL_INT;
    toAccl[1] = 0x00;       // Disable interrupts from accelerometer.
    I2CGenTransmit(toAccl, 1, WRITE, ACCL_ADDR);

    toAccl[0] = ACCL_OFFSET_X;
    toAccl[1] = 0x00;
    I2CGenTransmit(toAccl, 1, WRITE, ACCL_ADDR);

    toAccl[0] = ACCL_OFFSET_Y;
    toAccl[1] = 0x00;
    I2CGenTransmit(toAccl, 1, WRITE, ACCL_ADDR);

    toAccl[0] = ACCL_OFFSET_Z;
    toAccl[1] = 0x00;
    I2CGenTransmit(toAccl, 1, WRITE, ACCL_ADDR);
}

/********************************************************
 * Function to read accelerometer
 ********************************************************/
vector3_t
getAcclData (void)
{
    char    fromAccl[] = {0, 0, 0, 0, 0, 0, 0}; // starting address, placeholder for data to be read.
    vector3_t acceleration;
    uint8_t bytesToRead = 6;

    fromAccl[0] = ACCL_DATA_X0;
    I2CGenTransmit(fromAccl, bytesToRead, READ, ACCL_ADDR);

    acceleration.x = (fromAccl[2] << 8) | fromAccl[1]; // Return 16-bit acceleration readings.
    acceleration.y = (fromAccl[4] << 8) | fromAccl[3];
    acceleration.z = (fromAccl[6] << 8) | fromAccl[5];

    return acceleration;
}

/********************************************************
 * Function to calculate mean values from accelerometer
 * Code adapted from lab code ADCdemo.c from ENCE361 LABS
 ********************************************************/
int
calculateMeanValues(circBuf_t g_inBuffer)
{
    int32_t sum;
    int i;
    sum = 0;
    // Background task: calculate the (approximate) mean of the values in the
    // circular buffer and updates variable
    for (i = 0; i < BUF_SIZE; i++)
        sum = sum + readCircBuf (&g_inBuffer);
    // Calculate and display the rounded mean of the buffer contents
    return ((2 * sum + BUF_SIZE) / 2 / BUF_SIZE);
    //return sum / BUF_SIZE;
}


//Stores means from circular buffers into a vector struct
vector3_t
store_values_vector3_t (circBuf_t g_inBuffer_x, circBuf_t g_inBuffer_y, circBuf_t g_inBuffer_z) {
    vector3_t mean_value;
    mean_value.x = calculateMeanValues(g_inBuffer_x); // calculate initial/orientation mean values for x and store in struct
    mean_value.y = calculateMeanValues(g_inBuffer_y);
    mean_value.z = calculateMeanValues(g_inBuffer_z);
    return mean_value;
}


/********************************************************
 * main
 ********************************************************/
int
main (void)
{
    uint8_t butState; // Defines button state for use in button polling
    vector3_t acceleration_raw;//input values pulled from accelerometer/ADC
    vector3_t average_raw_vector3_t;//Averaged original unit data
    vector3_t orientation_value;//current_mean_accel; holds reference orientation value

    initCircBuf (&g_inBuffer_x, BUF_SIZE);
    initCircBuf (&g_inBuffer_y, BUF_SIZE);
    initCircBuf (&g_inBuffer_z, BUF_SIZE);

    initClock ();
    initAccl ();
    initDisplay ();
    initButtons();

    OLEDStringDraw ("Accelerometer", 0, 0);

    int fill_buffer = 0;
    while (fill_buffer < BUF_SIZE) { //Initial start up loops that runs five times to fill buffer with values
        acceleration_raw = getAcclData();
        SysCtlDelay (SysCtlClockGet () / 600);    // Approx 20 Hz
        writeCircBuf(&g_inBuffer_x, acceleration_raw.x);
        writeCircBuf(&g_inBuffer_y, acceleration_raw.y);
        writeCircBuf(&g_inBuffer_z, acceleration_raw.z);
        fill_buffer += 1;
    }



    //Stores initial orientation values
    orientation_value = store_values_vector3_t(g_inBuffer_x, g_inBuffer_y, g_inBuffer_z);
    //take_new_orientation_flag = 1;



    while (1) //Main operation loop
    {
        acceleration_raw = getAcclData();
        SysCtlDelay (SysCtlClockGet () / 250);    // Approx 20 Hzs

        updateButtons();       // Poll the buttons

        butState = checkButton (DOWN); // Reads new orientation
        switch (butState)
        {
        case PUSHED:
            /*if (display_setting < 2) {
                display_setting +=1;
            } else {
                display_setting = 0;
            }*/
            ISR();//Reads new orientation
            break;
        case RELEASED:
            break; // Do nothing if state is NO_CHANGE
        }

        butState = checkButton (UP); //Changes display units
        switch (butState)
        {
        case PUSHED:
            //take_new_orientation_flag = 1;
            ISR2();//Cycles display units
            break;
        case RELEASED:
            break; // Do nothing if state is NO_CHANGE
        }


        // If button pressed take and store new orientation values from circular buffers
        if (take_new_orientation_flag) {
            orientation_value = store_values_vector3_t(g_inBuffer_x, g_inBuffer_y, g_inBuffer_z);
            take_new_orientation_flag = 0;
        }


        // Sets rate of display refresh at tenth of the clock rate
        if (display_time_keeper > 9) {
            average_raw_vector3_t = store_values_vector3_t(g_inBuffer_x, g_inBuffer_y, g_inBuffer_z); // Calculates the most recent mean values from circular buffers
            displayGravity_values(average_raw_vector3_t, orientation_value); // Displays the values to screen; Should happen at 2hz
            display_time_keeper = 0; // Resets time keeper to count to new refresh
        }
        display_time_keeper += 1;


        // Writes x,y,z to circular buffer ; Make function?
        writeCircBuf(&g_inBuffer_x, acceleration_raw.x);
        writeCircBuf(&g_inBuffer_y, acceleration_raw.y);
        writeCircBuf(&g_inBuffer_z, acceleration_raw.z);

    }
}
