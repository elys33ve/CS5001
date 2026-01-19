#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define RNG 100     // range upper limit

// randomly generate integers for matrices m1 and m1,
// writes data to m1.bin and m2.bin

// TODO: *****fix memory allocation
// TODO: *****header for dimensions

int main() {
    srand(time(NULL));

    int M = 100;
    int N = 100;
    int P = 100;

    int m1[M][N];
    int m2[N][P];

    // generate m1
    for (int i=0; i<M; i++) {
        for (int j=0; j<N; j++) {
            m1[i][j] = (rand() % RNG) + 1;  // range: 1 to 100
        }
    }
    // save m1 to .bin
    FILE *f1 = fopen("m1.bin", "wb");
    if (f1) {
        // write header
        fwrite(&M, sizeof(int), 1, f1);
        fwrite(&N, sizeof(int), 1, f1);
        // write matrix
        fwrite(m1, sizeof(int), M*N, f1);
        fclose(f1);
    }

    // generate m2
    for (int i=0; i<N; i++) {
        for (int j=0; j<P; j++) {
            m2[i][j] = (rand() % RNG) + 1;  // range: 1 to 100
        }
    }
    // save m2 to .bin
    FILE *f2 = fopen("m2.bin", "wb");
    if (f2) {
        // write header
        fwrite(&N, sizeof(int), 1, f2);
        fwrite(&P, sizeof(int), 1, f2);
        // write matrix
        fwrite(m2, sizeof(int), N*P, f2);
        fclose(f2);
    }
    
    return 0;
}
