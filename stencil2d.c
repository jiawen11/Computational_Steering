#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <omp.h>
#include <sys/time.h>
#include <sys/timeb.h>
#include <string.h>

/* 2D/3D stencil computation, take a radius sized coefficient matrix, and apply stencil computation to a matrix
 * The stencil could be cross-based, which only uses neighbors from one dimension stride (A[i-1][j][k], or square-based
 * which use neighbors from multiple dimension stride (A[i-2][j-1][k]).
 */

#define DEFAULT_DIMSIZE 256
#define REAL float
/* use the macro (SQUARE_STENCIL) from compiler to build two versions of the stencil
 * 1. CROSS-based stencil, default, coefficient is an array of 4*radius+1, [0] is the center value, and then row and column values
 * 2. SQUARE-based stencil, coefficient is a square matrix with one dimension of (2*radius+1)
 */

void print_array(char * title, char * name, REAL * A, long n, long m) {
	printf("%s:\n", title);
	long i, j;
    for (i = 0; i < n; i++) {
        for (j = 0; j < m; j++) {
            printf("%s[%d][%d]:%f\n", name, i, j, A[i * m + j]);
        }
        printf("\n");
    }
    printf("\n");
}


void init_array(long N, REAL *A, REAL lower, REAL upper) {
	long i;

	for (i = 0; i < N; i++) {
		A[i] = (REAL)(lower + ((REAL)rand() / (REAL)RAND_MAX) * (upper - lower));
	}
}

REAL check_accdiff(const REAL *output, const REAL *reference, const long dimx, const long dimy, const int radius, REAL tolerance){
	int ix, iy;
	REAL acc_diff = 0.0;
	int count = 0;
	for (ix = -radius ; ix < dimx + radius ; ix++) {
		for (iy = -radius ; iy < dimy + radius ; iy++) {
			if (ix >= 0 && ix < dimx && iy >= 0 && iy < dimy) {
				// Determine the absolute difference
				REAL difference = fabs(*reference - *output);
				acc_diff += difference;

				REAL error;
				// Determine the relative error
				if (*reference != 0)
					error = difference / *reference;
				else
					error = difference;

				// Check the error is within the tolerance
				//printf("Data at point (%d,%d)\t%f instead of %f\n", ix, iy, *output, *reference);
				if (error > tolerance) {
	//				if (count++<16)	printf("Data error at point (%d,%d)\t%f instead of %f\n", ix, iy, *output, *reference);
				}
			}
			++output;
			++reference;
		}
	}
	return acc_diff;
}

void stencil2d_seq(long n, long m, REAL *u, int radius, REAL *filter, int num_its);
void stencil2d_omp(long n, long m, REAL *u, int radius, REAL *coeff, int num_its);

