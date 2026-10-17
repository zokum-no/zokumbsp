#include "constants.hpp"
#include "level.hpp"
#include "zennode.hpp"
#include "logger.hpp"
#include "bspcommon.hpp"
#include "bspgeometry.hpp"
#include "algorithmVertexSpace.hpp"


int AlgorithmVertex(SEG* segs, int noSegs, nodeBuilderData* nbd) {

	sBSPOptions* options = nbd->options;
	DoomLevel* level = nbd->level;
	int* width = &nbd->width;
	std::vector<int> wideSegs = nbd->wideSegs;
	// int *picks = nbd->maxPicks;

	nbd->goodScoresFound = 0;


	FUNCTION_ENTRY(NULL, "AlgorithmVertex", true);

	int quality[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

	SEG* testSeg = segs;
	int /*count[3],*/ noScores = 0/*, rank, i*/;
	// int &lCount = count [0], &sCount = count [1], &rCount = count [2];

	bsp.score[0].index = 0;

	// Check if it is a sub sector!
	if (IsConvex(segs, noSegs, nbd)) {
		return -2;
	}

	int vertices;

	// vertexSpace vs[nbd->vertices];
	std::vector<vertexSpace> vs(nbd->vertices);

	vertices = GenerateVertexSpace(vs.data(), segs, noSegs);

	FindConcaveCorners(segs, noSegs, vs.data(), vertices, nbd);

	noScores = 0; // We have 0 scores, let's try to find a nice partition line
	/*
	   noScores = PartitionLineGoodSeg(segs, noSegs, nbd, 9999, 9999);
	   */



	   // Basic decent seg based search, very quick
	if (noScores == 0) {
		// quick way to find good partitions
		// printf("gooseg\n");
		noScores = PartitionLineGoodSeg(segs, noSegs, nbd, 0, 2);
		//printf("scores %d\n", noScores);
	}

	// Let's try to split at points we have to split anyway...
	if (noScores == 0) {
		noScores = PartitionLineLastResort(segs, noSegs, vs.data(), vertices, nbd, 2);
	}

	// Let's try again, slightly less optimal split points.
	if (noScores == 0) {
		//  Let's try to split off at concave joins.
		// noScores = PartitionLineLastResort(segs, noSegs, vs, vertices, nbd, 1);
	}

	// Ok, if we have to split, might as well use a good algorithm
	/*	if (noScores) {
		if (score[0].sCount > 0) {
		noScores = PartitionLineGoodSeg(segs, noSegs, nbd, score[0].sCount, 99);
		}
		}
		*/
		// This is pretty much the last resort, this will try pretty much anything and come up with something
	if (noScores == 0) {
		noScores = PartitionLineLastResort(segs, noSegs, vs.data(), vertices, nbd, 0);
	}

	// Truly last resort, this can't happen I think
	if (noScores == 0) {
		noScores = PartitionLineGoodSeg(segs, noSegs, nbd, 9999, 9999);
	}

	// this gives a better progress indicator, since it finalizes the smaller areas first
	if (bsp.score[0].lCount < bsp.score[0].rCount) {
		int temp1 = (int) bsp.score[0].startx;
		int temp2 = (int) bsp.score[0].starty;

		bsp.score[0].startx = bsp.score[0].endx;
		bsp.score[0].starty = bsp.score[0].endy;

		bsp.score[0].endx = temp1;
		bsp.score[0].endy = temp2;

	}


	// We're done and we have at least 1 partition line, so let's have a look at it and return it.
	if (bsp.score[0].invalid) {
		int noBad = 0;
		for (int i = 0; i < noScores; i++) if (bsp.score[i].invalid) noBad++;
		WARNING("Non-splittable linedefs have been split! (" << noBad << "/" << noScores << ")");
	}

	// printf("Scores: %ld\n");

	int retval;

	if (noScores == 1) {
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
		}
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

	ComputeQuality(quality, noSegs, nbd);

	nbd->scoresFound = noScores;

	nbd->pSeg.start.x = bsp.score[0].startx;
	nbd->pSeg.start.y = bsp.score[0].starty;

	nbd->pSeg.end.x = bsp.score[0].endx;
	nbd->pSeg.end.y = bsp.score[0].endy;

	nbd->options->vertexPartition = true; // score[0].vertex;

	SEG* pSeg = &nbd->pSeg;

	// printf("P-line S0    : %f %5f %5f %5f\n", pSeg->start.x, pSeg->start.y, pSeg->end.x, pSeg->end.y);

	if (vertices < 5) {
		for (int verts = 0; verts != vertices; verts++) {
			// printf("%d %d\n",  vs[verts].x, vs[verts].y);
		}
	}

	// printf("Best line was: %5.0f %5.0f -> %5.0f %5.0f, segs: %d/%d, scores: %d, vertices: %d, rc: %d, lc: %d\n", nbd->pSeg.start.x, nbd->pSeg.start.y, nbd->pSeg.end.x, nbd->pSeg.end.y, noSegs, segCount, noScores, vertices, score[0].rCount, score[0].lCount);

	for (int d = 0; d != noSegs; d++) {
		// printf("%5.0f, %5.0f to %5.0f, %5.0f || ", segs[d].start.x, segs[d].start.y, segs[d].end.x, segs[d].end.y);
	}
	// printf ("\n");

	return 0;
}


int PartitionLineLastResort(SEG* segs, int noSegs, vertexSpace* vs, int vertices, nodeBuilderData* nbd, int convexMode) {

	if (convexMode) {
		FindConcaveCorners(segs, noSegs, vs, vertices, nbd);
	}

	int quality[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

	SEG* testSeg = segs;
	int count[3], noScores = 0 /*, rank, i*/;
	int& lCount = count[0], & sCount = count[1], & rCount = count[2];

	bsp.score[0].index = 0;





	for (int outer = 0; outer < vertices; outer++) {

		if ((convexMode > 0) && (vs[outer].flags == 0)) {
			continue;
		}


		if (noSegs > 5) {
			double progress = (double)outer / (double)vertices;

			ProgressBar((char*)"V-space   ", progress, 50, 1);
		}

		int bestSplit = 99999;

		for (int inner = outer + 1; inner < vertices; inner++) {

			if ((convexMode == 2) && (vs[inner].flags == 0)) {
				continue;
			}


			int startx = vs[outer].x;
			int starty = vs[outer].y;

			int endx = vs[inner].x;
			int endy = vs[inner].y;

			if (!(((int)startx == (int)endx) && (((int)starty == (int)endy)))) {

				bsp.X = startx;
				bsp.Y = starty;

				bsp.DX = endx - startx;
				bsp.DY = endy - starty;

				bsp.ANGLE = ComputeAngle(bsp.DX, bsp.DY);

				count[0] = count[1] = count[2] = 0;

				bsp.H = (bsp.DX * bsp.DX) + (bsp.DY * bsp.DY);
				bsp.currentAlias = 0;

				nbd->pSeg.flags = SEG_FINAL; // clever hack

				// printf("Part %5.0f %5.0f %5.0f %5.0f %d\n", bsp.X, bsp.Y, bsp.DX, bsp.DY, bsp.ANGLE);

				if ((fabs(bsp.DX) < EPSILON) && (fabs(bsp.DY) < EPSILON)) continue; //goto vertexnext;

				sScoreInfo* curScore = &bsp.score[noScores];
				curScore->invalid = 0;
				curScore->vertex = true;

				curScore->metric1 = 0;
				curScore->metric2 = 0;

				memset(bsp.usedSector, 0, sizeof(UINT8) * bsp.sectorCount);

				SEG* destSeg = segs;
				for (int j = 0; j < noSegs; j++, destSeg++) {
					switch (_WhichSide(destSeg, nbd->options, nbd->level)) {
					case SIDE_LEFT:
						lCount++; bsp.usedSector[destSeg->Sector] |= 0xF0;
						break;
					case SIDE_SPLIT:
						if (destSeg->flags & SEG_DONT_SPLIT) {
							curScore->invalid++;
						}
						sCount++; bsp.usedSector[destSeg->Sector] |= 0xFF;

						if (sCount > bestSplit) {
							// inner = vertices - 1;
						}

						break;
					case SIDE_RIGHT:
						rCount++; bsp.usedSector[destSeg->Sector] |= 0x0F;
						break;
					}
					if (noSegs < 35) {
						// printf("SeG %f %f -> %f %f | side %d %d %d\n", destSeg->start.x, destSeg->start.y, destSeg->end.x, destSeg->end.y, lCount, sCount, rCount);
					}
				}
				/*
				   if (vertices < 50) {
				   printf("Part %5.0f %5.0f %5.0f %5.0f %5ld | %5d %5d -> %5d %5d\n", bsp.X, bsp.Y, bsp.DX, bsp.DY, bsp.ANGLE, startx, starty, endx, endy);
				   }
				   */
				if ((lCount * rCount + sCount)) {

					// printf("l: %d, r: %d, s: %d\n", count[0], count[1],count[2]);

					if (sCount < bestSplit) {
						bestSplit = sCount;
					}

					int lsCount = 0, rsCount = 0, ssCount = 0;
					for (int j = 0; j < bsp.sectorCount; j++) {
						switch (bsp.usedSector[j]) {
						case 0xF0: lsCount++;	break;
						case 0xFF: ssCount++;	break;
						case 0x0F: rsCount++;	break;
						}
					}
					curScore->index = 0;

					int cornerCount = 0;
					for (int c = 0; c != vertices; c++) {
						if (vs[c].flags & 0x01) {
							// printf("corner check\n");

							int ax = startx;
							int ay = starty;
							int bx = endx;
							int by = endy;
							int cx = vs[0].x;
							int cy = vs[0].y;

							if ((ax != cx) && (bx != cx) && (ay != cy) && (by != cy)) {



								if (((bx - ax) * (cy - ay)) == ((cx - ax) * (by - ay))) {
									cornerCount++;
									// printf("cc++");
								}
							}
						}
					}
					ssCount -= cornerCount;

					if (cornerCount) {
						// printf("ss: %d, cc: %d\n", ssCount, cornerCount);
					}

					if (convexMode) {
						curScore->metric1 = (long)  ((0xffff - ssCount) / (sCount + 1)) + sqrt(bsp.DX * bsp.DX + bsp.DY * bsp.DY);;
					}
					else {
						// printf("ssc: %d\n", ssCount);
						curScore->metric1 = (long)  ((0xffff /*- ssCount*/) / (sCount + 1)) - abs(rCount - lCount);
					}

					if ((!ssCount) && (!sCount)) {
						// curScore->metric1 = ((0xfffff));

					}

					if (((lCount * 15) < rCount) || (rCount * 15) < (lCount)) {
						// curScore->metric1 = 0;
						// curScore->metric2 = 0;
					}

					/*
					   if (((lCount * 50) < rCount) || ((rCount * 50) < lCount)) {
					   curScore->metric1 = lCount * rCount;
					   }
					   */


					curScore->startx = startx;
					curScore->starty = starty;
					curScore->endx = endx;

					curScore->endy = endy;
					curScore->rCount = rCount;
					curScore->lCount = lCount;

					int ssf = 1;


					// printf("index %d, \n", curScore->index);


					if ((convexMode == 0) || (sCount == 0)) {
						if ((convexMode == 0) || (ssCount == 1)) {
							noScores++;
						}
					}

					if (noScores == (MAX_SCORES - 1)) {
						SortScores(noScores, bsp.score);
						// noScores = MAX_SCORES / 2;
						noScores = 10; // we only keep top 10
					}
				}

			}
		}
	}

	if (noScores > 1) {
		SortScores(noScores, bsp.score);
	}


	// printf("Last resort return: %8f %8f -> %8f %8f (%d scores) \n", score[0].startx, score[0].starty, score[0].endx, score[0].endy, noScores );

	return noScores;

}

void FindConcaveCorners(SEG* segs, int noSegs, vertexSpace* vs, int vertices, nodeBuilderData* nbd) {
	for (int s = 0; s != noSegs; s++) {

		for (int c = s + 1; c != noSegs; c++) {

			// check if they belong to the same sector
			if (segs[s].Sector == segs[c].Sector) {

				// Do the segs have a common vertex?
				if ((segs[s].end.x == segs[c].start.x) && (segs[s].end.y == segs[c].start.y)) {

					for (int v = 0; v < vertices; v++) {

						// printf("%3d / %3d- Checking %d %d\n", v, vertices, vs[v].x, vs[v].y);

						if ((segs[s].end.x == vs[v].x) && (segs[s].end.y == vs[v].y) && (vs[v].flags == 0)) {

							// simple but slightly inefficient test

							BAM a1, a2;

							if (segs[s].flags & 0x01) {
								a1 = segs[s].Dataangle;
							}
							else {
								a1 = segs[s].Dataangle;
							}

							if (segs[s].flags & 0x01) {
								a2 = segs[c].Dataangle;
							}
							else {
								a2 = segs[c].Dataangle;
							}

							if (a2 == a1) {
								continue;
							}


							a2 = a2 - a1;
							a1 = 0;



							if (a2 < 32767) {
								// printf("Concave: %6d | %6d | lds:  %3d,%3d Sectors: %d,%d\n", a1, a2, segs[s].DatalineDef, segs[c].DatalineDef, segs[s].Sector, segs[c].Sector);
								vs[v].flags = 1;

							}
							else {
								// printf("       : %6d | %6d | lds:  %3d,%3d Sectors: %d,%d\n", a1, a2, segs[s].DatalineDef, segs[c].DatalineDef, segs[s].Sector, segs[c].Sector);
							}

						}
					}
				}
				// printf("meh\n");
			}
		}
	}
	// exit(0);
}

void ComputeQuality(int* quality, int noSegs, nodeBuilderData* nbd) {
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

	if (quality[0] > 6) {
		// printf("hq %d\n", quality[0]);
	}

}

// splits refers to segs being splits and subSectors the visplane generating areas
int PartitionLineGoodSeg(SEG* segs, int noSegs, nodeBuilderData* nbd, int maxSplits, int maxSubSectors) {

	int quality[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

	SEG* testSeg = segs;
	int count[3], noScores = 0, /*rank,*/ i;
	int& lCount = count[0], & sCount = count[1], & rCount = count[2];

	bsp.score[0].index = 0;

	nbd->pSeg.flags = SEG_FINAL; // clever hack

	for (i = 0; i < noSegs; i++) {
		int alias = testSeg->flags & SEG_SPLIT ? 0 : bsp.lineDefAlias[testSeg->DatalineDef];

		if ((alias == 0) || (bsp.lineChecked[alias] == false)) {

			bsp.lineChecked[alias] = -1;
			count[0] = count[1] = count[2] = 0;

			ComputeStaticVariables(segs, i);

			bsp.X = lrint(bsp.X);
			bsp.Y = lrint(bsp.Y);
			bsp.DX = lrint(bsp.DX);
			bsp.DY = lrint(bsp.DY);

			if ((fabs(bsp.DX) < EPSILON) && (fabs(bsp.DY) < EPSILON)) goto next;
			sScoreInfo* curScore = &bsp.score[noScores];
			curScore->invalid = 0;
			memset(bsp.usedSector, 0, sizeof(UINT8) * bsp.sectorCount);
			SEG* destSeg = segs;
			for (int j = 0; j < noSegs; j++, destSeg++) {
				// printf ("%d %d\n", j, noSegs);
				switch (WhichSide(destSeg, nbd->options, nbd->level)) {
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

				//curScore->startx = segs[i].start.x;
				//curScore->starty = segs[i].start.y;
				//curScore->endx = segs[i].end.x;
				//curScore->endy = segs[i].end.y;

				curScore->startx = bsp.X;
				curScore->starty = bsp.Y;

				curScore->endx = bsp.X + bsp.DX;
				curScore->endy = bsp.Y + bsp.DY;

				curScore->lCount = lCount;
				curScore->rCount = rCount;

				if (sCount) {
					long temp = bsp.X1 * sCount;
					if (bsp.X2 < temp) curScore->metric1 = bsp.X2 * curScore->metric1 / temp;
					curScore->metric1 -= (bsp.X3 * sCount * (sCount / 3) + bsp.X4) * sCount;
				}
				else if (nbd->options->SplitReduction & 0x01) {
					curScore->metric1 = 0xfffffff - abs(lCount - rCount);
				}

				int ssf = 1;

				if (ssCount) {
					long temp = bsp.X1 * ssCount;
					if (bsp.X2 < temp) curScore->metric2 = bsp.X2 * curScore->metric2 / temp;
					curScore->metric2 -= (bsp.X3 * ssCount * ssf + bsp.X4) * sCount;
				}
				else if (nbd->options->SplitReduction & 0x02) {
					curScore->metric2 = 0xffffff - abs(lsCount - rsCount);
				}

				if ((sCount <= maxSplits) && (ssCount <= maxSubSectors)) {
					noScores++;
				}

				if (noScores == (MAX_SCORES - 1)) {
					SortScores(noScores, bsp.score);
					noScores = 10; // we only keep top 10
				}


			}
			else if (alias != 0) {
				*bsp.convexPtr++ = alias;
			}
		}
	next:
		testSeg++;
	}

	// printf("found %f, %f - %f, %f\n", score[0].startx, score[0].starty, score[0].endx, score[0].endy);

	return noScores;
}

int GenerateVertexSpace(vertexSpace* vs, SEG* segs, int noSegs) {

	int vertices = 0;

	for (int i = 0; i != noSegs; i++) {

		int v = 0;
		bool dupe = false;
		/*
				if (segs[i].flags & SEG_SPLIT) {
					if ( ((int) segs[i].start.x != lrint(segs[i].start.x))
						|| ((int) segs[i].start.y != lrint(segs[i].start.y))
						|| ((int) segs[i].end.x != lrint(segs[i].end.x))
						|| ((int) segs[i].end.y != lrint(segs[i].end.y))) {
						continue;
					}
				}
		*/
		if ((!(segs[i].flags & SEG_SPLIT_START)) || (segs[i].flags & SEG_FINAL)) {

			int x = lrint(segs[i].start.x);
			int y = lrint(segs[i].start.y);

			while (v != vertices) {
				if ((x == vs[v].x) && (y == vs[v].y)) {
					dupe = true;
					break;
				}
				v++;
			}
			if (!dupe) {
				vs[vertices].x = x;
				vs[vertices].y = y;
				vs[vertices].flags = 0;
				vertices++;
			}
		}

		// this fixes some rare bugs..... e1m8 in doom.wad etc
		// repeat once more, for end-vertices. These often occur after splits!

		if ((!(segs[i].flags & SEG_SPLIT_END) || (segs[i].flags & SEG_FINAL))) {
			v = 0;
			dupe = false;

			int x = lrint(segs[i].start.x);
			int y = lrint(segs[i].start.y);

			while (v != vertices) {
				if ((x == vs[v].x) && (y == vs[v].y)) {
					dupe = true;
					break;
				}
				v++;
			}
			if (!dupe) {
				vs[vertices].x = (int) segs[i].end.x;
				vs[vertices].y = (int) segs[i].end.y;
				vs[vertices].flags = 0;
				vertices++;
			}
		}
	}
	return vertices;
}