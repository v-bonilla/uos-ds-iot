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
#include "net/rime.h" 

#define BROADCAST_PORT 1100 

static const int MEASUREMENT_SIZE = 10;
static int CONFIGURATION_COUNTER = 10;
static int M_TO_CONFIGURATION = 10;

static const float VERY_LOW_STD_DEV_THRESHOLD = 5.0;
static const float LOW_STD_DEV_THRESHOLD = 10.0;
static const float MEDIUM_STD_DEV_THRESHOLD = 20.0;
static const float HIGH_STD_DEV_THRESHOLD = 30.0;
static const float VERY_HIGH_STD_DEV_THRESHOLD = 50.0;
static const float VERY_LOW_KURTOSIS_THRESHOLD = 10.0;
static const float LOW_KURTOSIS_THRESHOLD = 6.0;
static const float MEDIUM_KURTOSIS_THRESHOLD = 4.0;
static const float HIGH_KURTOSIS_THRESHOLD = 2.0;
static const float VERY_HIGH_KURTOSIS_THRESHOLD = 1.0;

static int PAA_SIZE = 0;
static int ALPHABET_SIZE = 0;
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

static process_event_t event_data_ready;

static struct broadcast_conn broadcast;
static void broadcast_recv(struct broadcast_conn *c, const rimeaddr_t *from) 
{ 
    char * data = (char *)packetbuf_dataptr();
    printf("Broadcast received from %d.%d: '%s'\n", from->u8[0], from->u8[1], data); 
    // insert_measure_ts(ts, data);
}
static const struct broadcast_callbacks broadcast_call = {broadcast_recv};

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

struct Timeseries{
    // Why not to use MEASUREMENT_SIZE? https://stackoverflow.com/a/20654444 -> 4th solution
    int values[10];
    int count;
    float sum;
    float average;
    float std_dev;
    float kurtosis;
    int lock;
};

struct Timeseries * create_ts(){
    static struct Timeseries result;
    result.count = 0;
    result.sum = 0.0;
    result.average = 0.0;
    result.std_dev = 0.0;
    result.kurtosis = 0.0;
    result.lock = 0;
    return &result;
}

int is_full_ts(struct Timeseries *ts){
    return ts->count == MEASUREMENT_SIZE;
}

void insert_measure_ts(struct Timeseries *ts, int measure){
    if (is_full_ts(ts) == 0){
        ts->values[ts->count] = measure;
        ts->count++;
        ts->sum += (float) measure;
        // printf("[Timeseries %p] M: %d   MBuff: %p   C: %d   S: %d.%d\n", ts, measure, ts->values[ts->count], ts->count, d1(ts->sum), d2(ts->sum));
    } else {
        printf("[Timeseries %p] Timeseries is full\n", ts);
    }
}

void insert_all_measures_ts(struct Timeseries *ts, int array[]){
    int i;
    for (i = 0; i < MEASUREMENT_SIZE; i++){
        insert_measure_ts(ts,  array[i]);
    }
}

void calculate_stats_ts(struct Timeseries *ts){
    // Average
    ts->average = ts->sum / (float) ts->count;
    
    // Standard deviation and kurtosis
    int i;
    float sum_squared_dev, sum_4thpow = 0.0;
    for (i = 0; i < ts->count; i++){
        sum_squared_dev += powf(ts->values[i] - ts->average, 2.0);
        sum_4thpow += powf(ts->values[i] - ts->average, 4.0);
    }
    ts->std_dev = sqrtf(sum_squared_dev / (ts->count - 1.0)); // Standard deviation
    float fourth_momentum = sum_4thpow / (ts->count - 1.0);
    ts->kurtosis = fourth_momentum / powf(ts->std_dev, 4.0); // Kurtosis

    // printf("[Timeseries %p] Average: %d.%d   Std_dev: %d.%d   Kurtosis: %d.%d\n", ts, d1(ts->average), d2(ts->average), d1(ts->std_dev), d2(ts->std_dev), d1(ts->kurtosis), d2(ts->kurtosis));
}

