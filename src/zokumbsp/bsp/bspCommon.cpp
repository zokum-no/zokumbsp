#include "zennode.hpp"
#include "bspcommon.hpp"
#include "../../common/logger.hpp"
#include "level.hpp"

/*
void ComputeStaticVariables(SEG* list, int offset) {
	SEG* pSeg = &list[offset];

	FUNCTION_ENTRY(NULL, "ComputeStaticVariables", true);
#if defined ( DIAGNOSTIC )
	int different = 0;
#endif

	// if ( pSeg->final == false ) {
	if ((pSeg->flags & SEG_FINAL) == false) {
		bsp.currentAlias = bsp.lineDefAlias[pSeg->DatalineDef];
		bsp.currentSide = bsp.sideInfo ? bsp.sideInfo[bsp.currentAlias] : NULL;

		if (!(pSeg->flags & SEG_ALIAS_FLIP)) {
			bsp.X = pSeg->start.x;
			bsp.Y = pSeg->start.y;

			bsp.DX = pSeg->end.x - pSeg->start.x;
			bsp.DY = pSeg->end.y - pSeg->start.y;
		}
		else {
			bsp.X = pSeg->end.x;
			bsp.Y = pSeg->end.y;

			bsp.DX = pSeg->start.x - pSeg->end.x;
			bsp.DY = pSeg->start.y - pSeg->end.y;
		}


#if defined ( DIAGNOSTIC )
		if (vertS != vertE) {
			different++;
		}
#endif


	}
	else {
		bsp.currentAlias = 0;

		bsp.X = pSeg->start.x;
		bsp.Y = pSeg->start.y;
		bsp.DX = pSeg->end.x - pSeg->start.x;
		bsp.DY = pSeg->end.y - pSeg->start.y;
	}

	bsp.H = (bsp.DX * bsp.DX) + (bsp.DY * bsp.DY);

#if defined ( DIAGNOSTIC )
	if (((int)DX == 0) && ((int)DY == 0)) {
		fprintf(stderr, "\nX=%f, bsp.Y=%f, bsp.DX=%f, bsp.DY=%f, H=%f \n", bsp.X, bsp.Y, bsp.DX, bsp.DY, H);
		fprintf(stderr, "DX & bsp.DY are both 0!\n");
		fprintf(stderr, "Offset: %d, different: %d\n", offset, different);

		exit(0);
	}
#endif

	bsp.ANGLE = pSeg->Dataangle;
}
*/

void SortScores(int noScores, sScoreInfo* score) {
	int rank, i;

	// printf("scores %d\n", noScores);

	score[noScores + 1].metric1 = 0;
	score[noScores + 1].metric2 = 0;



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
}

int sortTotalMetric(const void* ptr1, const void* ptr2) {
	FUNCTION_ENTRY(NULL, "sortTotalMetric", true);

	int dif;
	dif = ((sScoreInfo*)ptr1)->invalid - ((sScoreInfo*)ptr2)->invalid;
	if (dif) return dif;
	dif = ((sScoreInfo*)ptr1)->total - ((sScoreInfo*)ptr2)->total;
	if (dif) return dif;
	dif = ((sScoreInfo*)ptr1)->index - ((sScoreInfo*)ptr2)->index;
	return dif;
}

int sortMetric1(const void* ptr1, const void* ptr2) {
	FUNCTION_ENTRY(NULL, "sortMetric1", true);

	if (((sScoreInfo*)ptr2)->metric1 < ((sScoreInfo*)ptr1)->metric1) return -1;
	if (((sScoreInfo*)ptr2)->metric1 > ((sScoreInfo*)ptr1)->metric1) return  1;
	if (((sScoreInfo*)ptr2)->metric2 < ((sScoreInfo*)ptr1)->metric2) return -1;
	if (((sScoreInfo*)ptr2)->metric2 > ((sScoreInfo*)ptr1)->metric2) return  1;
	return ((sScoreInfo*)ptr1)->index - ((sScoreInfo*)ptr2)->index;
}

int sortMetric2(const void* ptr1, const void* ptr2) {
	FUNCTION_ENTRY(NULL, "sortMetric2", true);

	if (((sScoreInfo*)ptr2)->metric2 < ((sScoreInfo*)ptr1)->metric2) return -1;
	if (((sScoreInfo*)ptr2)->metric2 > ((sScoreInfo*)ptr1)->metric2) return  1;
	if (((sScoreInfo*)ptr2)->metric1 < ((sScoreInfo*)ptr1)->metric1) return -1;
	if (((sScoreInfo*)ptr2)->metric1 > ((sScoreInfo*)ptr1)->metric1) return  1;
	return ((sScoreInfo*)ptr1)->index - ((sScoreInfo*)ptr2)->index;
}