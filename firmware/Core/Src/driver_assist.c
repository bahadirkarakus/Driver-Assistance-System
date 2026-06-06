/**
 * driver_assist.c
 * Driver Assistance -- Obstacle Warning & Brake Alert System
 * Target  : STM32 Nucleo-F411RE
 * Authors : Fatih Bahadir Karakus, Furkan Duksal
 * Course  : EE325 Embedded Systems
 *
 * Pinout:
 *   Speed potentiometer    -> PA0  (ADC1 Channel 0)
 *   Distance potentiometer -> PA1  (ADC1 Channel 1)
 *   LED1 WARNING           -> PC0  (GPIO Output, A5)
 *   LED2 BRAKE             -> PC1  (GPIO Output, A4)
 *   LED3 FAULT             -> PB0  (GPIO Output, A3)
 */
#include "main.h"
#include "driver_assist.h"
#include <stdint.h>
#include <math.h>

#define ADC_MAX          4095.0f
#define SPEED_MAX_KMH    130.0f
#define DISTANCE_MAX_M   200.0f
#define DISTANCE_MIN_M   0.5f
#define REACTION_TIME_S  1.0f
#define MU_FRICTION      0.7f
#define GRAVITY          9.81f
#define WARN_FACTOR_CITY 1.4f
#define WARN_FACTOR_HWY  1.7f
#define CITY_SPEED_KMH   60.0f
#define BLINK_PERIOD_MS  300

extern ADC_HandleTypeDef hadc1;
static uint32_t last_blink_tick = 0;

/* ---- ADC helper ---- */
static uint32_t read_adc_channel(uint32_t channel)
{
    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel      = channel;
    sConfig.Rank         = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_84CYCLES;
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);
    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, HAL_MAX_DELAY);
    uint32_t val = HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);
    return val;
}

/* ---- Physics ---- */
static float compute_stop_distance(float speed_kmh)
{
    float v_ms    = speed_kmh / 3.6f;
    float d_react = v_ms * REACTION_TIME_S;
    float d_brake = (v_ms * v_ms) / (2.0f * MU_FRICTION * GRAVITY);
    return d_react + d_brake;
}

/* ---- LED helpers ---- */
static void led_warning_blink_nb(uint32_t now_ms)
{
    if ((now_ms - last_blink_tick) >= BLINK_PERIOD_MS) {
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_0);
        last_blink_tick = now_ms;
    }
}

static void led_brake_on(void)
{
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_1, GPIO_PIN_SET);
}

static void led_all_off(void)
{
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_1, GPIO_PIN_RESET);
}

/* FAULT LED moved from PC2 (morpho) to PB0 = Arduino header A3 */
static void led_fault_on(void)
{
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);
}

static void led_fault_off(void)
{
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
}

/* ---- Mode selection ---- */
typedef enum { MODE_CITY = 0, MODE_HIGHWAY } DriveMode;

static DriveMode get_drive_mode(float speed_kmh)
{
    return (speed_kmh <= CITY_SPEED_KMH) ? MODE_CITY : MODE_HIGHWAY;
}

/* ---- Main application loop ---- */
void driver_assist_run(void)
{
    float speed_kmh, distance_m, d_stop, d_warn, d_brake, warn_factor;
    uint32_t adc_raw_speed, adc_raw_dist, now_ms;
    DriveMode mode;

    /* Configure PB0 (Arduino header A3) as output for the FAULT LED.
       (Relocated from PC2 on the morpho header to the easy Arduino row.) */
    __HAL_RCC_GPIOB_CLK_ENABLE();
    {
        GPIO_InitTypeDef gpio = {0};
        gpio.Pin   = GPIO_PIN_0;
        gpio.Mode  = GPIO_MODE_OUTPUT_PP;
        gpio.Pull  = GPIO_NOPULL;
        gpio.Speed = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(GPIOB, &gpio);
    }

    while (1)
    {
        adc_raw_speed = read_adc_channel(ADC_CHANNEL_0);   /* PA0 */
        adc_raw_dist  = read_adc_channel(ADC_CHANNEL_1);   /* PA1 */

        speed_kmh  = ((float)adc_raw_speed / ADC_MAX) * SPEED_MAX_KMH;
        distance_m = ((float)adc_raw_dist  / ADC_MAX) * DISTANCE_MAX_M;
        now_ms     = HAL_GetTick();

        /* Sensor plausibility check */
        if (speed_kmh > SPEED_MAX_KMH || distance_m < DISTANCE_MIN_M
                                       || distance_m > DISTANCE_MAX_M)
        {
            led_all_off();
            led_fault_on();
            HAL_Delay(100);
            continue;
        }
        led_fault_off();

        mode        = get_drive_mode(speed_kmh);
        warn_factor = (mode == MODE_CITY) ? WARN_FACTOR_CITY
                                          : WARN_FACTOR_HWY;
        d_stop  = compute_stop_distance(speed_kmh);
        d_warn  = warn_factor * d_stop;
        d_brake = d_stop;

        if (distance_m < d_brake)
        {
            led_brake_on();
        }
        else if (distance_m < d_warn)
        {
            led_warning_blink_nb(now_ms);
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_1, GPIO_PIN_RESET);
        }
        else
        {
            led_all_off();
        }

        HAL_Delay(100);
    }
}
