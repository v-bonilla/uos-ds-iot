/* Assignment 2, Name: Victor Bonilla Pardo, URN: 6612559 
* References:
* [1] Lecture 6: IoT Data Processing (Slides), EEM048/COM3023- Internet of Thing, Prof. Payam Barnaghi
* [2] Jessica Lin, Eamonn Keogh, Stefano Lonardi, and Bill Chiu. 2003. A symbolic representation of time series, with implications for streaming algorithms. In Proceedings of the 8th ACM SIGMOD workshop on Research issues in data mining and knowledge discovery (DMKD '03). ACM, New York, NY, USA, 2-11.
* [3] Keogh, E,. Chakrabarti, K,. Pazzani, M. & Mehrotra (2000). Dimensionality reduction for fast similarity search in large time series databases. Journal of Knowledge and Information Systems.
* [4] Senin, P., Lin, J., Wang, X., Oates, T., Gandhi, S., Boedihardjo, A.P., Chen, C., Frankenstein, S., GrammarViz 3.0: Interactive Discovery of Variable-Length Time Series Patterns, ACM Trans. Knowl. Discov. Data, February 2018.
*/

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

static const int PAA_SIZE = 3;
static const int ALPHABET_SIZE = 3;
static const char ALPHABET[] = { 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j'}; // The code is not designed to work with more than 10 letters
static const float case2[] = { 0 };
static const float case3[] = { -0.430, 0.430 };
static const float case4[] = { -0.674, 0, 0.674 };
static const float case5[] = { -0.841, -0.253, 0.253, 0.841 };
static const float case6[] = { -0.967, -0.430, 0, 0.430, 0.967 };
static const float case7[] = { -1.067, -0.565, -0.180, 0.180, 0.565, 1.067 };
static const float case8[] = { -1.150, -0.674, -0.318, 0, 0.318, 0.674, 1.150 };
static const float case9[] = { -1.220, -0.764, -0.430, -0.139, 0.139, 0.430, 0.764, 1.220 };
static const float case10[] = { -1.281, -0.841, -0.524, -0.253, 0, 0.253, 0.524, 0.841, 1.281 };

static const clock_time_t TIMER_INTERVAL = CLOCK_CONF_SECOND;
static struct etimer measurement_timer;
static process_event_t event_data_ready;

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

int length(void * array){
    unsigned long n = sizeof(array) / sizeof(array[0]);
    printf("Lenght method:\n");
    printf("Array: %p   Size: %lu\n", array, n);
    return n;
}

struct Timeseries{
    // Why not to use MEASUREMENT_SIZE? https://stackoverflow.com/a/20654444 -> 4th solution
    int values[12];
    int count;
    float sum;
    float average;
    float std_dev;
    int lock;
};

struct Timeseries * create_buffer(){
    static struct Timeseries result;
    result.count = 0;
    result.sum = 0.0;
    result.average = 0.0;
    result.std_dev = 0.0;
    result.lock = 0;
    return &result;
}

int is_full_buffer(struct Timeseries *buffer){
    return buffer->count == MEASUREMENT_SIZE;
}

void insert_measure_buffer(struct Timeseries *buffer, int measure){
    if (is_full_buffer(buffer) == 0){
        buffer->values[buffer->count] = measure;
        buffer->count++;
        buffer->sum += (float) measure;
        printf("[Timeseries %p] M: %d   MBuff: %p   C: %d   S: %d.%d\n", buffer, measure, buffer->values[buffer->count], buffer->count, d1(buffer->sum), d2(buffer->sum));
    } else {
        printf("[Timeseries %p] Timeseries is full\n", buffer);
    }
}

void insert_all_measures_buffer(struct Timeseries *buffer, int array[]){
    int i;
    for (i = 0; i < MEASUREMENT_SIZE; i++){
        insert_measure_buffer(buffer,  array[i]);
    }
}

void calculate_stats_buffer(struct Timeseries *buffer){
    buffer->average = buffer->sum / (float) buffer->count;
    
    int i;
    float sum_squared_dev = 0.0;
    for (i = 0; i < buffer->count; i++){
        sum_squared_dev += powf(buffer->values[i] - buffer->average, 2.0);
    }
    buffer->std_dev = sqrtf(sum_squared_dev / (buffer->count - 1.0));
    
    printf("[Timeseries %p] Average: %d.%d   Std_dev: %d.%d\n", buffer, d1(buffer->average), d2(buffer->average), d1(buffer->std_dev), d2(buffer->std_dev));
}

int is_locked_buffer(struct Timeseries *buffer){
    return buffer->lock;
}

void lock_buffer(struct Timeseries *buffer){
    while (is_locked_buffer(buffer) && buffer->count == 0){
        printf("[Timeseries %p] Waiting to lock the buffer\n", buffer);
    }
    buffer->lock = 1;
    printf("[Timeseries %p] Timeseries locked\n", buffer);
}

void unlock_buffer(struct Timeseries *buffer){
    buffer->lock = 0;
    printf("[Timeseries %p] Timeseries unlocked\n", buffer);
}

void reset_buffer(struct Timeseries *buffer){
    buffer->sum = 0;
    buffer->average = 0.0;
    buffer->count = 0;
    printf("[Timeseries %p] Timeseries reseted\n", buffer);
}

/* PROCESSES */
/*---------------------------------------------------------------------------*/

PROCESS(measurement, "Measure process");
PROCESS(aggregation, "Aggregate process");

AUTOSTART_PROCESSES(&measurement, &aggregation);

/*---------------------------------------------------------------------------*/

/* Measurement process:
 * Measure light continuously and send data to the agregation process.
 */
PROCESS_THREAD(measurement, ev, data)
{  

    static struct Timeseries *buffer;

    PROCESS_BEGIN();

    printf("[Measurement] Create buffer\n");
    buffer = create_buffer();
    printf("[Measurement] Created buffer in: %p   1stM: %d   C: %d   S: %d   A: %d.%d   SD: %d.%d   L: %d\n", buffer, buffer->values[buffer->count], buffer->count, buffer->sum, d1(buffer->average), d2(buffer->average), d1(buffer->std_dev), d2(buffer->std_dev), buffer->lock);
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
            // Printing results of measurement
            printf("[Measurement] Results of measurement:");
            printf("B = (");
            int i;
            for (i = 0; i < MEASUREMENT_SIZE - 1; i++){
                printf("%d, ", buffer->values[i]);
            }
            printf("%d)\n", buffer->values[MEASUREMENT_SIZE - 1]);
            printf("StdDev = %d.%d\n", d1(buffer->std_dev), d2(buffer->std_dev));
            printf("No aggregation done\n");
            printf("X = (");
            for (i = 0; i < MEASUREMENT_SIZE - 1; i++){
                printf("%d, ", buffer->values[i]);
            }
            printf("%d)\n", buffer->values[MEASUREMENT_SIZE - 1]);

        }
        
        reset_buffer(buffer);
        unlock_buffer(buffer);

    }  

  PROCESS_END();
}

