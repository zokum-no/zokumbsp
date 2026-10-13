#include <math.h>

#ifdef _WIN32
// Windows-specific includes and defines
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "constants.hpp"
#include "zennode.hpp"
#include "logger.hpp"
#include "bspgeometry.hpp"

unsigned int ComputeAngle(int dx, int dy) {
	double w;

	w = (atan2((double)dy, (double)dx) * (double)(65536 / (M_PI * 2)));

	if (w < 0) w = (double)65536 + w;

	return (unsigned)w;
}

BAM HandleAngle(const wLineDefInternal* lineDef, long dx, long dy) {
	BAM angle;
	/*
	 * These 4 new linedef types allow for arbitrary rotation of the ways walls are rendered compared to
	 * the base linedef. Check docs for more info.
	 */

	 // 		double a2;

	if (lineDef->type == 1081) { // linedef special to force angle to degrees
		angle = (BAM)((lineDef->tag * BAM360) / 360);
	}
	else if (lineDef->type == 1083) { // linedef special to force angle to BAM
		angle = (BAM)lineDef->tag;
	}
	else {
		// based on BSP's code, cleaner and simpler :)
		angle = ComputeAngle(dx, dy);

		if (lineDef->type == 1080) { // linedef special for additive degrees
			angle += (BAM)((lineDef->tag * BAM360) / 360);
		}
		else if (lineDef->type == 1082) { // linedef special for additive bam
			angle += (BAM)lineDef->tag;
		}
	}
	return angle;
}

const wSideDef* HandleSegRender(const wSideDef* sideDef) {
	if (sideDef) {
		if (*(UINT16*)sideDef->text2 != (UINT16)EMPTY_TEXTURE) {
			if (strncmp(sideDef->text2, "BSPNOSEG", 8) == 0) {
				return NULL;
			}
		}
	}
	return sideDef;
}
//----------------------------------------------------------------------------
//  Determine if the given SEG is co-linear (ie: they lie on the same line)
//    with the currently selected partition.
// 
//  It checks if the cross product of the two vectors is zero, which means they are parallel and thus co-linear.
//----------------------------------------------------------------------------

bool CoLinear(const SEG* seg, int x, int y, int dx, int dy, unsigned angle) {
	if ((angle & ANGLE_MASK) != (seg->Dataangle & ANGLE_MASK))
		return false;

	if (dx == 0)
		return seg->start.x == x;

	if (dy == 0)
		return seg->start.y == y;

	return dx * (seg->start.y - y) ==
		dy * (seg->start.x - x);
}

int _WhichSide(SEG* seg, sBSPOptions* options, DoomLevel* level) {
	FUNCTION_ENTRY(NULL, "_WhichSide", true);

	sVertex* vertS = &seg->start;
	sVertex* vertE = &seg->end;
	double y1, y2;

	if (bsp.DX == 0.0) {
		if (bsp.DY > 0.0) {
			y1 = (lrint(bsp.X) - lrint(vertS->x)), y2 = (lrint(bsp.X) - lrint(vertE->x));
		}
		else {
			y1 = (lrint(vertS->x) - lrint(bsp.X)), y2 = (lrint(vertE->x) - lrint(bsp.X));
		}
	}
	else if (bsp.DY == 0.0) {
		if (bsp.DX > 0.0) {
			y1 = (lrint(vertS->y) - lrint(bsp.Y)), y2 = (lrint(vertE->y) - lrint(bsp.Y));
		}
		else {
			y1 = (lrint(bsp.Y) - lrint(vertS->y)), y2 = (lrint(bsp.Y) - lrint(vertE->y));
		}
	}
	else {

		y1 = bsp.DX * (vertS->y - bsp.Y) - bsp.DY * (vertS->x - bsp.X);
		y2 = bsp.DX * (vertE->y - bsp.Y) - bsp.DY * (vertE->x - bsp.X);

		if ((y1 * y2 != 0.0) && ((fabs(y1) <= bsp.H) || (fabs(y2) <= bsp.H))) {


			// const wLineDefInternal *lineDef = seg->LineDef;
			wLineDefInternal* lineDef = &level->GetLineDefs()[seg->DatalineDef];

			wVertex* _vertS = &bsp.newVertices[lineDef->start];
			wVertex* _vertE = &bsp.newVertices[lineDef->end];

			double dx = _vertE->x - _vertS->x;
			double dy = _vertE->y - _vertS->y;
			double det = dx * bsp.DY - dy * bsp.DX;

			if (det != 0.0) {

				double num = bsp.DX * (_vertS->y - bsp.Y) - bsp.DY * (_vertS->x - bsp.X);
				double l = num / det;

				// if ( seg->Dataflip != 0 ) l = 1.0 - l;
				if (seg->flags & SEG_BACKSIDE) {
					l = 1.0 - l;
				}

				// if ( l < vertS->l ) { y1 = 0.0; goto xx; }
				//if ( l > vertE->l ) { y2 = 0.0; goto xx; }

				if (l < seg->startL) {
					y1 = 0.0;
					goto xx;
				}
				if (l > seg->endL) {
					y2 = 0.0;
					goto xx;
				}

				long x = lrint(_vertS->x + num * dx / det);
				long y = lrint(_vertS->y + num * dy / det);

				if ((lrint(vertS->x) == x) && (lrint(vertS->y) == y)) y1 = 0.0;
				if ((lrint(vertE->x) == x) && (lrint(vertE->y) == y)) y2 = 0.0;
			}
		}
	}


	if (fabs(y1) < EPSILON) y1 = 0.0;
	if (fabs(y2) < EPSILON) y2 = 0.0;

xx:

	// If its co-linear, decide based on direction
	if ((y1 == 0.0) && (y2 == 0.0)) {
		double x1 = bsp.DX * (vertS->x - bsp.X) + bsp.DY * (vertS->y - bsp.Y);
		double x2 = bsp.DX * (vertE->x - bsp.X) + bsp.DY * (vertE->y - bsp.Y);
		return (x1 <= x2) ? SIDE_RIGHT : SIDE_LEFT;
	}

	// Otherwise:
	//   Left   -1 : ( y1 >= 0 ) && ( y2 >= 0 )
	//   Both    0 : (( y1 < 0 ) && ( y2 > 0 )) || (( y1 > 0 ) && ( y2 < 0 ))
	//   Right   1 : ( y1 <= 0 ) && ( y2 <= 0 )

	// printf("-- %f %f\n", DX, bsp.DY);

	return (y1 < 0.0) ? ((y2 <= 0.0) ? SIDE_RIGHT : SIDE_SPLIT) :
		(y1 == 0.0) ? ((y2 <= 0.0) ? SIDE_RIGHT : SIDE_LEFT) :
		((y2 >= 0.0) ? SIDE_LEFT : SIDE_SPLIT);
}

