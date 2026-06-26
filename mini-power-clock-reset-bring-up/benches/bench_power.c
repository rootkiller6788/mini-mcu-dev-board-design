#include "../include/power_design.h"
#include <stdio.h>
#include <time.h>
int main(void) {
    decoupling_cap_t caps[4] = {
        {10e-6,0.01,1e-9,1.6e6,6.3,"X7R","0805"},
        {1e-6,0.02,0.8e-9,5.6e6,10.0,"X7R","0603"},
        {100e-9,0.05,0.5e-9,22e6,16.0,"X7R","0402"},
        {10e-9,0.1,0.3e-9,100e6,25.0,"NP0","0201"}
    };
    double freqs[100], z_out[100];
    for(int i=0;i<100;i++) freqs[i]=1e3*pow(10,4.0*i/99.0);
    clock_t start = clock();
    for(int iter=0;iter<10000;iter++)
        decoupling_impedance_sweep(caps,4,freqs,100,z_out);
    clock_t end = clock();
    double ms = (double)(end-start)*1000.0/CLOCKS_PER_SEC;
    printf("PDN sweep bench: 10k x 100pts x 4caps = %.1f ms\n", ms);
    printf("Per iter: %.3f us\n", ms*1000/10000);
    return 0;
}
