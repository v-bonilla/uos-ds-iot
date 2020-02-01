/*
* Lab Session 5: A Simple Alarm
*/

#include "contiki.h"

#include <stdio.h>   /* For printf() */
#include <stdbool.h>
#include <random.h>  /* For random_rand() */

#include <dev/light-sensor.h>
#include <dev/sht11-sensor.h>
#include <dev/leds.h>

float convert_raw_temp(int raw_temp_data) {
  return -39.6 + 0.01 * raw_temp_data; // 3V and 12 bit
}

void print_float(float f) {
  unsigned short d1 = (unsigned short)f; // d1: Integer part
  unsigned short d2 = (f-d1)*1000;       // d2: Fractional part
  printf("%u.%03u\n",d1,d2);
}

/*---------------------------------------------------------------------------*/
PROCESS(alarm_process, "Alarm process");
AUTOSTART_PROCESSES(&alarm_process);
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(alarm_process, ev, data)
{
  const float TEMPERATURE_THRESHOLD = 29.0;
  static struct etimer timer;
  static bool ALARM = false;
  
  PROCESS_BEGIN();

  etimer_set(&timer,CLOCK_CONF_SECOND/4);
  SENSORS_ACTIVATE(sht11_sensor);

  while(1) {

    PROCESS_WAIT_EVENT_UNTIL(ev=PROCESS_EVENT_TIMER);
    
    int raw_temp_data = sht11_sensor.value(SHT11_SENSOR_TEMP);
    float temp = convert_raw_temp(raw_temp_data);
    // printf("Temperature data: ");
    // print_float(temp);

    if(temp > TEMPERATURE_THRESHOLD && ALARM == false) {
      leds_on(LEDS_ALL);
      ALARM = true;
    }
    else if(temp < TEMPERATURE_THRESHOLD && ALARM == true) {
      leds_off(LEDS_ALL);
      ALARM = false;
    }

    printf(ALARM ? "Alarm!\n": "");

    etimer_reset(&timer);
  }
  
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/