int WhichSide(SEG* seg, sBSPOptions* options, DoomLevel* level) {
	FUNCTION_ENTRY(NULL, "WhichSide", true);

	// Treat split partition/seg differently
	//if (( seg->Split == true ) || ( currentAlias == 0 )) {
	if ((seg->flags & SEG_SPLIT) || (bsp.currentAlias == 0)) {
		return _WhichSide(seg, options, level);
	}

	// See if partition & seg lie on the same line
	int alias = bsp.lineDefAlias[seg->DatalineDef];
	if (alias == bsp.currentAlias) {
		// return seg->AliasFlip ^ SIDE_RIGHT;

		// if (seg->AliasFlip) {
		if (seg->flags & SEG_ALIAS_FLIP) {
			return -1;
		}
		else {
			return 1;
		}

		// return seg->AliasFlip ^ true;
		return (seg->flags & SEG_ALIAS_FLIP) ^ true;
	}

	// See if we've already categorized the LINEDEF for this SEG
	int side = bsp.currentSide[seg->DatalineDef];
	if (IS_LEFT_RIGHT(side)) return side;

	side = _WhichSide(seg, options, level);

	//if ( seg->Split == false ) {
	if ((seg->flags & SEG_SPLIT) == false) {
		bsp.currentSide[seg->DatalineDef] = (char)side;
	}

	return side;
}

bool IsConvex(SEG* segs, int noSegs, nodeBuilderData* nbd) {

	SEG* testSeg = segs;

	sBSPOptions* options = nbd->options;

	int count[3];
	int& lCount = count[0], & sCount = count[1], & rCount = count[2];

	for (int i = 0; i < noSegs; i++) {
		count[0] = count[1] = count[2] = 0;

		if (segs[i].flags & SEG_SPLIT) {
			continue;
		}

		ComputeStaticVariables(segs, i);

		if ((fabs(bsp.DX) < EPSILON) && (fabs(bsp.DY) < EPSILON)) continue;

		// memset ( bsp.usedSector, 0, sizeof ( UINT8 ) * bsp.sectorCount );
		SEG* destSeg = segs;
		for (int j = 0; j < noSegs; j++, destSeg++) {
			switch (_WhichSide(destSeg, options, nbd->level)) {
			case SIDE_LEFT:
				lCount++;
				// printf("R: %f %f -> %f %f\n", destSeg->start.x, destSeg->start.y, destSeg->end.x, destSeg->end.y);
				break;
			case SIDE_SPLIT:
				// printf("S: %f %f -> %f %f\n", destSeg->start.x, destSeg->start.y, destSeg->end.x, destSeg->end.y);
				return false;
				break;
			case SIDE_RIGHT:
				// printf("L: %f %f -> %f %f\n", destSeg->start.x, destSeg->start.y, destSeg->end.x, destSeg->end.y); 
				rCount++;
				break;
			}
			if (rCount * lCount) {
				return false;
			}
		}
	}
	return true;
}

//----------------------------------------------------------------------------
//  Calculate the set of variables used frequently that are based on the
//    currently selected SEG to be used as a partition line.
//----------------------------------------------------------------------------

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
	if (((int)bsp.DX == 0) && ((int)bsp.DY == 0)) {
		fprintf(stderr, "\nX=%f, bsp.Y=%f, bsp.DX=%f, bsp.DY=%f, H=%f \n", bsp.X, bsp.Y, bsp.DX, bsp.DY, H);
		fprintf(stderr, "DX & bsp.DY are both 0!\n");
		fprintf(stderr, "Offset: %d, different: %d\n", offset, different);

		exit(0);
	}
#endif

	bsp.ANGLE = pSeg->Dataangle;
}