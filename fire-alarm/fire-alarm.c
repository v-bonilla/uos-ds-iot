/* Assignment 1, Name: Victor Bonilla Pardo, URN: 6612559 */

#include "contiki.h"

#include "stdio.h"
#include "dev/light-sensor.h"
#include "dev/sht11-sensor.h"
#include "dev/leds.h"

static const int MEASUREMENT_SIZE = 12;
static int CONFIGURATION_COUNTER = 10;
static int M_TO_CONFIGURATION = 10;

static const int TEMP_THRESHOLD = 6440; // 6560 = 26ºC
static const int LIGHT_THRESHOLD = 218; // 218 = 500lux
static const char NOTIFICATION_TEXT[] = "Fire!\n";

/* PROCESSES */
/*---------------------------------------------------------------------------*/

PROCESS(fire_check, "Fire check process");
PROCESS(notification, "Alarm notification process");

AUTOSTART_PROCESSES(&fire_check);

/*---------------------------------------------------------------------------*/

/* Fire check process:
 * It checks for each second for fire given the two defined thresholds: temperature and light.
 */
PROCESS_THREAD(fire_check, ev, data)
{  
    static struct etimer fire_check_timer;
    static struct Timeseries *ts;
    
    PROCESS_BEGIN();

    etimer_set(&fire_check_timer,CLOCK_CONF_SECOND);

    int FIRE = 0;

    SENSORS_ACTIVATE(light_sensor);
    SENSORS_ACTIVATE(sht11_sensor);

    while(1) {
        PROCESS_WAIT_EVENT_UNTIL(ev=PROCESS_EVENT_TIMER);

        int current_temp = sht11_sensor.value(SHT11_SENSOR_TEMP);
        int current_light = light_sensor.value(LIGHT_SENSOR_PHOTOSYNTHETIC);
        printf("T: %d    L: %d\n", current_temp, current_light);

        if(FIRE == 0 && current_temp >= TEMP_THRESHOLD && current_light >= LIGHT_THRESHOLD) {
            process_start(&notification, NULL);
            FIRE = 1;
        }
        else if(FIRE == 1 && (current_temp < TEMP_THRESHOLD || current_light < LIGHT_THRESHOLD)) {
            process_exit(&notification);
            FIRE = 0;
        }

        etimer_reset(&fire_check_timer);
    }  

  PROCESS_END();
}

/*---------------------------------------------------------------------------*/

/* Notificarion process:
 * It blinks leds and prints a message for each second.
 */
PROCESS_THREAD(notification, ev, data)
{  
    static struct etimer notification_timer;

    PROCESS_BEGIN();

    etimer_set(&notification_timer,CLOCK_CONF_SECOND);
    while(1) {
        PROCESS_WAIT_EVENT_UNTIL(ev=PROCESS_EVENT_TIMER);

        leds_blink();
        printf(NOTIFICATION_TEXT);

        etimer_reset(&notification_timer);
    }
 
  PROCESS_END();
}

/*---------------------------------------------------------------------------*/
