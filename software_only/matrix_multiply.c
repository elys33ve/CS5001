#include <stdio.h>
#include <stdlib.h>
#include <time.h>

//#define M 100     // rows of M1
//#define N 100     // cols of M1 / rows of M2
//#define P 100     // cols of M2

int main() {
    struct timespec start, end;
    int M, N, P;


    // ======= read matrixes from bin files
    // read m1.bin
    FILE *f1 = fopen("m1.bin", "rb");
    if (!f1) { 
        perror("cannot read m1.bin");
        return 1;
    }
    // read dimensions
    fread(&M, sizeof(int), 1, f1);
    fread(&N, sizeof(int), 1, f1);
    // read matrix
    int *m1 = malloc(M * N * sizeof(int));
    fread(m1, sizeof(int), M*N, f1);
    fclose(f1);

    // read m2.bin
    FILE *f2 = fopen("m2.bin", "rb");
    if (!f2) {
        perror("cannot read m2.bin");
        return 1;
    }
    // read dimensions
    fread(&N, sizeof(int), 1, f2);
    fread(&P, sizeof(int), 1, f2);
    // read matrix
    int *m2 = malloc(N * P * sizeof(int));
    fread(m2, sizeof(int), N*P, f2);
    fclose(f2);
    
    // initialize result matrix
    int *m3 = calloc(M * P, sizeof(int));
 

    // ======== matrix multiply and record time
    // --- start timer
    clock_gettime(CLOCK_MONOTONIC, &start);
    // matrix multiply
    for (int i=0; i<M; i++) {           // row of M1
        for (int j=0; j<P; j++) {       // col of M2
            for (int k=0; k<N; k++) {   // common dimension
                // [row * total_cols + col]
                m3[i * P + j] += m1[i * N + k] * m2[k * P + j];
            }
        }
    }
    // --- end timer
    clock_gettime(CLOCK_MONOTONIC, &end);
    double time_spent = (end.tv_sec - start.tv_sec) * 1000000000.0 + 
                        (end.tv_nsec - start.tv_nsec);
    
    // deallocate arrays
    free(m1); free(m2); free(m3);

    // print results
    printf("%dx%d * %dx%d\n", M,N,N,P);
    printf("Execution time: %f ns\n", time_spent);

    return 0;
}
