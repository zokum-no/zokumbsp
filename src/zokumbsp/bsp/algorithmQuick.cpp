#include "constants.hpp"
#include "level.hpp"
#include "zennode.hpp"
#include "logger.hpp"
#include "bspcommon.hpp"
#include "bspgeometry.hpp"

#include "algorithmQuick.hpp"

int AlgorithmQuick(SEG* segs, int noSegs, nodeBuilderData* nbd) {

	sBSPOptions* options = nbd->options;
	DoomLevel* level = nbd->level;


	FUNCTION_ENTRY(NULL, "AlgorithmQuick", true);

	SEG* pSeg = NULL, * testSeg = segs;
	int count[3];
	int& lCount = count[0], & sCount = count[1], & rCount = count[2];
	// Compute the maximum value maxMetric can possibly reach
	long maxMetric = (long)(noSegs / 2) * (long)(noSegs - noSegs / 2);
	long bestMetric = LONG_MIN, bestSplits = LONG_MAX;

	int i = 0, max = (noSegs < 30) ? noSegs : 30;

	int bestSeg = -2;

retry:

	for (; i < max; i++) {
		// if ( showProgress && (( i & 15 ) == 0 )) ShowProgress (); obsolete, spammy
		// int alias = testSeg->Split ? 0 : lineDefAlias [ testSeg->Data.lineDef ];
		int alias = testSeg->flags & SEG_SPLIT ? 0 : bsp.lineDefAlias[testSeg->DatalineDef];

		if ((alias == 0) || (bsp.lineChecked[alias] == false)) {
			bsp.lineChecked[alias] = -1;
			count[0] = count[1] = count[2] = 0;

			//ComputeStaticVariables ( testSeg );
			ComputeStaticVariables(segs, i);

			if ((fabs(bsp.DX) < EPSILON) && (fabs(bsp.DY) < EPSILON)) goto next;
			if (bestMetric < 0) for (int j = 0; j < noSegs; j++) {
				count[WhichSide(&segs[j], options, level) + 1]++;
			}
			else for (int j = 0; j < noSegs; j++) {
				count[WhichSide(&segs[j], options, level) + 1]++;
				if (sCount > bestSplits) goto next;
			}
			if (lCount * rCount + sCount) {
				long metric = (long)lCount * (long)rCount;
				if (sCount) {
					long temp = bsp.X1 * sCount;
					if (bsp.X2 < temp) metric = bsp.X2 * metric / temp;
					metric -= (bsp.X3 * sCount + bsp.X4) * sCount;
				}
				if (bsp.ANGLE & 0x3FFF) {
					metric--;
					// printf("red\n");
				}
				/*if ( bsp.ANGLE & 0x7FFF ) metric--;
				  if ( bsp.ANGLE & 0xAFFF ) metric--;*/

				  //if ( metric == maxMetric ) return testSeg;
				if (metric == maxMetric) {
					return i;
				}

				if (metric > bestMetric) {
					bestSeg = i;
					pSeg = testSeg;
					bestSplits = sCount;
					bestMetric = metric;
				}
			}
			else if (alias != 0) {
				// Eliminate outer edges of the map from here & down
				*bsp.convexPtr++ = alias;
			}
		}
#if defined ( DEBUG )
		else if (lineChecked[alias] > 0) {
			count[0] = count[1] = count[2] = 0;
			ComputeStaticVariables(testSeg);
			int side = WhichSide(testSeg, options, level);
			if ((fabs(bsp.DX) < EPSILON) && (fabs(bsp.DY) < EPSILON)) continue;
			for (int j = 0; j < noSegs; j++) {
				switch (WhichSide(&segs[j], options, level)) {
				case SIDE_LEFT:
					if (side == SIDE_RIGHT) {
						WARNING("lineDef " << segs[j].DatalineDef << " should not to the left of lineDef " << testSeg->DatalineDef);
					}
					break;
				case SIDE_SPLIT:
					WARNING("lineDef " << segs[j].DatalineDef << " should not be split by lineDef " << testSeg->DatalineDef);
					break;
				case SIDE_RIGHT:
					if (side == SIDE_LEFT) {
						WARNING("lineDef " << segs[j].DatalineDef << " should not to the right of lineDef " << testSeg->DatalineDef);
					}
					break;
				default:
					break;
				}
			}
		}
#endif

	next:
		testSeg++;
	}

	if ((pSeg == NULL) && (max < noSegs)) {
		max += 5;
		if (max > noSegs) max = noSegs;
		goto retry;
	}

	return bestSeg;
	// return pSeg;
}