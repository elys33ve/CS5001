#include <stdio.h>
#include <time.h>

#define M 3     // rows of M1
#define N 3     // cols of M1 / rows of M2
#define P 3     // cols of M2

int main() {
    struct timespec start, end;

    int m1[M][N];
    int m2[N][P];
    int m3[M][P];

    // ======= read matrixes from bin files
    // read m1.bin
    FILE *f1_in = fopen("m1.bin", "rb");
    if (f1_in) {
        fread(m1, sizeof(int), M*N, f1_in);
        fclose(f1_in);
    }
    // read m2.bin
    FILE *f2_in = fopen("m2.bin", "rb");
    if (f2_in) {
        fread(m2, sizeof(int), N*P, f2_in);
        fclose(f2_in);
    }

    // initialize result matrix
    for (int i=0; i<M; i++) {
        for (int j=0; j<P; j++) {
            m3[i][j] = 0;
        }
    }
 
    // ======== matrix multiply and record time
    // --- start timer
    clock_gettime(CLOCK_MONOTONIC, &start);
    // matrix multiply
    for (int i=0; i<M; i++) {           // row of M1
        for (int j=0; j<P; j++) {       // col of M2
            for (int k=0; k<N; k++) {   // common dimension
                m3[i][j] += m1[i][k] * m2[k][j];
            }
        }
    }
    // --- end timer
    clock_gettime(CLOCK_MONOTONIC, &end);
    double time_spent = (end.tv_sec - start.tv_sec) * 1000000000.0 + 
                        (end.tv_nsec - start.tv_nsec);
    
    // print results
    printf("Execution time: %f ns\n", time_spent);

    return 0;
}







