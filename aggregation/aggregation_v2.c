/* Assignment 2, Name: Victor Bonilla Pardo, URN: 6612559 
* References:
* [1] Lecture 6: IoT Data Processing (Slides), EEM048/COM3023- Internet of Thing, Prof. Payam Barnaghi
* [2] Jessica Lin, Eamonn Keogh, Stefano Lonardi, and Bill Chiu. 2003. A symbolic representation of time series, with implications for streaming algorithms. In Proceedings of the 8th ACM SIGMOD workshop on Research issues in data mining and knowledge discovery (DMKD '03). ACM, New York, NY, USA, 2-11.
* [3] Keogh, E,. Chakrabarti, K,. Pazzani, M. & Mehrotra (2000). Dimensionality reduction for fast similarity search in large time series databases. Journal of Knowledge and Information Systems.
* [4] Senin, P., Lin, J., Wang, X., Oates, T., Gandhi, S., Boedihardjo, A.P., Chen, C., Frankenstein, S., GrammarViz 3.0: Interactive Discovery of Variable-Length Time Series Patterns, ACM Trans. Knowl. Discov. Data, February 2018.
*
*
*
*/

#include "contiki.h"
// #include "lib/memb.h"

#include "stdio.h"
#include "math.h"
#include "float.h"
#include "dev/light-sensor.h"

#define LENGHT(x)  (sizeof(x) / sizeof((x)[0]))
#define TESTT 1

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

static const int PAA_SIZE = 2;
static const int ALPHABET_SIZE = 3;
static const char ALPHABET[] = { 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k',
      'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z' };
static const float case2[] = { 0 };
static const float case3[] = { -0.4307273, 0.4307273 };
static const float case4[] = { -0.6744898, 0, 0.6744898 };
static const float case5[] = { -0.841621233572914, -0.2533471031358, 0.2533471031358,
    0.841621233572914 };
static const float case6[] = { -0.967421566101701, -0.430727299295457, 0,
    0.430727299295457, 0.967421566101701 };
static const float case7[] = { -1.06757052387814, -0.565948821932863, -0.180012369792705,
    0.180012369792705, 0.565948821932863, 1.06757052387814 };
static const float case8[] = { -1.15034938037601, -0.674489750196082, -0.318639363964375,
    0, 0.318639363964375, 0.674489750196082, 1.15034938037601 };
static const float case9[] = { -1.22064034884735, -0.764709673786387, -0.430727299295457,
    -0.139710298881862, 0.139710298881862, 0.430727299295457, 0.764709673786387,
    1.22064034884735 };
static const float case10[] = { -1.2815515655446, -0.841621233572914, -0.524400512708041,
    -0.2533471031358, 0, 0.2533471031358, 0.524400512708041, 0.841621233572914, 1.2815515655446 };

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

// struct Int {
//     int value;
// };

// struct Float {
//     float value;
// };

// struct Char {
//     char character;
// };

// MEMB(values, struct Float, MEASUREMENT_SIZE);
// MEMB(values, struct Float, PAA_SIZE);
// MEMB(values, struct Char, PAA_SIZE);

struct Timeseries{
    // Why not to use MEASUREMENT_SIZE? https://stackoverflow.com/a/20654444 -> 4th solution
    int values[12];
    int count;
    float sum;
    float average;
    float std_dev;
    int lock;
};

// struct SaxedTS{
//     struct Int * original_values;
//     int window;
//     int paa_size;
//     struct Float * znormalised;
//     struct Float * paa_values;
//     struct Char * saxed_ts;
// };

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
        printf("[Timeseries %p] M: %d   MBuff: %p   C: %d   S: %u.%u\n", buffer, measure, buffer->values[buffer->count], buffer->count, d1(buffer->sum), d2(buffer->sum));
    } else {
        printf("[Timeseries %p] Timeseries is full\n", buffer);
    }
}

void insert_all_measures_buffer(struct Timeseries *buffer, int array[]){
    // if (lenght(array) == MEASUREMENT_SIZE){
        int i;
        for (i = 0; i < MEASUREMENT_SIZE; i++){
            insert_measure_buffer(buffer,  array[i]);
        }
    // }
    
}

