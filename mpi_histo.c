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


/*
 * This function computes the bin_count for the histogram for the given local data.
 *
 * */

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

int main(int argc, char *argv[]) {

    float *data, *bin_max;
    float *loc_data;
    int comm_sz, rank;
    float min_meas, max_meas, bin_width;
    long bin_count, data_count;
    long data_per_process, *histogram, *loc_histogram;
    double start_total, finish_total;
    double start_app, finish_app;
    double start_histo, final_histo;

    MPI_Init(NULL, NULL);
    MPI_Comm_size(MPI_COMM_WORLD, &comm_sz);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    // Process 0 will take inputs from the user create data and distribute the data to other processes.
    if (rank == 0) {
        // To determine the total execution time for histogram computation.
        start_total = MPI_Wtime();
//        printf("\nEnter values for: <bin_count> <min_meas> <max_meas> <data_count>: \n ");
        printf("\nStarting computation for histogram... \n");
        printf("\nTotal number of processes: %d... \n", comm_sz);

        // Input from command line arguments.
        bin_count = strtol(argv[1], NULL, 10);
        min_meas = strtof(argv[2], NULL);
        max_meas = strtof(argv[3], NULL);
        data_count = strtol(argv[4], NULL, 10);

        // Allocate memory for data array and threads.
        data = malloc(data_count * sizeof(float));
        bin_max = malloc(sizeof(float) * bin_count); // Global bin_max for histogram
        histogram = malloc(sizeof(long) * bin_count); // Global count for histogram

        // Generate random data
        printf("\nGenerating data");
        generate_data(data, data_count, min_meas, max_meas);
        // Calculate bin width and determine bin_max values. This bin_max will be shared to all the process.
        bin_width = (max_meas - min_meas) / (float) bin_count;
        float prev_bin_max = min_meas;
        for (int i = 0; i < bin_count; i++) {
            prev_bin_max = prev_bin_max + bin_width;
            bin_max[i] = prev_bin_max;
        }

        // Determine data to be distributed to al the processes
        data_per_process = data_count / comm_sz;
        loc_data = malloc(sizeof(float) * data_per_process);

        // We will use MPI scatterv to distribute the data to all the process at once.
        // For this we need displacements and the number of data to be shared to all the processes.
        int counts[comm_sz];
        int displacements[comm_sz];
        start_app = MPI_Wtime();
        // Send equal amount of data to all the process.
        // If the total data cannot be divided equally to all the process, send the remaining data to the last process.
        for (int i = 0; i < comm_sz; i++) {
            counts[i] = data_per_process;
            displacements[i] = i * data_per_process;
        }
        // Send the remaining data to last process.
        counts[comm_sz - 1] = data_count - (comm_sz - 1) * data_per_process;
        // Bcast meta data to other processes. Processes use this data to determine what amaount of data is to be received from process 0.
        MPI_Bcast(&data_count, 1, MPI_LONG, 0, MPI_COMM_WORLD);
        MPI_Bcast(&bin_count, 1, MPI_LONG, 0, MPI_COMM_WORLD);
        // Scatter our data to all the process.
        MPI_Scatterv(data, counts, displacements, MPI_FLOAT, loc_data, (int) data_per_process, MPI_FLOAT, 0,
                     MPI_COMM_WORLD);
        free(data);
    }

    if (rank != 0) {
        // Receive broadcasted information and calculate the amount of data to be received from the process 0.
        MPI_Bcast(&data_count, 1, MPI_LONG, 0, MPI_COMM_WORLD);
        MPI_Bcast(&bin_count, 1, MPI_LONG, 0, MPI_COMM_WORLD);
        data_per_process = data_count / comm_sz;
        if (rank == comm_sz - 1) {
            // Remaining data should be handled by the last process.
            data_per_process = data_count - ((comm_sz - 1) * data_per_process);
        }

        bin_max = malloc(sizeof(float) * bin_count); // Global bin_max for histogram
        loc_data = malloc(sizeof(float) * data_per_process); // Initalize local data to ve received by the other processes.
        MPI_Scatterv(NULL, NULL, NULL, MPI_FLOAT, loc_data, (int) data_per_process, MPI_FLOAT, 0, // Receive data.
                     MPI_COMM_WORLD);
    }
    // Bcast bin_max to compute histogram.
    MPI_Bcast(bin_max, (int) bin_count, MPI_FLOAT, 0, MPI_COMM_WORLD);
    loc_histogram = malloc(sizeof(long) * bin_count);
    // For timing report.
    if(rank == 0)
        start_histo = MPI_Wtime();
    // At thi spoint all the data is distributed across the processes. Now compute the histogram from the distributed data and send the data to process 0.
    // Process 0 will compute the final histogram of the program.
    compute_histogram(bin_count, data_per_process, loc_data, bin_max, loc_histogram);
    if(rank == 0){
        final_histo = MPI_Wtime();
        printf("\nLocal histogram elapsed: %f", final_histo - start_histo);
    }
    // Send data back to process 0 and perform reduction operation to compute final histogram.
    MPI_Reduce(loc_histogram, histogram, (int) bin_count, MPI_LONG, MPI_SUM, 0, MPI_COMM_WORLD);

    // Process 0 prints the output.
    if (rank == 0) {
        // Print output values.
        long total_count = 0;
        printf("\nbin_maxes = %ld", bin_count);
        for (int i = 0; i < bin_count; i++) {
            printf("%.3f ", bin_max[i]);
        }
        printf("\nbin_count = ");
        for (int i = 0; i < bin_count; i++) {
            total_count += histogram[i];
           printf("%ld ", (long) histogram[i]);
        }
        printf("\nTotal histogram computed: %ld", total_count);
    }
    free(bin_max);
    free(loc_histogram);
    free(loc_data);
    // Process 0 is tracking the timing reports.
    if(rank == 0){
        finish_total = MPI_Wtime();
        finish_app = finish_total;
        printf("\nTotal time elapsed: %f", finish_total - start_total);
        printf("\nApplication time elapsed: %f", finish_app - start_app);
    }
    MPI_Finalize();
    printf("\n");
    return 0;
}