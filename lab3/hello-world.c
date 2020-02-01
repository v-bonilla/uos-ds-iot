/*
 * Copyright (c) 2006, Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 *
 * $Id: hello-world.c,v 1.1 2006/10/02 21:46:46 adamdunkels Exp $
 */

/**
 * \file
 *         A very simple Contiki application showing how Contiki programs look
 * \author
 *         Adam Dunkels <adam@sics.se>
 */

#include "contiki.h"

#include <stdio.h>   /* For printf() */
#include <random.h>  /* For random_rand() */

#include <dev/light-sensor.h>
#include <dev/sht11-sensor.h>

float convert_raw_temp(int raw_temp_data) {
  return -39.6 + 0.01 * raw_temp_data; // 3V and 12 bit
}

float convert_raw_light(int raw_light_data) {
  float V_sensor = 1.5 * raw_light_data / 4096;
  float I = V_sensor / 100000;
  float light_lx = 0.625 * 1e6 * I * 1000;
  return light_lx;
}

int temp_to_raw(float t) {
  return 100 * (t + 39.6);
}

int light_to_raw(float l) {
  return 4096 * l / 9375;
}

void print_float(float f) {
  unsigned short d1 = (unsigned short)f; // d1: Integer part
  unsigned short d2 = (f-d1)*1000;       // d2: Fractional part
  printf("%u.%03u\n",d1,d2);
}

/*---------------------------------------------------------------------------*/
PROCESS(hello_world_process, "Hello world process");
AUTOSTART_PROCESSES(&hello_world_process);
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(hello_world_process, ev, data)
{
  static struct etimer timer;
  
  PROCESS_BEGIN();

  etimer_set(&timer,CLOCK_CONF_SECOND/4);
  SENSORS_ACTIVATE(light_sensor);
  SENSORS_ACTIVATE(sht11_sensor);

  printf("Hello, world\n"); // This is "Hello World"

  int i;
  float f, f_sum = 0;
  printf("Random Numbers:\n");
  for (i=0; i<10; i++)
  {
    unsigned short r = random_rand();
    f = ((float)r/RANDOM_RAND_MAX);
    f_sum += f;
    unsigned short d1 = (unsigned short)f; // d1: Integer part
    unsigned short d2 = (f-d1)*1000;       // d2: Fractional part
    printf("%u.%03u\n",d1,d2);
  }

  unsigned short d1 = (unsigned short)f_sum;
  unsigned short d2 = (f_sum-d1)*1000;
  printf("Sum = %u.%03u\n",d1,d2);

  while(1) {

    PROCESS_WAIT_EVENT_UNTIL(ev=PROCESS_EVENT_TIMER);
    
    // Here goes the code executed each second
    int raw_light_data = light_sensor.value(LIGHT_SENSOR_PHOTOSYNTHETIC);
    float light = convert_raw_light(raw_light_data);
    printf("Raw light data: %d\n", raw_light_data);
    print_float(light);
    printf("Reconvert light data:\n");
    print_float(light_to_raw(light));
    int raw_temp_data = sht11_sensor.value(SHT11_SENSOR_TEMP);
    float temp = convert_raw_temp(raw_temp_data);
    printf("Raw temperature data: %d\n", raw_temp_data);
    print_float(temp);
    printf("Reconvert temperature data:\n");
    print_float(temp_to_raw(temp));
    // TODO: Print some text with the converted values.

    etimer_reset(&timer);

  }
  
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/