int main(int argc, char * argv[]) {
	long n = DEFAULT_DIMSIZE;
	long m = DEFAULT_DIMSIZE;
	int radius = 3;
	int num_its = 1000;
	int num_threads = 1;
    fprintf(stderr,"Usage: stencil2d [<n> <m> <radius> <num_its>]\n");
    fprintf(stderr, "\tn - grid dimension in x direction, default: %d\n", n);
    fprintf(stderr, "\tm - grid dimension in y direction, default: n if provided or %d\n", m);
    fprintf(stderr, "\tradius - Filter radius, default: %d\n", radius);
    fprintf(stderr, "\tnum_its  - # iterations for iterative solver, default: %d\n", num_its);

    if (argc == 2)      //{ sscanf(argv[1], "%d", &n); m = n; }
	{
		num_threads = atoi(argv[1]);
		omp_set_num_threads(num_threads);
		printf("%d\n",num_threads);
	
	}
    else if (argc == 3) //{ sscanf(argv[1], "%d", &n); sscanf(argv[2], "%d", &m); }
	{
		num_threads = atoi(argv[1]);
                omp_set_num_threads(num_threads);
                printf("%d\n",num_threads);
		radius = atoi(argv[2]);
	}
    //else if (argc == 4) { sscanf(argv[1], "%d", &n); sscanf(argv[2], "%d", &m); sscanf(argv[3], "%d", &radius); }
    //else if (argc == 5) { sscanf(argv[1], "%d", &n); sscanf(argv[2], "%d", &m); sscanf(argv[3], "%d", &radius); sscanf(argv[4], "%d", &num_its); }
    else {
    	/* the rest of arg ignored */
    }

	if (num_its%2==0) num_its++; /* make it odd so uold/u exchange easier */

	long u_dimX = n+radius+radius;
	long u_dimY = m+radius+radius;
	long u_volumn = u_dimX*u_dimY;
	int coeff_volumn;
	coeff_volumn = (2*radius+1)*(2*radius+1); /* this is for square. Even the cross stencil that use only 4*radius +1, we will use the same square coeff simply */
//	coeff_volumn = 4*radius+1;
    REAL * u = (REAL *)malloc(sizeof(REAL)* u_volumn);
	REAL * u_omp = (REAL *)malloc(sizeof(REAL)* u_volumn);
	REAL *coeff = (REAL *) malloc(sizeof(REAL)*coeff_volumn);

	srand(0);
	init_array(u_volumn, u, 0.0, 1.0);
	init_array(coeff_volumn, coeff, 0.0, 1.0);
	memcpy(u_omp, u, sizeof(REAL)*u_volumn);
#if 0
	printf("serial execution\n");
	REAL base_elapsed = omp_get_wtime();
        int i;
        int num_runs = 1;
	for (i=0;i<num_runs;i++) stencil2d_seq(n, m, u, radius, coeff, num_its);
	base_elapsed = ((omp_get_wtime() - base_elapsed))/num_runs*1000;
#endif
	printf("OMP execution\n");
	REAL omp_elapsed = omp_get_wtime();
	int i;
	int num_runs = 1;
	for (i=0;i<num_runs;i++) stencil2d_omp(n, m, u_omp, radius, coeff, num_its);
	omp_elapsed = (omp_get_wtime() - omp_elapsed)/num_runs*1000;

	long flops = n*m*radius;
#ifdef SQUARE_STENCIL
	flops *= 8;
#else
	flops *= 16;
#endif

	printf("======================================================================================================\n");
	printf("\tStencil 2D: %dx%d, stencil radius: %d, #iteratins: %d\n", n, m, radius, num_its);
	printf("------------------------------------------------------------------------------------------------------\n");
	printf("Performance:\t\tRuntime (ms)\t MFLOPS \t\tError (compared to base)\n");
	printf("------------------------------------------------------------------------------------------------------\n");
	//printf("base:\t\t%4f\t%4f \t\t%g\n", base_elapsed, flops / (1.0e-3 * base_elapsed), 0.0); //check_accdiff(u, u, u_dimX, u_dimY, radius, 1.0e-7));
	printf("omp: \t\t%4f\t%4f \t\t%g\n", omp_elapsed, flops / (1.0e-3 * omp_elapsed), check_accdiff(u, u_omp, n, m, radius, 0.00001f));

	free(u);
	free(u_omp);
	free(coeff);

	return 0;
}