/*---------------------------------------------------------------------------*/

/* Processing data process:
 * Aggregate data with SAX
 */
PROCESS_THREAD(aggregation, ev, data)
{  
    PROCESS_BEGIN();
    while(1) {

        printf("[Aggregating] Waiting for the data\n");
        PROCESS_WAIT_EVENT_UNTIL(ev == event_data_ready);

        struct Timeseries *ts = data + sizeof(int); // See question 1

        printf("[Aggregating] Timeseries received\n");
        lock_buffer(ts);

        printf("[Aggregating] Aggregate values with SAX\n");
        
        int saxed_ts_size = MEASUREMENT_SIZE / PAA_SIZE;
        float znormalised[MEASUREMENT_SIZE];
        float paa_values[saxed_ts_size];
        char saxed_ts[saxed_ts_size];
        float * breakpoints;
        int n_breakpoints = ALPHABET_SIZE - 1;
        int i, j;
        
        // Normalising
        printf("[Aggregating] Normalising time series\n");
        for (i = 0; i < MEASUREMENT_SIZE; i++){
            znormalised[i] = ((float) ts->values[i] - ts->average) / ts->std_dev;
        }

        // PAA
        printf("[Aggregating] Transforming time series with PAA(w=%d)\n", PAA_SIZE);
        if(PAA_SIZE > MEASUREMENT_SIZE){
            printf("PAA size can't be larger than the time series\n");
            unlock_buffer(ts);
            continue;
        } else if(PAA_SIZE == MEASUREMENT_SIZE){
            printf("PAA size equals the time series\n");
            unlock_buffer(ts);
            continue;
        } else if(MEASUREMENT_SIZE % PAA_SIZE != 0){
            printf("PAA size has to be a multiple of the measurement size in this implementation\n");
            unlock_buffer(ts);
            continue;
        } else {
            float sum_frame;
            for (i = 1; i <= saxed_ts_size; i++){
                sum_frame = 0;
                for (j = (MEASUREMENT_SIZE * (i - 1) + 1) / saxed_ts_size; j < MEASUREMENT_SIZE * i / saxed_ts_size; j++){
                    sum_frame = sum_frame + znormalised[j];
                }
                paa_values[i - 1] = (float) PAA_SIZE * sum_frame / (float) MEASUREMENT_SIZE;
            }
        }

        // Select breakpoints
        switch (ALPHABET_SIZE){
            case 2:
                breakpoints = (float *) case2;
                break;
            case 3:
                breakpoints = (float *) case3;
                break;
            case 4:
                breakpoints = (float *) case4;
                break;
            case 5:
                breakpoints = (float *) case5;
                break;
            case 6:
                breakpoints = (float *) case6;
                break;
            case 7:
                breakpoints = (float *) case7;
                break;
            case 8:
                breakpoints = (float *) case8;
                break;
            case 9:
                breakpoints = (float *) case9;
                break;
            case 10:
                breakpoints = (float *) case10;
                break;
            default:
                printf("Invalid alphabet_size\n");
                unlock_buffer(ts);
                continue;
        }

        // PAA to char
        printf("[Aggregating] Transforming PAA values to characters with alphabet size = %d\n", ALPHABET_SIZE);
        for (i = 0; i < saxed_ts_size; i++){

            float value = paa_values[i];
            if (value < breakpoints[0]){
                saxed_ts[i] = ALPHABET[0];
            } else if (value >= breakpoints[n_breakpoints - 1]){
                saxed_ts[i] = ALPHABET[ALPHABET_SIZE - 1];
            } else{
                j = 1;
                while (!((breakpoints[j - 1]<= value) && (value < breakpoints[j]))){
                    j++;
                }
                saxed_ts[i] = ALPHABET[j];
            }
                        
        }

        // Printing results of SAX
        printf("[Aggregating] Results of aggregation:\n");
        printf("B = (");
        for (i = 0; i < MEASUREMENT_SIZE - 1; i++){
            printf("%d, ", ts->values[i]);
        }
        printf("%d)\n", ts->values[MEASUREMENT_SIZE - 1]);
        printf("StdDev = %d.%d\n", d1(ts->std_dev), d2(ts->std_dev));
        printf("SAX parameters: window = %d; alphabet_size = %d\n", PAA_SIZE, ALPHABET_SIZE);
        printf("X = (");
        for (i = 0; i < saxed_ts_size - 1; i++){
            printf("%c, ", saxed_ts[i]);
        }
        printf("%c)\n", saxed_ts[saxed_ts_size - 1]);

        unlock_buffer(ts);
    }
    PROCESS_END();
}

/*---------------------------------------------------------------------------*/

// 1. Why the pointer in the receiving process is one value less?
// 1. ANSWER: 
// 2. Scheduling the processes: when posting the event the receiver doesn't start
// 2. ANSWER: Cause the kernel didn't have time to switch thread context