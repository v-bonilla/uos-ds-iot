/* Assignment 2, Name: Victor Bonilla Pardo, URN: 6612559 */

#include "contiki.h"

#include "stdio.h"
#include "math.h"
#include "dev/light-sensor.h"

static const int MEASUREMENT_SIZE = 12;
static const float LOW_STD_DEV_THRESHOLD = 5.0;
static const float MEDIUM_STD_DEV_THRESHOLD = 10.0;
static const float HIGH_STD_DEV_THRESHOLD = 25.0;
static const float VERY_HIGH_STD_DEV_THRESHOLD = 50.0;
static const int NO_AGGREGATION = 1;
static const int LOW_AGGREGATION_LEVEL = 2;
static const int MEDIUM_AGGREGATION_LEVEL = 3;
static const int HIGH_AGGREGATION_LEVEL = 4;
static const int VERY_HIGH_AGGREGATION_LEVEL = 6;

static const clock_time_t TIMER_INTERVAL = CLOCK_CONF_SECOND;
static struct etimer measurement_timer;
static process_event_t event_data_ready;

/* USER DEFINED FUNCTIONS */
/*---------------------------------------------------------------------------*/

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

struct Buffer{
    // Why not to use MEASUREMENT_SIZE? https://stackoverflow.com/a/20654444 -> 4th solution
    int measures[12];
    int count;
    float sum;
    float average;
    float std_dev;
    int lock;
    // char *process_locker;
};

static struct Buffer *create_buffer(){
    static struct Buffer result;
    result.count = 0;
    result.sum = 0.0;
    result.average = 0.0;
    result.std_dev = 0.0;
    result.lock = 0;
    // result.process_locker = "";
    return &result;
}

static int is_full_buffer(struct Buffer *buffer){
    return buffer->count == MEASUREMENT_SIZE;
}

static void insert_measure_buffer(struct Buffer *buffer, int measure){
    if (is_full_buffer(buffer) == 0){
        buffer->measures[buffer->count] = measure;
        buffer->count++;
        buffer->sum += (float) measure;
        printf("[Buffer %p] M: %d   MBuff: %p   C: %d   S: %u.%u\n", buffer, measure, buffer->measures[buffer->count], buffer->count, d1(buffer->sum), d2(buffer->sum));
    } else {
        printf("[Buffer %p] Buffer is full\n", buffer);
    }
}

static void calculate_stats_buffer(struct Buffer *buffer){
    buffer->average = buffer->sum / (float) buffer->count;
    
    int i;
    float sum_squared_dev = 0.0;
    for (i = 0; i < buffer->count; i++){
        sum_squared_dev += powf(buffer->measures[i] - buffer->average, 2.0);
    }
    buffer->std_dev = sqrtf(sum_squared_dev / (buffer->count - 1.0));
    
    printf("[Buffer %p] Average: %u.%u   Std_dev: %u.%u\n", buffer, d1(buffer->average), d2(buffer->average), d1(buffer->std_dev), d2(buffer->std_dev));
}

static int is_locked_buffer(struct Buffer *buffer){
    return buffer->lock;
}

static void lock_buffer(struct Buffer *buffer){
    while (is_locked_buffer(buffer) && buffer->count == 0){
        printf("[Buffer %p] Waiting to lock the buffer\n", buffer);
    }
    buffer->lock = 1;
    printf("[Buffer %p] Buffer locked\n", buffer);
}

static void unlock_buffer(struct Buffer *buffer){
    buffer->lock = 0;
    printf("[Buffer %p] Buffer unlocked\n", buffer);
}

static void reset_buffer(struct Buffer *buffer){
    buffer->sum = 0;
    buffer->average = 0.0;
    buffer->count = 0;
    // buffer->lock = 0;
    printf("[Buffer %p] Buffer reseted\n", buffer);
}

// static float * aggregate_measures(int measures[], int aggregation_level) {
//     printf("Aggregate measures with aggregation level %d\n", aggregation_level);
//     float result[MEASUREMENT_SIZE / aggregation_level];
//     int i;
//     int aggregation_sum = 0;
//     for(i = 0; i < MEASUREMENT_SIZE; i++){
//         aggregation_sum += measures[i];
//         printf("i: %d   agg_sum: %d   mod: %d\n", i, aggregation_sum, (i + 1) % aggregation_level);
//         if (i > 0 && (i + 1) % aggregation_level == 0){
//             result[((i + 1) / aggregation_level) - 1] = aggregation_sum / aggregation_level;
//             aggregation_sum = 0;
//         }
//     }
//     int j;
//     for (j = 0; j < MEASUREMENT_SIZE / aggregation_level; j++){
//         printf("j: %d   result[%d]: %u.%u\n", j, j, d1(result[j]), d2(result[j]));
//     }
    
//     return result;
// }

static void print_buffer(int buffer[]){
    int i;
    for (i = 0; i < MEASUREMENT_SIZE - 1; i++){
        printf("%d, ", buffer[i]);
    }
    printf("%d)\n", buffer[MEASUREMENT_SIZE - 1]);
}

static void print_measures(int measures[]){
    printf("B = (");
    print_buffer(measures);
}

static void print_measures_as_result(int measures[]){
    printf("X = (");
    print_buffer(measures);
}

