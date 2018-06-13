#include <memory.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define M_PI                3.14159265358979323846

unsigned int ComputeAngle(int dx, int dy) {
        double w;

        w = (atan2( (double) dy , (double) dx) * (double)(65536/(M_PI*2)));

        if(w<0) w = (double)65536+w;

        return (unsigned) w;
}

int main ( int argc, const char *argv [] ) {

	if (argc < 6) {
		printf("Expected at least 6 arguments, startX startY endY endY pointX pointY\n");
		return 1;
	} else {
		double sx = atoi(argv[1]);
		double sy = atoi(argv[2]);
		double ex = sx + atoi(argv[3]);
		double ey = sy + atoi(argv[4]);
		double px; // = atio(argv[5]);
		double py; // = atio(argv[6]);

		double dist;

		int points = (argc - 4) / 2;

		printf("Line from %4.0f,%-4.0f to %4.0f,%-4.0f\n", sx, sy, ex, ey);

		for (int p = 0; p != points; p++) {
			px = atoi(argv[(p * 2)+ 5]);
			py = atoi(argv[(p * 2)+ 6]);
			
			double upper = abs(((ey - sy) * px) - ((ex -sx) * py) + (ex * sy) - (ey * sx));
			double below = sqrt( pow(ey - sy, 2) + pow(ex - sx, 2));

			dist = abs(upper / below);

			printf("Distance from point #%-2d (%5.0f,%-5.0f) to line: %10.8f", p, px, py, dist);


			if (dist > 0.5) {
				printf(" ! Bad rounding, should have inserted a seg!");
			} else 	if (dist > 0.3) {
				printf(" ! Possible slime trail!");
			} else if (dist > 0.1) {
				printf(" - Doubtful sime tail.");
			} else {
				printf(" - All good!");
			}
			
			printf("\n");			


		}
	}
}
