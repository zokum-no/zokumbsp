#include "constants.hpp"
#include "level.hpp"
#include "zennode.hpp"
#include "logger.hpp"
#include "bspcommon.hpp"
#include "bspgeometry.hpp"

//----------------------------------------------------------------------------
//  ALGORITHM 2: 'ZenNode Quality'
//    This is the 2nd algorithm used by ZenNode.  It attempts to keep the
//    resulting BSP tree balanced based on the number of sectors on each side of
//    the partition line in addition to the number of SEGs.  This seems more
//    reasonable since a given SECTOR is usually made up of one or more SSECTORS.
//----------------------------------------------------------------------------

int AlgorithmBalancedTree(SEG* segs, int noSegs, nodeBuilderData* nbd) {

	sBSPOptions* options = nbd->options;
	DoomLevel* level = nbd->level;
	int* width = &nbd->width;
	std::vector<int> wideSegs = nbd->wideSegs;
	// int *picks = nbd->maxPicks;

	nbd->goodScoresFound = 0;


	FUNCTION_ENTRY(NULL, "AlgorithmBalancedTree", true);

	/*
		if (wideSegs) {

		   int check = wideSegs[0];

		   if (check != -1) {
		   int widthCopy = *width;

		   if (*width < (options->Width - 1)) {
		   if (wideSegs[*width + 1] == -1) {
		 *width = options->Width + 1;
		 }
		 }
		 printf("\nReturning %d %d\n", check, wideSegs[widthCopy]);

		// return &segs [wideSegs[widthCopy]];

		} else {
		// printf("check failed\n");
		}
		}
		*/

	int quality[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

	SEG* testSeg = segs;
	int count[3], noScores = 0, rank, i;
	int& lCount = count[0], & sCount = count[1], & rCount = count[2];

	// This is a pointless memset, we should run without it!
	/*	if (noAliases < noSegs) {
			memset ( score, -1, sizeof ( sScoreInfo ) * noAliases );
		} else {
			memset(  score, -1, sizeof ( sScoreInfo ) * noAliases );
		}
	*/
	bsp.score[0].index = 0;


	// stupid test
	//for (int k = 0; k < noSegs; k++){

	// stupid fix

	// *testSeg = segs;


	for (i = 0; i < noSegs; i++) {
		// if ( showProgress && (( i & 15 ) == 0 )) ShowProgress (); // obsolete, spammy
		// int alias = testSeg->Split ? 0 : lineDefAlias [ testSeg->Data.lineDef ];
		// int alias = testSeg->flags & SEG_SPLIT ? 0 : lineDefAlias [ testSeg->DatalineDef ];
/*
		if (segs[i].flags & SEG_EDGE) {
			// printf("worked\n");
			continue;
		}
*/
		int alias = testSeg->flags & SEG_SPLIT ? 0 : bsp.lineDefAlias[testSeg->DatalineDef];

		if ((alias == 0) || (bsp.lineChecked[alias] == false)) {

			bsp.lineChecked[alias] = -1;
			count[0] = count[1] = count[2] = 0;

			// ComputeStaticVariables ( testSeg );

			// printf("AB tree nosegs=%d, i=%d\n", noSegs, i);
			ComputeStaticVariables(segs, i);

			if ((fabs(bsp.DX) < EPSILON) && (fabs(bsp.DY) < EPSILON)) goto next;
			sScoreInfo* curScore = &bsp.score[noScores];
			curScore->invalid = 0;
			memset(bsp.usedSector, 0, sizeof(UINT8) * bsp.sectorCount);
			SEG* destSeg = segs;
			for (int j = 0; j < noSegs; j++, destSeg++) {
				// printf ("%d %d\n", j, noSegs);
				switch (WhichSide(destSeg, options, level)) {
				case SIDE_LEFT:
					lCount++; bsp.usedSector[destSeg->Sector] |= 0xF0;
					break;
					//case SIDE_SPLIT : if ( destSeg->DontSplit ) curScore->invalid++;
				case SIDE_SPLIT:
					if (destSeg->flags & SEG_DONT_SPLIT) {
						curScore->invalid++;
					}
					sCount++; bsp.usedSector[destSeg->Sector] |= 0xFF;
					break;
				case SIDE_RIGHT:
					rCount++; bsp.usedSector[destSeg->Sector] |= 0x0F;
					break;
				}
			}
			// Only consider SEG if it is not a boundary line

			if (!(lCount * rCount + sCount)) {
				segs[i].flags |= SEG_EDGE;
			}

			if (lCount * rCount + sCount) {
				int lsCount = 0, rsCount = 0, ssCount = 0;
				for (int j = 0; j < bsp.sectorCount; j++) {
					switch (bsp.usedSector[j]) {
					case 0xF0: lsCount++;	break;
					case 0xFF: ssCount++;	break;
					case 0x0F: rsCount++;	break;
					}
				}

				curScore->index = i;
				curScore->metric1 = (long)(lCount + sCount) * (long)(rCount + sCount);
				curScore->metric2 = (long)(lsCount + ssCount) * (long)(rsCount + ssCount);

				if (sCount) {
					long temp = bsp.X1 * sCount;
					if (bsp.X2 < temp) curScore->metric1 = bsp.X2 * curScore->metric1 / temp;
					/*
					 * 27940 curScore->metric1 -= ( X3 * sCount * (sCount / 2 ) + X4 ) * sCount;
					 * 27923 curScore->metric1 -= ( X3 * sCount * (sCount / 3 ) + X4 ) * sCount;
					 * 27922 curScore->metric1 -= ( X3 * sCount * (sCount / 3 ) + X4 ) * sCount;
					 *       curScore->metric1 = 0xffffffff - abs(lCount -rCount) ;
					 * */
					curScore->metric1 -= (bsp.X3 * sCount * (sCount / 3) + bsp.X4) * sCount;
				}
				else if (options->SplitReduction & 0x01) {
					curScore->metric1 = 0xfffffff - abs(lCount - rCount);
					// curScore->metric1 = 0xfffffff - sCount;
				}

				// SubsSctorFactor

				int ssf = 1;
				/*
								if (noSegs < 200) {
									ssf = (200 / noSegs);
								}

								ssf = 1;
				*/
				// printf("ssf %d\n", ssf);

				if (ssCount) {
					long temp = bsp.X1 * ssCount;
					if (bsp.X2 < temp) curScore->metric2 = bsp.X2 * curScore->metric2 / temp;
					/*
					 * 27940 curScore->metric2 -= ( X3 * ssCount + X4 ) * sCount / 2;
					 * 27923 curScore->metric2 -= ( X3 * ssCount + X4 ) * sCount / 2;
					 * 27922 curScore->metric2 -= ( X3 * ssCount + X4 ) * sCount / 2;
					 *       curScore->metric2 = 0xffffffff - abs(lsCount -rsCount);
					 */
					curScore->metric2 -= (bsp.X3 * ssCount * ssf + bsp.X4) * sCount;
					// curScore->metric2 -= ( X3 * ssCount + X4 ) * sCount * 5;
				}
				else if (options->SplitReduction & 0x02) {
					curScore->metric2 = 0xffffff - abs(lsCount - rsCount);
					// curScore->metric2 = 0xffffff - ssCount;
				}

				if (!sCount && !ssCount) {
					quality[0]++;
				}
				else if ((sCount == 1) && (!ssCount)) {
					quality[1]++;
				}
				else if ((sCount == 0) && (ssCount == 1)) {
					quality[1]++;
				}
				else if ((sCount <= 1) && (ssCount <= 2)) {
					quality[2]++;
				}
				else if ((sCount <= 2) && (ssCount <= 1)) {
					quality[3]++;
				}
				else if ((sCount <= 3) && (ssCount <= 2)) {
					quality[4]++;
				}
				else if ((sCount <= 4) && (ssCount <= 4)) {
					quality[5]++;
				}
				else {
					quality[9]++;
				}



				noScores++;
			}
			else if (alias != 0) {
				// Eliminate outer edges of the map
				// segs[i].flags |= SEG_EDGE;
				*bsp.convexPtr++ = alias; // check THIS!!!!!
			}
		}
#if defined ( DEBUG )
		else if (lineChecked[alias] > 0) {
			count[0] = count[1] = count[2] = 0;
			ComputeStaticVariables(testSeg);
			int side = WhichSide(testSeg, options);
			if ((fabs(DX) < EPSILON) && (fabs(DY) < EPSILON)) continue;
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


	//	} // stupid test


	if (noScores > 1) {
		qsort(bsp.score, noScores, sizeof(sScoreInfo), sortMetric1);
		for (rank = i = 0; i < noScores; i++) {
			bsp.score[i].total = rank;
			if (bsp.score[i].metric1 != bsp.score[i + 1].metric1) rank++;
		}
		qsort(bsp.score, noScores, sizeof(sScoreInfo), sortMetric2);
		for (rank = i = 0; i < noScores; i++) {
			bsp.score[i].total += rank;
			if (bsp.score[i].metric2 != bsp.score[i + 1].metric2) rank++;
		}
		qsort(bsp.score, noScores, sizeof(sScoreInfo), sortTotalMetric);
	}

	if (noScores && bsp.score[0].invalid) {
		int noBad = 0;
		for (int i = 0; i < noScores; i++) if (bsp.score[i].invalid) noBad++;
		WARNING("Non-splittable linedefs have been split! (" << noBad << "/" << noScores << ")");
	}

	// SEG *pSeg = noScores ? &segs [ score [0].index ] : NULL;
	// SEG *pSeg = noScores ? &segs [ score [*width].index ] : NULL;

/*
	int w2 = *width;

	if (noScores < w2) {
		w2 = noScores;
	}

	for (int i = 0; i != w2; i++);
		widthSegs[i] = score[i].index;
	}
*/
	int retval;

	if (noScores == 0) {
		retval = -2;
	}
	else if (noScores == 1) {
		retval = bsp.score[0].index;
		bsp.noMoreScores = true;
	}
	else {
		int w = *width;

		for (int s = 0; s != noScores; s++) {
			if (bsp.score[s].index > -1) {
				retval = bsp.score[s].index;
				w--;
				if (w == 0) {
					break;
				}
			}
			else {

				// printf("Dodged a bullet\n");
			}
		}
		// retval = score [0].index;
	}

	if (noScores == *width) {
		bsp.noMoreScores = true;
	}

	for (int i = 0; i != nbd->options->Width; i++) {
		nbd->wideSegs[i] = bsp.score[i].index;

		if ((i + 1) == noScores) {
			break;
		}

	}


	/*
	if ((score [(*width) - 1].index) == -1) {
		printf("adjusting retval from %d (%d) to %d\n", retval, score [1].index,  score [0].index);
		retval =  score [0].index;
		if (retval == -1) {
			printf("error\n");
		}
	}*/

	// printf("depth %d, width %d, index %d, noScores: %d, retval %d (%d)\n", nodeDepth, *width, score [(*width) - 1].index, noScores, retval, score [0].index);
	/*
	if (pickFix && (noScores > 0)) {
		// printf("pick fix\n");
		nbd->goodScoresFound = 1;
	} else {
		nbd->goodScoresFound = 0;
	}*/


	nbd->goodScoresFound = quality[0];

	if (noSegs < 14) { // 14
		nbd->goodScoresFound += quality[1];
	}

	if (noSegs < 50) { // 50
		nbd->goodScoresFound += quality[2];
	}

	if (noSegs < 200) { // 200
		nbd->goodScoresFound += quality[3];
	}

	if (noSegs < 500) { // 500
		nbd->goodScoresFound += quality[4];
	}
	else {
		nbd->goodScoresFound = quality[0] + quality[1] + quality[2] + quality[3] + quality[4] + quality[5] + quality[6] + quality[7] + quality[8] + quality[9];
	}

	// nbd->goodScoresFound = 99;

	if (quality[0] > 6) {
		// printf("hq %d\n", quality[0]);
	}


	nbd->scoresFound = noScores;


	/*	for (int j = 0; j != noScores; j++) {
					printf("Score %d\n", score[j].index);
			}


		printf("return %d\n", retval);
	*/
	return retval;

	// zokum 2017, sept

	/*	if (score [1].index > -1) {
	// printf("retur #2");
	// printf("retur %d/%d\n", score [1].index, score [0].index);
	pSeg = noScores ? &segs [ score [1].index ] : NULL;
	}
	*/

	/*
	   if (cache < ENTRIES )  {
// printf("Requesting cache: %d, is %d\n", cache, splitCache[cache][0]);

if (splitCache[cache][0] != -2) {

// printf("Returned cahced seg!\n");
SEG *sSeg = &segs[ splitCache[cache][0]];

if (pSeg != sSeg) {
printf("CACHE ERROR %d %d\n", cache, score [0].index);
}
}
}


if ((cache < ENTRIES) && pSeg ) {
// we can only cache segs that aren't dynamic!
if (score [0].index < level->LineDefCount()) {
// printf("added to cache %d value %d\n", cache, score [0].index);
splitCache[cache][0] = score [0].index;
} else {
	// printf("seg was dynamic\n");
	}

// printf("\ncache: %d %d %d\n", score [0].index, score [1].index, score [2].index);
}

// printf("Returned computed seg\n");
*/
// return pSeg;
}