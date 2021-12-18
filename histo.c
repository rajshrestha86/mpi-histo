#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>

#define SEED 100


/* This function generates given number of random data
 * between min and max measurements.
*/
void generate_data(float *arr, long count, float min, float max) {
    // Check if min and max value are valid.
    if (max < min) {
        printf("The value of min should be less than max value....\nExiting Program.\n");
        exit(1);
    }
    // Generate upto count number of random data.
    // The random data is stored in arr (float array declared in main function)
    for (int i = 0; i < count; i++) {
        // Generate random value between 0 to 1.
        float random_number = ((float) rand() / (float) RAND_MAX);
        // Scale 0 to 1 between min and max.
        arr[i] = min + random_number * (max - min);
    }
}

void compute_histogram(long bin_count, long data_per_process, const float *loc_data, const float *loc_bin_max,
                       long *loc_bin_sum) {
    for (int i = 0; i < bin_count; i++)
        loc_bin_sum[i] = 0;

    for (int i = 0; i < data_per_process; i++) {
        float data_value = loc_data[i];
        for (int j = 0; j < bin_count; j++) {
            // If the data is less than bin_max it should be included in the count.
            if (data_value <= loc_bin_max[j]) {
                loc_bin_sum[j] += 1;
                break;
            }

        }
    }

}

int main() {

    int *data;
    int comm_sz, rank;

    MPI_Init(NULL, NULL);
    MPI_Comm_size(MPI_COMM_WORLD, &comm_sz);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    // Process 0 will take inputs from the user.
    if (rank == 0) {
        float *data, *bin_max;
        long bin_count, data_count;
        float min_meas, max_meas, bin_width;
        long data_per_process, *histogram;
        printf("\nEnter values for: <bin_count> <min_meas> <max_meas> <data_count>: \n ");
        scanf("%ld", &bin_count);
        scanf("%f", &min_meas);
        scanf("%f", &max_meas);
        scanf("%ld", &data_count);

        // Allocate memory for data array and threads.
        data = malloc(data_count * sizeof(float));
        bin_max = malloc(sizeof(float) * bin_count); // Global bin_max for histogram
        histogram = malloc(sizeof(long) * bin_count); // Global count for histogram

        // Generate random data
        generate_data(data, data_count, min_meas, max_meas);
//        histogram = malloc(sizeof(long) * bin_count); // Global count for histogram
        bin_width = (max_meas - min_meas) / (float) bin_count;
        data_per_process = data_count / comm_sz;

//        for (int i = 0; i < data_count; i++) {
//            printf("\nIndex: %d, Data: %.2f", i, data[i]);
//        }

        // Populate bin_max array
        // Using bin width to create bin max for each bin.
        float prev_bin_max = min_meas;
        for (int i = 0; i < bin_count; i++) {
            prev_bin_max = prev_bin_max + bin_width;
            bin_max[i] = prev_bin_max;
        }

        // Let's send the data to other processes
        float *data_for_process = malloc(sizeof(float) * data_per_process);

//        MPI_Bcast(&bin_count, 1, MPI_LONG, 0, MPI_COMM_WORLD);
//        MPI_Scatter(data, data_count, MPI_FLOAT, );



        for (int i = 1; i < comm_sz; i++) {
            // Slice array and send it to another process.
            long data_len_for_i = data_per_process;
            // We will send all the remaining data to last process.
            if (i == (comm_sz - 1)) {
                data_len_for_i = data_count - data_per_process * (comm_sz - 1);
                free(data_for_process);
                data_for_process = malloc(sizeof(float) * data_len_for_i);
            }

            for (int j = 0; j < data_len_for_i; j++) {
                data_for_process[j] = data[i * data_per_process + j];
            }
            MPI_Send(&data_len_for_i, 1, MPI_LONG, i, 0, MPI_COMM_WORLD);
            MPI_Send(&bin_count, 1, MPI_LONG, i, 1, MPI_COMM_WORLD);
            MPI_Send(data_for_process, (int) data_len_for_i, MPI_FLOAT, i, 2, MPI_COMM_WORLD);
            MPI_Send(bin_max, (int) bin_count, MPI_FLOAT, i, 3, MPI_COMM_WORLD);
        }

        // Process 0 will also compute histogram
        compute_histogram(bin_count, data_per_process, data, bin_max, histogram);
        // After sending all data, receive bin count from respective processes...

        long *hist_from_process = malloc(sizeof(long ) * bin_count);
        for (int i = 1; i < comm_sz; i++) {
            MPI_Recv(hist_from_process, (int) bin_count, MPI_LONG, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            for (int j = 0; j < bin_count; j++) {
                histogram[j] += hist_from_process[j];
            }
        }

        // Print output values.
        long total_data_in_hist = 0;
        printf("\nbin_maxes = ");
        for (int i = 0; i < bin_count; i++) {
            printf("%.3f ", bin_max[i]);
        }
        printf("\nbin_count = ");
        for (int i = 0; i < bin_count; i++) {
            printf("%ld ", (long) histogram[i]);
            total_data_in_hist += histogram[i];
        }


        printf("\nHere are the values: %ld %f %f %ld", bin_count, min_meas, max_meas, data_count);
    } else {
        printf("I am another process....");
        // Receive number of data to process
        long data_per_process, bin_count;
        MPI_Recv(&data_per_process, 1, MPI_LONG, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Recv(&bin_count, 1, MPI_LONG, 0, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        printf("\nReceived message for data_per_process from process 0. My rank and data_count is : %d %ld\n", rank,
               data_per_process);

        // Receive total data
        float *loc_data, *loc_bin_max;
        long *loc_bin_sum;
        loc_data = malloc(data_per_process * sizeof(float));
        loc_bin_max = malloc(bin_count * sizeof(float));
        loc_bin_sum = malloc(bin_count * sizeof(long ));

        MPI_Recv(loc_data, (int) data_per_process, MPI_FLOAT, 0, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Recv(loc_bin_max, (int) bin_count, MPI_FLOAT, 0, 3, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        printf("******* Here ********");
        compute_histogram(bin_count, data_per_process, loc_data, loc_bin_max, loc_bin_sum);
        MPI_Send(loc_bin_sum, (int) bin_count, MPI_LONG, 0, 0, MPI_COMM_WORLD);

//        for (int i = 0; i < data_per_process; i++) {
//            printf("\nProcess: %d, Index: %d, Data: %.2f", rank, i, loc_data[i]);
//        }



    }


    MPI_Finalize();

    return 0;
}