void stencil2d_seq(long n, long m, REAL *u, int radius, REAL *coeff, int num_its) {
	long it; /* iteration */
	long u_dimX = n + 2 * radius;
	long u_dimY = m + 2 * radius;
	int coeff_dimX = 2*radius+1;
	REAL *uold = (REAL*)malloc(sizeof(REAL)*u_dimX * u_dimY);
	memcpy(uold, u, sizeof(REAL)*u_dimX*u_dimY);
	coeff = coeff + coeff_dimX * radius + radius; /* let coeff point to the center element */
	REAL * uold_save = uold;
	REAL * u_save = u;
	int count = 4*radius+1;
#ifdef SQUARE_STENCIL
	count = coeff_dimX * coeff_dimX;
#endif

	for (it = 0; it < num_its; it++) {
		int ix, iy, ir;

		for (ix = 0; ix < n; ix++) {
			for (iy = 0; iy < m; iy++) {
				int offset = (ix+radius)*u_dimY+radius+iy;
				REAL * temp_u = &u[offset];
				REAL * temp_uold = &uold[offset];
				REAL result = temp_uold[0] * coeff[0];
				/* 2/4 way loop unrolling */
				for (ir = 1; ir <= radius; ir++) {
					result += coeff[ir] * temp_uold[ir];           		//horizontal right
					result += coeff[-ir]* temp_uold[-ir];                  // horizontal left
					result += coeff[-ir*coeff_dimX] * temp_uold[-ir * u_dimY]; //vertical up
					result += coeff[ir*coeff_dimX] * temp_uold[ir * u_dimY]; // vertical bottom
#ifdef SQUARE_STENCIL
					result += coeff[-ir*coeff_dimX-ir] * temp_uold[-ir * u_dimY-ir]; // left upper corner
					result += coeff[-ir*coeff_dimX+ir] * temp_uold[-ir * u_dimY+ir]; // right upper corner
					result += coeff[ir*coeff_dimX-ir] * temp_uold[ir * u_dimY]-ir]; // left bottom corner
					result += coeff[ir*coeff_dimX+ir] * temp_uold[ir * u_dimY]+ir]; // right bottom corner
#endif
				}
				*temp_u = result/count;
			}
		}
		REAL * tmp = uold;
		uold = u;
		u = tmp;
//		if (it % 500 == 0)
//			printf("Finished %d iteration\n", it);
	} /*  End iteration loop */
	free(uold_save);
}

void stencil2d_omp(long n, long m, REAL *u, int radius, REAL *coeff, int num_its) {
	long it; /* iteration */
	long u_dimX = n + 2 * radius;
	long u_dimY = m + 2 * radius;
	int coeff_dimX = 2 * radius + 1;
	REAL *uold = (REAL *) malloc(sizeof(REAL) * u_dimX * u_dimY);
	memcpy(uold, u, sizeof(REAL)*u_dimX*u_dimY);
	coeff = coeff + (2 * radius + 1) * radius + radius; /* let coeff point to the center element */
	int count = 4*radius+1;
#ifdef SQUARE_STENCIL
	count = coeff_dimX * coeff_dimX;
#endif

	REAL * uold_save = uold;
	REAL * u_save = u;
#pragma omp parallel shared(n, m, radius, coeff, num_its, u_dimX, u_dimY, coeff_dimX, count) firstprivate(u, uold) private(it)
	{
		int ix, iy, ir;
		for (it = 0; it < num_its; it++) {
#pragma omp for
			for (ix = 0; ix < n; ix++) {
				REAL *temp_u = &u[(ix + radius) * u_dimY+radius];
				REAL *temp_uold = &uold[(ix + radius) * u_dimY+radius];
				for (iy = 0; iy < m; iy++) {
					REAL result = temp_uold[0] * coeff[0];
					/* 2/4 way loop unrolling */
					for (ir = 1; ir <= radius; ir++) {
						result += coeff[ir] * temp_uold[ir];                //horizontal right
						result += coeff[-ir] * temp_uold[-ir];                  // horizontal left
						result += coeff[-ir * coeff_dimX] * temp_uold[-ir * u_dimY]; //vertical up
						result += coeff[ir * coeff_dimX] * temp_uold[ir * u_dimY]; // vertical bottom
#ifdef SQUARE_STENCIL
						result += coeff[-ir*coeff_dimX-ir] * temp_uold[-ir * u_dimY-ir] // left upper corner
						result += coeff[-ir*coeff_dimX+ir] * temp_uold[-ir * u_dimY+ir] // right upper corner
						result += coeff[ir*coeff_dimX-ir] * temp_uold[ir * u_dimY]-ir] // left bottom corner
						result += coeff[ir*coeff_dimX+ir] * temp_uold[ir * u_dimY]+ir] // right bottom corner
#endif
					}
					*temp_u = result/count;
					temp_u++;
					temp_uold++;
				}
			}
			REAL *tmp = uold;
			uold = u;
			u = tmp;
//		if (it % 500 == 0)
//			printf("Finished %d iteration by thread %d of %d\n", it, omp_get_thread_num(), omp_get_num_threads());
		} /*  End iteration loop */
	}
	free(uold_save);
}