int is_locked_ts(struct Timeseries *ts){
    return ts->lock;
}

void lock_ts(struct Timeseries *ts){
    while (is_locked_ts(ts) && ts->count == 0){
        printf("[Timeseries %p] Waiting to lock the ts\n", ts);
    }
    ts->lock = 1;
    printf("[Timeseries %p] Timeseries locked\n", ts);
}

void unlock_ts(struct Timeseries *ts){
    ts->lock = 0;
    printf("[Timeseries %p] Timeseries unlocked\n", ts);
}

void reset_ts(struct Timeseries *ts){
    ts->count = 0;
    ts->sum = 0;
    ts->average = 0.0;
    ts->std_dev = 0.0;
    ts->kurtosis = 0.0;
    printf("[Timeseries %p] Timeseries reseted\n", ts);
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

    static struct etimer measurement_timer;
    static const clock_time_t TIMER_INTERVAL = CLOCK_CONF_SECOND / 2;
    static struct Timeseries *ts;

    PROCESS_EXITHANDLER(broadcast_close(&broadcast);)
    PROCESS_BEGIN();

    printf("[Measurement] Create ts\n");
    ts = create_ts();
    printf("[Measurement] Created ts in: %p   1stM: %d   C: %d   S: %d   A: %d.%d   SD: %d.%d   K: %d.%d   L: %d\n", ts, ts->values[ts->count], ts->count, ts->sum, d1(ts->average), d2(ts->average), d1(ts->std_dev), d2(ts->std_dev), d1(ts->kurtosis), d2(ts->kurtosis), ts->lock);
    event_data_ready = process_alloc_event();
    etimer_set(&measurement_timer, TIMER_INTERVAL);
    SENSORS_ACTIVATE(light_sensor);

    while(1) {
        
        lock_ts(ts);
        printf("[Measurement] Start measurement. Measures to configuration: %d\n", M_TO_CONFIGURATION - CONFIGURATION_COUNTER);
        while(!is_full_ts(ts)){
            
            PROCESS_WAIT_EVENT_UNTIL(ev=PROCESS_EVENT_TIMER);
            
            int current_light = light_sensor.value(LIGHT_SENSOR_PHOTOSYNTHETIC);
            insert_measure_ts(ts, current_light);

            etimer_reset(&measurement_timer);
            
        }
        calculate_stats_ts(ts);
        
        printf("[Measurement] Print buffer each 5 seconds:\n");
        printf("B = (");
        int i;
        for (i = 0; i < MEASUREMENT_SIZE - 1; i++){
            printf("%d, ", ts->values[i]);
        }
        printf("%d)\n", ts->values[MEASUREMENT_SIZE - 1]);
        printf("Average = %d.%d\n", d1(ts->average), d2(ts->average));


        if (CONFIGURATION_COUNTER == M_TO_CONFIGURATION){
            // Configuration
            if (ts->std_dev < VERY_LOW_STD_DEV_THRESHOLD){
                PAA_SIZE = 2;
            } else if (ts->std_dev < LOW_STD_DEV_THRESHOLD){
                PAA_SIZE = 3;
            } else if (ts->std_dev < MEDIUM_STD_DEV_THRESHOLD){
                PAA_SIZE = 4;
            } else if (ts->std_dev < HIGH_STD_DEV_THRESHOLD){
                PAA_SIZE = 6;
            } else if (ts->std_dev > HIGH_STD_DEV_THRESHOLD){
                PAA_SIZE = 0;
            } 

            if (ts->kurtosis > VERY_LOW_KURTOSIS_THRESHOLD){
                ALPHABET_SIZE = 2;
            } else if (ts->kurtosis > LOW_KURTOSIS_THRESHOLD){
                ALPHABET_SIZE = 3;
            } else if (ts->kurtosis > MEDIUM_KURTOSIS_THRESHOLD){
                ALPHABET_SIZE = 5;
            } else if (ts->std_dev > MEDIUM_STD_DEV_THRESHOLD){
                if (ts->kurtosis > HIGH_KURTOSIS_THRESHOLD){
                    ALPHABET_SIZE = 7;
                } else if (ts->kurtosis > VERY_HIGH_KURTOSIS_THRESHOLD){
                    ALPHABET_SIZE = 9;
                } else if (ts->kurtosis < VERY_HIGH_KURTOSIS_THRESHOLD){
                    ALPHABET_SIZE = 10;
                }
            } else {
                printf("[Configuration] Kurtosis too high and standard deviation not that high. No reason to make the alphabet that complex\n");
                ALPHABET_SIZE = 4;
            }
            CONFIGURATION_COUNTER = 0;
            printf("[Configuration] Configured: WINDOW=%d; ALPHABET_SIZE=%d\n", PAA_SIZE, ALPHABET_SIZE);
        }
        if (PAA_SIZE > 100){
            unlock_ts(ts);
            printf("[Measurement] Sending ts to processing thread to aggregate the data\n");
            process_post_synch(&aggregation, event_data_ready, &ts);
        } else {
            broadcast_open(&broadcast, BROADCAST_PORT, &broadcast_call);
            // char pack[10];
            packetbuf_copyfrom("HI", 2);
            // packetbuf_copyfrom(snprintf(pack, sizeof ts->average, "%f", ts->average), sizeof pack);
            printf("[Measurement] SENT BROADCAST\n");
            broadcast_send(&broadcast);

            printf("[Measurement] Std dev of ts %p is very high\n", ts);
            printf("[Measurement] No aggregation is done\n");
            
            // Printing results of measurement
            printf("[Measurement] Results of measurement:");
            printf("B = (");
            int i;
            for (i = 0; i < MEASUREMENT_SIZE - 1; i++){
                printf("%d, ", ts->values[i]);
            }
            printf("%d)\n", ts->values[MEASUREMENT_SIZE - 1]);
            printf("StdDev = %d.%d\n", d1(ts->std_dev), d2(ts->std_dev));
            printf("No aggregation done\n");
            printf("X = (");
            for (i = 0; i < MEASUREMENT_SIZE - 1; i++){
                printf("%d, ", ts->values[i]);
            }
            printf("%d)\n", ts->values[MEASUREMENT_SIZE - 1]);

        }
        CONFIGURATION_COUNTER++;

        reset_ts(ts);
        unlock_ts(ts);

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

        struct Timeseries *ts = data + sizeof(int) * 7; // See question 1

        printf("[Aggregating] Timeseries received\n");
        lock_ts(ts);

        printf("[Aggregating] Aggregate values with SAX\n");
        
        int saxed_ts_size = PAA_SIZE;
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
            printf("[ERROR] PAA size can't be larger than the time series\n");
            unlock_ts(ts);
            continue;
        } else if(PAA_SIZE == MEASUREMENT_SIZE){
            printf("[Aggregating] PAA size equals the time series. No aggregation\n");
            // Should print the results but avoid for readability and this case is not gonna happen the way the code is written (thresholds configuration)
            unlock_ts(ts);
            continue;
        } else if(MEASUREMENT_SIZE % PAA_SIZE != 0){
            printf("[ERROR] PAA size has to be a multiple of the measurement size in this implementation\n");
            unlock_ts(ts);
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
                printf("[ERROR] Invalid alphabet_size\n");
                unlock_ts(ts);
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

        unlock_ts(ts);
    }
    PROCESS_END();
}

/*---------------------------------------------------------------------------*/

// 1. Why the pointer in the receiving process is different to the one in the sender process?
// 1. ANSWER: 

/*-------------------------- END OF ASSIGNMENT 2 ----------------------------*/
