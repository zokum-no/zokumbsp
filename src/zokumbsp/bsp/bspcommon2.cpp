#include "level.hpp"
#include "zennode.hpp"
#include "logger.hpp"

void ComputeStaticVariables(SEG* list, int offset) {
	SEG* pSeg = &list[offset];

	FUNCTION_ENTRY(NULL, "ComputeStaticVariables", true);
#if defined ( DIAGNOSTIC )
	int different = 0;
#endif

	// if ( pSeg->final == false ) {
	if ((pSeg->flags & SEG_FINAL) == false) {
		currentAlias = lineDefAlias[pSeg->DatalineDef];
		currentSide = sideInfo ? sideInfo[currentAlias] : NULL;

		if (!(pSeg->flags & SEG_ALIAS_FLIP)) {
			X = pSeg->start.x;
			Y = pSeg->start.y;

			DX = pSeg->end.x - pSeg->start.x;
			DY = pSeg->end.y - pSeg->start.y;
		}
		else {
			X = pSeg->end.x;
			Y = pSeg->end.y;

			DX = pSeg->start.x - pSeg->end.x;
			DY = pSeg->start.y - pSeg->end.y;
		}


#if defined ( DIAGNOSTIC )
		if (vertS != vertE) {
			different++;
		}
#endif


	}
	else {
		currentAlias = 0;

		X = pSeg->start.x;
		Y = pSeg->start.y;
		DX = pSeg->end.x - pSeg->start.x;
		DY = pSeg->end.y - pSeg->start.y;
	}

	H = (DX * DX) + (DY * DY);

#if defined ( DIAGNOSTIC )
	if (((int)DX == 0) && ((int)DY == 0)) {
		fprintf(stderr, "\nX=%f, Y=%f, DX=%f, DY=%f, H=%f \n", X, Y, DX, DY, H);
		fprintf(stderr, "DX & DY are both 0!\n");
		fprintf(stderr, "Offset: %d, different: %d\n", offset, different);

		exit(0);
	}
#endif

	bsp.ANGLE = pSeg->Dataangle;
}