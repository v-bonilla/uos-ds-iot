/* Assignment 1, Name: Victor Bonilla Pardo, URN: 6612559 
* Refrences:
* [1] Adam Dunkels, https://github.com/contiki-os/contiki/blob/master/examples/rime/example-unicast.c
*/

#include "contiki.h"

#include "stdio.h"
#include "dev/light-sensor.h"
#include "dev/sht11-sensor.h"
#include "dev/leds.h"
#include "net/rime.h"

static const int MEASUREMENT_SIZE = 10;
// Reconfiguration in an hour
static int CONFIGURATION_COUNTER = 60 * 60;
static int M_TO_CONFIGURATION = 60 * 60;

static int TEMP_THRESHOLD = 0; // 6560 = 26ºC
static int LIGHT_THRESHOLD = 0; // 218 = 500lux
static const char NOTIFICATION_TEXT[] = "Fire!\n";

static struct unicast_conn uc;

/* USER DEFINED FUNCTIONS */
/*---------------------------------------------------------------------------*/

/* digits before decimal point */
int d1(float f)
{
	return ((int)f);
}

/* digits after decimal point */
int d2(float f)
{
	return (1000 * (f - d1(f)));
}

int add_20_degrees(int temperature){
    float t_degrees = 0.01 * temperature - 39.6;
    return (int) (100 * (t_degrees + 20 + 39.6));
}

static void recv_uc(struct unicast_conn *c, const rimeaddr_t *from){
  printf("[Unicast] Message received from %d.%d\n", from->u8[0], from->u8[1]);
}
static const struct unicast_callbacks unicast_callbacks = {recv_uc};

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
    
    PROCESS_BEGIN();

    etimer_set(&fire_check_timer, CLOCK_CONF_SECOND);

    static int FIRE = 0;


    SENSORS_ACTIVATE(light_sensor);
    SENSORS_ACTIVATE(sht11_sensor);

    printf("[Fire check] Starting fire check\n");
    while(1) {

        if (CONFIGURATION_COUNTER == M_TO_CONFIGURATION){
            // Configuration
            static int i;
            static int temp_measures[10];
            static float temp_average = 0.0;
            static int light_measures[10];
            static float light_average = 0.0;
            
            printf("[Configuration] Starting configuration\n");
            for (i = 0; i < MEASUREMENT_SIZE; i++){

                PROCESS_WAIT_EVENT_UNTIL(ev=PROCESS_EVENT_TIMER);

                int current_temp = sht11_sensor.value(SHT11_SENSOR_TEMP);
                int current_light = light_sensor.value(LIGHT_SENSOR_PHOTOSYNTHETIC);    
                temp_measures[i] = current_temp;
                temp_average += current_temp;
                light_measures[i] = current_light;
                light_average += current_light;

                etimer_reset(&fire_check_timer);
            }
            temp_average = temp_average / MEASUREMENT_SIZE;
            light_average = light_average / MEASUREMENT_SIZE;
            TEMP_THRESHOLD = add_20_degrees(temp_average);
            LIGHT_THRESHOLD = (int) light_average + light_average * 0.5; // LIGHT_THRESHOLD is 50% greater than average

            CONFIGURATION_COUNTER = 0;
            printf("[Configuration] measurement_size = %d   temp_average = %d.%d   light_average = %d.%d\n", MEASUREMENT_SIZE, d1(temp_average), d2(temp_average), d1(light_average), d2(light_average));
            printf("[Configuration] Configured: TEMP_THRESHOLD=%d; LIGHT_THRESHOLD=%d\n", TEMP_THRESHOLD, LIGHT_THRESHOLD);
        }

        PROCESS_WAIT_EVENT_UNTIL(ev=PROCESS_EVENT_TIMER);

        int current_temp = sht11_sensor.value(SHT11_SENSOR_TEMP);
        int current_light = light_sensor.value(LIGHT_SENSOR_PHOTOSYNTHETIC);
        printf("[Fire check]  T: %d    L: %d\n", current_temp, current_light);

        if(FIRE == 0 && current_temp >= TEMP_THRESHOLD && current_light >= LIGHT_THRESHOLD) {
            process_start(&notification, NULL);
            FIRE = 1;
        }
        else if(FIRE == 1 && (current_temp < TEMP_THRESHOLD || current_light < LIGHT_THRESHOLD)) {
            process_exit(&notification);
            FIRE = 0;
        }

        CONFIGURATION_COUNTER++;
        etimer_reset(&fire_check_timer);
    }  

  PROCESS_END();
}

/*---------------------------------------------------------------------------*/

/* Notification process:
 * It blinks leds, prints a message and send it through unicast for each second.
 */
PROCESS_THREAD(notification, ev, data)
{  
    static struct etimer notification_timer;
    rimeaddr_t addr;

    PROCESS_BEGIN();

    unicast_open(&uc, 146, &unicast_callbacks);
    etimer_set(&notification_timer, CLOCK_CONF_SECOND);
    while(1) {
        PROCESS_WAIT_EVENT_UNTIL(ev=PROCESS_EVENT_TIMER);

        leds_blink();
        printf("[Notification] ");
        printf(NOTIFICATION_TEXT);

        printf("[Notification] Sending unicast packet\n");
        packetbuf_copyfrom("Fire!", 5);
        addr.u8[0] = 1;
        addr.u8[1] = 0;
        if(!rimeaddr_cmp(&addr, &rimeaddr_node_addr)) {
            unicast_send(&uc, &addr);
        }

        etimer_reset(&notification_timer);
    }
 
  PROCESS_END();
}

/*---------------------------------------------------------------------------*/
/*-------------------------- END OF ASSIGNMENT 1 ----------------------------*/