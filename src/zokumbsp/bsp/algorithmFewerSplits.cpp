#include "constants.hpp"
#include "level.hpp"
#include "zennode.hpp"
#include "logger.hpp"
#include "bspcommon.hpp"
#include "bspgeometry.hpp"

int AlgorithmFewerSplits(SEG* segs, int noSegs, nodeBuilderData* nbd) {

	sBSPOptions* options = nbd->options;
	DoomLevel* level = nbd->level;


	FUNCTION_ENTRY(NULL, "AlgorithmFewerSplits", true);

	SEG /**pSeg = NULL,*/* testSeg = segs;
	int count[3];
	int& lCount = count[0], & sCount = count[1], & rCount = count[2];
	// Compute the maximum value maxMetric can possibly reach
	long maxMetric = (noSegs / 2) * (noSegs - noSegs / 2);
	long bestMetric = LONG_MIN, bestSplits = LONG_MAX;

	bool diagonal;

	int edgeBalance = noSegs;
	int bestEdgeBalance = noSegs;

	int bestOffset = -2;

	for (int i = 0; i < noSegs; i++) {
		// if ( showProgress && (( i & 15 ) == 0 )) ShowProgress (); obsolete, spammy
		// int alias = testSeg->Split ? 0 : lineDefAlias [ testSeg->DatalineDef ];

		int alias = (testSeg->flags & SEG_SPLIT) ? 0 : bsp.lineDefAlias[testSeg->DatalineDef];

		if ((alias == 0) || (bsp.lineChecked[alias] == false)) {
			bsp.lineChecked[alias] = -1;
			count[0] = count[1] = count[2] = 0;
			// ComputeStaticVariables ( testSeg );

			ComputeStaticVariables(segs, i);

			if ((fabs(bsp.DX) < EPSILON) && (fabs(bsp.DY) < EPSILON)) goto next;
			if (bestMetric < 0) for (int j = 0; j < noSegs; j++) {
				count[WhichSide(&segs[j], options, level) + 1]++;
			}
			else for (int j = 0; j < noSegs; j++) {
				count[WhichSide(&segs[j], options, level) + 1]++;
				if (sCount >= bestSplits) goto next; // Added =
			}

			// Only consider SEG if it is not a boundary line
			if (lCount * rCount + sCount != 0) {
				long metric = (long)lCount * (long)rCount;
				if (sCount) {
					long temp = bsp.X1 * sCount;
					if (bsp.X2 < temp) metric = bsp.X2 * metric / temp;
					metric -= (bsp.X3 * sCount + bsp.X4) * sCount;
				}

				if ((bsp.ANGLE & 0x0000) || (bsp.ANGLE & 0x3FFF) || (bsp.ANGLE & 0x7FFF) || (bsp.ANGLE & 0xAFFF)) {
					// metric--;
					diagonal = false;
				}
				else {
					diagonal = true;
				}

				if (metric == maxMetric) {
					// return testSeg;
					return i;
				}

				bool betterBalance;
				bool decentBalance;

				edgeBalance = abs(lCount - rCount);

				if (edgeBalance < bestEdgeBalance) { // children have more equal amount of edges
					betterBalance = true;
					decentBalance = true;
				}
				else if (edgeBalance == bestEdgeBalance) { // same amount, maybe not diagonal?
					decentBalance = true;
				}
				else { // less balanced than our previous best
					betterBalance = false;
					decentBalance = false;
				}

				if ((metric > bestMetric)
					|| ((metric == bestMetric) && betterBalance)
					|| ((metric == bestMetric) && !diagonal && decentBalance)
					) {
					bestOffset = i;
					// pSeg       = testSeg;
					bestSplits = sCount + 2;
					bestMetric = metric;
					bestEdgeBalance = edgeBalance;
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
			int side = WhichSide(testSeg, options);
			if ((fabs(bsp.DX) < EPSILON) && (fabs(bsp.DY) < EPSILON)) continue;
			for (int j = 0; j < noSegs; j++) {
				switch (WhichSide(&segs[j], options)) {
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
						WARNING("lineDef " << segs[j].DatalineDef << " should not to the right of lineDef " << testSeg->Data.lineDef);
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

	// return pSeg;
	return bestOffset;
}