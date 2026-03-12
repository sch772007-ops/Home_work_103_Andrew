#include <stdio.h>
#include <stdlib.h>
#include <math.h>
void read_matrix(double *a, int n){
    for (int i=0; i < n; i++){
        for(int j = 0; j < n; j++){
            scanf("%lf", &a[i*n+j]);
        }
    }
}
void swap_rows(double *a, int n, int r1, int r2){
    if (r1 == r2) return;
    for(int j = 0; j<n; j++){
        double tmp = a[r1 * n + j];
        a[r1 * n + j] = a[r2 * n + j];
        a[r2 * n + j] = tmp;
    }
}
double gauss(double *a, int n){
    double det = 1.0;
    for(int col = 0; col < n; col++){
        int p = col;
        double best = fabs(a[col * n +col]);
        for(int row = col+1; row < n; row++){
            double val = fabs(a[row*n + col]);
            if(val > best){
                best = val;
                p = row;
            }
        }
        if(best == 0){      // !!!!!!
            return 0.0;
        }
        if(p != col){
            swap_rows(a, n, p, col);
            det = -1 * det;
        }
        double diag = a[col * n + col];
        for(int row = col + 1; row < n; row++){
            double f = a[row * n + col] / diag;
            for(int j = col; j < n; j++){
                a[row * n + j] -= f * a [col * n + j];
            }
        }
    }
    for(int i = 0; i<n; i++){
        det *= a[i * n + i];
    }
    return det;
}


int main()
{
    int n;
    scanf("%d", &n);
    double *a = (double *)malloc(sizeof(double) * n * n);
    read_matrix(a, n);
    double det;
    det = gauss(a, n);
    free(a);
    printf("%lf", det);
    return 0;
}