static void print_results(int original_measures[], float std_dev, int aggregation_level, float aggregation[]){
    print_measures(original_measures);
    printf("StdDev = %u.%u\n", d1(std_dev), d2(std_dev));
    printf("Number of values to aggregate = %d\n", aggregation_level);
    if (aggregation_level == NO_AGGREGATION){
        print_measures_as_result(original_measures);
    } else {
        printf("X = (");
        int i;
        int aggregation_size = MEASUREMENT_SIZE / aggregation_level;
        for (i = 0; i < aggregation_size - 1; i++){
            printf("%u.%u, ", d1(aggregation[i]), d2(aggregation[i]));
        }
        printf("%u.%u)\n", d1(aggregation[aggregation_size - 1]), d2(aggregation[aggregation_size - 1]));
    }
}

/* PROCESSES */
/*---------------------------------------------------------------------------*/

PROCESS(measurement, "Measure the light continuously and calculate stats");
PROCESS(aggregation, "Aggregate the measurement if needed");

AUTOSTART_PROCESSES(&measurement, &aggregation);

/*---------------------------------------------------------------------------*/

/* Measurement process:
 * Measure light continuously and send data to the agregation process.
 */
PROCESS_THREAD(measurement, ev, data)
{  

    static struct Buffer *buffer;

    PROCESS_BEGIN();
    // printf("Test\n");
    // int test[] = {1,1,1,2,2,2,3,3,3,4,4,4};
    // print_results(test, 0.0, 3, aggregate_measures(test, 3));
    
    printf("[Measurement] Create buffer\n");
    buffer = create_buffer();
    printf("[Measurement] Created buffer in: %p   1stM: %d   C: %d   S: %d   A: %u.%u   SD: %u.%u   L: %d\n", buffer, buffer->measures[buffer->count], buffer->count, buffer->sum, d1(buffer->average), d2(buffer->average), d1(buffer->std_dev), d2(buffer->std_dev), buffer->lock);
    event_data_ready = process_alloc_event();
    etimer_set(&measurement_timer, TIMER_INTERVAL);
    SENSORS_ACTIVATE(light_sensor);

    while(1) {
        
        lock_buffer(buffer);
        printf("[Measurement] Start measurement\n");
        while(!is_full_buffer(buffer)){
            
            PROCESS_WAIT_EVENT_UNTIL(ev=PROCESS_EVENT_TIMER);
            
            int current_light = light_sensor.value(LIGHT_SENSOR_PHOTOSYNTHETIC);
            insert_measure_buffer(buffer, current_light);

            etimer_reset(&measurement_timer);
            
        }
        calculate_stats_buffer(buffer);

        if (buffer->std_dev < VERY_HIGH_STD_DEV_THRESHOLD){
            unlock_buffer(buffer);
            printf("[Measurement] Std dev of buffer %p is not very high\n", buffer);
            printf("[Measurement] Sending buffer to processing thread to aggregate the data\n");
            process_post_synch(&aggregation, event_data_ready, &buffer);
        } else {
            printf("[Measurement] Std dev of buffer %p is very high\n", buffer);
            printf("[Measurement] No aggregation is done\n");
            print_results(buffer->measures, buffer->std_dev, NO_AGGREGATION, NULL);
        }
        
        reset_buffer(buffer);
        unlock_buffer(buffer);

    }  

  PROCESS_END();
}

/*---------------------------------------------------------------------------*/

/* Processing data process:
 * Aggregate data
 */
PROCESS_THREAD(aggregation, ev, data)
{  
    PROCESS_BEGIN();
    while(1) {

        printf("[Processing] Waiting for the data\n");
        PROCESS_WAIT_EVENT_UNTIL(ev == event_data_ready);

        struct Buffer *buffer = data + sizeof(int); // See 1
        printf("[Processing] Buffer received\n");
        lock_buffer(buffer);

        int aggregation_level;
        if (buffer->std_dev < LOW_STD_DEV_THRESHOLD){
            aggregation_level = VERY_HIGH_AGGREGATION_LEVEL;
        } else if (buffer->std_dev < MEDIUM_STD_DEV_THRESHOLD){
            aggregation_level = HIGH_AGGREGATION_LEVEL;
        } else if (buffer->std_dev < HIGH_STD_DEV_THRESHOLD){
            aggregation_level = MEDIUM_AGGREGATION_LEVEL;
        } else if (buffer->std_dev < VERY_HIGH_STD_DEV_THRESHOLD){
            aggregation_level = LOW_AGGREGATION_LEVEL;
        }

        printf("Aggregate measures with aggregation level %d\n", aggregation_level);
        float aggregation[MEASUREMENT_SIZE / aggregation_level];
        int i;
        int aggregation_sum = 0;
        for(i = 0; i < MEASUREMENT_SIZE; i++){
            aggregation_sum += buffer->measures[i];
            // printf("i: %d   agg_sum: %d   mod: %d\n", i, aggregation_sum, (i + 1) % aggregation_level);
            if (i > 0 && (i + 1) % aggregation_level == 0){
                aggregation[((i + 1) / aggregation_level) - 1] = (float) aggregation_sum / aggregation_level;
                aggregation_sum = 0;
            }
        }
        print_results(buffer->measures, buffer->std_dev, aggregation_level, aggregation);
        
        unlock_buffer(buffer);

    }
    PROCESS_END();
}

/*---------------------------------------------------------------------------*/

// 1. Why the pointer in the receiving process is one value less?
// 1. ANSWER: 
// 2. Scheduling the processes: when posting the event the receiver doesn't start
// 2. ANSWER: Cause the kernel didn't have time to switch thread context