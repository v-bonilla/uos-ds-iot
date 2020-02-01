#include "contiki.h"
#include <stdio.h> /* For printf() */

/* Include temperature sensor driver */
#include "dev/sht11-sensor.h"

/* Number of values used in averaging process */
#define MAX_VALUES 4

/* Variables: the application specific event value */
static process_event_t event_data_ready;

/* digits before decimal point */
unsigned short d1(float f)
{
	return ((unsigned short)f);
}

/* digits after decimal point */
unsigned short d2(float f)
{
	return (1000 * (f - d1(f)));
}

/*---------------------------------------------------------------------------*/
/* We declare the two processes */
PROCESS(temp_process, "Temperature process");
PROCESS(print_process, "Print process");

/* We require the processes to be started automatically */
AUTOSTART_PROCESSES(&temp_process, &print_process);
/*---------------------------------------------------------------------------*/
/* Implementation of the first process */
PROCESS_THREAD(temp_process, ev, data)
{
	/* Variables are declared static to ensure their values are kept */
	/* between kernel calls. */
	static struct etimer timer;
	static int count = 0;
	static float average = 0, valid_measure = 0;

	/* This variable is recomputed at every run, therefore it is not */
	/* necessary to declare them static. */
	float measure;

	/* Any process must start with this. */
	PROCESS_BEGIN();

	/* Allocate the required event */
	event_data_ready = process_alloc_event();

	/* Initialise the temperature sensor */
	SENSORS_ACTIVATE(sht11_sensor);

	/* Initialise variables */
	count = 0;
	average = 0;
	valid_measure = 0;
	measure = 0;

	/* Set the etimer module to generate an event in 0.25 second. */
	etimer_set(&timer, CLOCK_CONF_SECOND / 4);

	while (1)
	{
		/* Wait here for the timer to expire */
		PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_TIMER);

		/* Get temperature measurement and convert it to degrees */
		measure = 0.01 * sht11_sensor.value(SHT11_SENSOR_TEMP) - 39.6;

		/* Sum temperature measurements */
		average += measure;

		/* Count how many temperature measurement we have summed up */
		count++;

		if (count == MAX_VALUES)
		{
			/* Average the sum and store */
			valid_measure = average / MAX_VALUES;

			/* Reset variables */
			average = 0;
			count = 0;

			/* Post an event to the print process */
			/* and pass a pointer to the last measure as data */
			process_post(&print_process, event_data_ready, &valid_measure);
		}

		/* Reset the timer so it will generate another event */
		etimer_reset(&timer);
	}
	/* Any process must end with this, even if it is never reached. */
	PROCESS_END();
}
/*---------------------------------------------------------------------------*/
/* Implementation of the second process */
PROCESS_THREAD(print_process, ev, data)
{
	PROCESS_BEGIN();

	while (1)
	{
		/* Wait until we get a data_ready event */
		PROCESS_WAIT_EVENT_UNTIL(ev == event_data_ready);

		/* Use 'data' variable to retrieve data and then display it */
		printf("temperature = %u.%u\n", d1(*(float *)data), d2(*(float *)data));
	}
	PROCESS_END();
}
/*---------------------------------------------------------------------------*/