void calculate_stats_buffer(struct Timeseries *buffer){
    buffer->average = buffer->sum / (float) buffer->count;
    
    int i;
    float sum_squared_dev = 0.0;
    for (i = 0; i < buffer->count; i++){
        sum_squared_dev += powf(buffer->values[i] - buffer->average, 2.0);
    }
    buffer->std_dev = sqrtf(sum_squared_dev / (buffer->count - 1.0));
    
    printf("[Timeseries %p] Average: %u.%u   Std_dev: %u.%u\n", buffer, d1(buffer->average), d2(buffer->average), d1(buffer->std_dev), d2(buffer->std_dev));
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

void printarrayf(float * array, int array_length){
    int i;
    printf("[printarrayf] Addres of array %p\n", &array);
    printf("[printarrayf] ");

    for (i = 0; i < array_length; i++){
        printf("%u.%u ", d1(array[i]), d2(array[i]));
    }
    printf("\n");
}

void print_buffer(int buffer[]){
    int i;
    for (i = 0; i < MEASUREMENT_SIZE - 1; i++){
        printf("%d, ", buffer[i]);
    }
    printf("%d)\n", buffer[MEASUREMENT_SIZE - 1]);
}

void print_measures(int values[]){
    printf("B = (");
    print_buffer(values);
}

void print_measures_as_result(int values[]){
    printf("X = (");
    print_buffer(values);
}

void print_results(int original_measures[], float std_dev, int aggregation_level, float aggregation[]){
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

void znorm(int input[], float output[], float average, float std_dev){
    int i;
    for (i = 0; i < MEASUREMENT_SIZE; i++){
        output[i] = ((float) input[i] - average) / std_dev;
    }
}

void paa(float input[], float output[], int paa_size){
    if(paa_size > MEASUREMENT_SIZE){
        printf("PAA size can't be larger than the time series\n'");
    } else if(paa_size == MEASUREMENT_SIZE){
        return input;
    } else {
        int n = MEASUREMENT_SIZE;
        int i, j;
        double sum_frame;
        for (i = 1; i <= paa_size; i++){
            sum_frame = 0;
            printf("[Inside i] Addresses: i %p   j %p   sum_frame %p   input[j] %p   output[i] %p\n", &i, &j, &sum_frame, &input[j], &output[i]);
            printf("[Inside i] sum_frame: %u.%u\n", d1(sum_frame), d2(sum_frame));
            printf("[Inside i] %d  n %d   i %d   paa_size %d\n", (n * (i - 1) + 1) / paa_size, n,i,paa_size);
            printf("\n");
            for (j = (n * (i - 1) + 1) / paa_size; j < n * i / paa_size; j++){
                printf("[Inside j] Addresses: i %p   j %p   sum_frame %p   input[j] %p   output[i] %p\n", &i, &j, &sum_frame, &input[j], &output[i]);
                printf("[Inside j] sum_frame: %u.%u\n", d1(sum_frame), d2(sum_frame));
                sum_frame = sum_frame + (double) input[j];
                printf("[Inside j] Addresses: i %p   j %p   sum_frame %p   input[j] %p   output[i] %p\n", &i, &j, &sum_frame, &input[j], &output[i]);
                printf("[Inside j] j= %d   input[j]: %u.%u   sum_frame: %u.%u\n", j, d1(input[j]), d2(input[j]), d1(sum_frame), d2(sum_frame));
                printf("\n");
            }
            printf("[Inside j] Addresses: i %p   j %p   sum_frame %p   input[j] %p   output[i] %p\n", &i, &j, &sum_frame, &input[j], &output[i]);
            output[i] = paa_size * sum_frame / n;
            printf("[Inside j] Addresses: i %p   j %p   sum_frame %p   input[j] %p   output[i] %p\n", &i, &j, &sum_frame, &input[j], &output[i]);
            printf("[Inside i Af j] i %d   output[i]: %u.%u\n", i, d1(output[i]), d2(output[i]));
            printf("\n");
        }
    }
}

char num2char(float value, float * breakpoints, int n_breakpoints){
    int letter = 0;
    if (value > 0.0){
        letter = n_breakpoints;
        while ((letter > 0) && (breakpoints[letter - 1] > value)){
            letter--;
        }
    } else {
        while ((letter < n_breakpoints) && (breakpoints[letter] <= value)){
            letter++;
        }
    }
    return ALPHABET[letter];
}

void paa2chars(float paa[], char output[], int alphabet_size){
    float * breakpoints;
    int n_breakpoints = alphabet_size - 1;
    switch (alphabet_size){
    case 2:
      breakpoints = case2;
    case 3:
      breakpoints = case3;
    case 4:
      breakpoints = case4;
    case 5:
      breakpoints = case5;
    case 6:
      breakpoints = case6;
    case 7:
      breakpoints = case7;
    case 8:
      breakpoints = case8;
    case 9:
      breakpoints = case9;
    case 10:
      breakpoints = case10;
    default:
        printf("Invalid alphabet_size\n");
    }
    printarrayf(breakpoints, n_breakpoints);
    int n = PAA_SIZE;
    int i;
    for (i = 0; i < n; i++){
        output[i] = num2char(paa[i], breakpoints, n_breakpoints);
    }
}

void sax(struct Timeseries *buffer, char output[], int paa_size, int alphabet_size) {
        printf("znrom\n");
        float znormalised[MEASUREMENT_SIZE];
        znorm(buffer->values, znormalised, buffer->average, buffer->std_dev);
        printarrayf(znormalised, MEASUREMENT_SIZE);

        printf("PAA\n");
        float paa_values[paa_size];
        printf("Addres of paa values %p\n", &paa_values);
        paa(znormalised, paa_values, paa_size);
        printf("Addres of paa values %p\n", &paa_values);
        printarrayf(paa_values, paa_size);

        printf("tochars\n");
        char res[paa_size];
        paa2chars(paa_values, res, alphabet_size);
}

// 1. Normalization
// 2. PAA = divide by windows and get the average of each window
// 3. to letters

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
    // printf("%d\n", TESTT);
    int test[] = {3,3,3,3,3,3,2,2,2,2,2,2};
    buffer = create_buffer();
    printf("Insert\n");
    insert_all_measures_buffer(buffer, test);
    print_buffer(buffer->values);
    calculate_stats_buffer(buffer);
    char res[PAA_SIZE];
    sax(buffer, res, PAA_SIZE, 3);

    PROCESS_EXIT();

    printf("[Measurement] Create buffer\n");
    buffer = create_buffer();
    printf("[Measurement] Created buffer in: %p   1stM: %d   C: %d   S: %d   A: %u.%u   SD: %u.%u   L: %d\n", buffer, buffer->values[buffer->count], buffer->count, buffer->sum, d1(buffer->average), d2(buffer->average), d1(buffer->std_dev), d2(buffer->std_dev), buffer->lock);
    event_data_ready = process_alloc_event();
    etimer_set(&measurement_timer, TIMER_INTERVAL);
    SENSORS_ACTIVATE(light_sensor);

    // while(1) {
        
    //     lock_buffer(buffer);
    //     printf("[Measurement] Start measurement\n");
    //     while(!is_full_buffer(buffer)){
            
    //         PROCESS_WAIT_EVENT_UNTIL(ev=PROCESS_EVENT_TIMER);
            
    //         int current_light = light_sensor.value(LIGHT_SENSOR_PHOTOSYNTHETIC);
    //         insert_measure_buffer(buffer, current_light);

    //         etimer_reset(&measurement_timer);
            
    //     }
    //     calculate_stats_buffer(buffer);

    //     if (buffer->std_dev < VERY_HIGH_STD_DEV_THRESHOLD){
    //         unlock_buffer(buffer);
    //         printf("[Measurement] Std dev of buffer %p is not very high\n", buffer);
    //         printf("[Measurement] Sending buffer to processing thread to aggregate the data\n");
    //         process_post_synch(&aggregation, event_data_ready, &buffer);
    //     } else {
    //         printf("[Measurement] Std dev of buffer %p is very high\n", buffer);
    //         printf("[Measurement] No aggregation is done\n");
    //         print_results(buffer->values, buffer->std_dev, NO_AGGREGATION, NULL);
    //     }
        
    //     reset_buffer(buffer);
    //     unlock_buffer(buffer);

    // }  

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

        struct Timeseries *buffer = data + sizeof(int); // See question 1
        printf("[Processing] Timeseries received\n");
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

        // printf("Aggregate values with aggregation level %d\n", aggregation_level);
        // float aggregation[MEASUREMENT_SIZE / aggregation_level];
        // int i;
        // int aggregation_sum = 0;
        // for(i = 0; i < MEASUREMENT_SIZE; i++){
        //     aggregation_sum += buffer->values[i];
        //     // printf("i: %d   agg_sum: %d   mod: %d\n", i, aggregation_sum, (i + 1) % aggregation_level);
        //     if (i > 0 && (i + 1) % aggregation_level == 0){
        //         aggregation[((i + 1) / aggregation_level) - 1] = (float) aggregation_sum / aggregation_level;
        //         aggregation_sum = 0;
        //     }
        // }
        // print_results(buffer->values, buffer->std_dev, aggregation_level, aggregation);

        printf("Aggregate values with SAX. PAA size: %d\n", PAA_SIZE);
        
        float saxed_ts[PAA_SIZE];
        sax(buffer->values, saxed_ts, PAA_SIZE, ALPHABET_SIZE);
        
        unlock_buffer(buffer);

    }
    PROCESS_END();
}

/*---------------------------------------------------------------------------*/

// 1. Why the pointer in the receiving process is one value less?
// 1. ANSWER: 
// 2. Scheduling the processes: when posting the event the receiver doesn't start
// 2. ANSWER: Cause the kernel didn't have time to switch thread context