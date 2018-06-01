//----------------------------------------------------------------------------
//
// File:        ZenNode.cpp
// Date:        26-Oct-1994
// Programmer:  Marc Rousseau
//
// Description: This module contains the logic for the NODES builder.
//
// Copyright (c) 1994-2004 Marc Rousseau, All Rights Reserved.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307, USA.
//
// Revision History:
//
//   06-??-95   Added LineDef alias list to speed up the process.
//   07-07-95   Added currentAlias/Side/Flipped to speed up WhichSide.
//   07-11-95   Initialized global variables in CreateNODES.
//              Changed logic for static variable last in CreateSSector.
//   10-05-95   Added convexList & extended the use of lineUsed.
//   10-25-95   Changed from doubly linked lists to an array of SEGs.
//   10-27-95   Added header to each function describing what it does.
//   11-14-95   Fixed sideInfo so that a SEG is always to it's own right.
//   12-06-95   Added code to support selective unique sectors & don't splits
//   05-09-96   Added nodePool to reduced calls to new/delete for NODEs
//   05-15-96   Reduced memory requirements for convexList & sectorInfo
//   05-23-96   Added FACTOR_XXX to reduced memory requirements
//   05-24-96   Removed all calls to new/delete during CreateNode
//   05-31-96   Made WhichSide inline & moved the bulk of code to _WhichSide
//   10-01-96   Reordered functions & removed all function prototypes
//   07-31-00   Increased max subsector factor from 15 to 256
//   10-29-00   Fixed _WhichSide & DivideSeg so that things finally(?) work!
//   02-21-01   Fixed _WhichSide & DivideSeg so that things finally(?) work!
//   02-22-01   Added vertSplitAlias to help _WhichSide's accuracy problem
//   02-26-01   Removed vertSplitAlias - switched to 64-bit ints to solve overflow problem
//   03-05-01   Fixed _WhichSide & DivideSeg so that things finally(?) work!
//   03-05-01   Switched to 64-bit ints to solve overflow problem
//   04-13-02   Changed vertex & ssector allocation to reallocate the free pool
//   04-14-02   Fixed _WhichSide so that things finally(?) work! :-)
//   04-16-02   Added more consistancy checks in DEBUG mode
//   04-20-02   Switched to floating point math
//   04-24-02   Fixed bug in _WhichSide for zero-length SEGs
//   05-02-02   Simplified CreateNode & eliminated NODES structure
//   05-04-02   Simplified use of aliases and flipping in WhichSide 
//   05-05-02   Fixed double-int conversions to round correctly
//   11-16-03   Converted code to work properly on 64 bit machines
//   01-31-04   Added extra sector sort to SortByLineDef
//
//----------------------------------------------------------------------------


#undef DIAGNOSTIC

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.hpp"
#include "logger.hpp"

#include "level.hpp"
#include "zennode.hpp"
#include "blockmap.hpp"
#include "console.hpp"

DBG_REGISTER ( __FILE__ );

#define ANGLE_MASK              0x7FFF

#define EPSILON                 0.0001

// Emperical values derived from a test of numerous .WAD files
// #define FACTOR_VERTEX           1.6             //  1.662791 - ???
//#define FACTOR_SEGS             3.0             //  1.488095 - ???
//#define FACTOR_NODE             0.6             //  0.590830 - MAP01 - csweeper.wad

#define FACTOR_VERTEX 1.1
#define FACTOR_SEGS 1.1
#define FACTOR_NODE 0.2


// int MAX_WIDTH = 3;

static int       maxSegs;
static int       maxVertices;

static int       nodesLeft;
static wNode    *nodePool;
static int       nodeCount;                     // Number of NODES stored

static int	nodePoolEntries;

static SEG      *tempSeg;
int 		tempSegEntries;

static SEG      *segStart;
static int       segCount;                      // Number of SEGS stored

static int       ssectorsLeft;
static wSSector *ssectorPool;
static int       ssectorCount;                  // Number of SSECTORS stored

static int 	ssectorPoolEntries;

static wVertex  *newVertices;
static int       noVertices;

//
// Variables used by WhichSide to speed up side calculations
//
static char     *currentSide;
static int       currentAlias;

static int      *convexList;
static int      *convexPtr;
static int       sectorCount;

static int 	convexListEntries;

static int       showProgress;
static UINT8    *usedSector;
static bool     *keepUnique;
static char     *lineUsed;
static char     *lineChecked;
static int       noAliases;
static int      *lineDefAlias;
static char    **sideInfo;
static double    DY, DX, X, Y, H;
static long      ANGLE;

// options 
static bool      uniqueSubsectors;

long sideInfoEntries;

static sScoreInfo *score;

int fakeLineDef = 0;

long progressWide = 0;


int nodeDepth = 0;


// may 2018 stupid hack!
bool noMoreScores;



// metric = S ? ( L * R ) / ( X1 ? X1 * S / X2 : 1 ) - ( X3 * S + X4 ) * S : ( L * R );
static long X1 = getenv ( "ZEN_X1" ) ? atol ( getenv ( "ZEN_X1" )) : 20;
static long X2 = getenv ( "ZEN_X2" ) ? atol ( getenv ( "ZEN_X2" )) : 10;
static long X3 = getenv ( "ZEN_X3" ) ? atol ( getenv ( "ZEN_X3" )) : 1;
static long X4 = getenv ( "ZEN_X4" ) ? atol ( getenv ( "ZEN_X4" )) : 25;

static long Y1 = getenv ( "ZEN_Y1" ) ? atol ( getenv ( "ZEN_Y1" )) : 1;
static long Y2 = getenv ( "ZEN_Y2" ) ? atol ( getenv ( "ZEN_Y2" )) : 7;
static long Y3 = getenv ( "ZEN_Y3" ) ? atol ( getenv ( "ZEN_Y3" )) : 1;
static long Y4 = getenv ( "ZEN_Y4" ) ? atol ( getenv ( "ZEN_Y4" )) : 0;

// static SEG *(*PartitionFunction) ( SEG *, int, sBSPOptions *options, DoomLevel *level, int *width, int *wideSegs);
static int (*PartitionFunction) ( SEG *, int, sBSPOptions *options, DoomLevel *level, int *width, int *wideSegs);

int bestSegCount = 32767;
int bestNodeCount = 32767;
int bestSubSectorCount = 32767;

bool earlyExit = false;

// Big values will break it when using adaptive :(
// It can still break when using smaller ones, but it's unlikely...
// #define ENTRIES 64
#define ENTRIES 0
#define CACHEWIDTH 1

int splitCache[ENTRIES][CACHEWIDTH];

//----------------------------------------------------------------------------
//  Create a list of SEGs from the *important* sidedefs.  A sidedef is
//    considered important if:
//     - It has non-zero length
//     - It's linedef has different sectors on each side
//     - It has at least one visible texture
//----------------------------------------------------------------------------

inline unsigned int ComputeAngle(int dx, int dy) {
	double w;

	w = (atan2( (double) dy , (double) dx) * (double)(65536/(M_PI*2)));

	if(w<0) w = (double)65536+w;

	return (unsigned) w;
}


static SEG *CreateSegs ( DoomLevel *level, sBSPOptions *options ) {
	FUNCTION_ENTRY ( NULL, "CreateSegs", true );


	// printf("\n");

	// Get a rough count of how many SideDefs we're starting with
	segCount = maxSegs = 0;
	const wLineDefInternal *lineDef = level->GetLineDefs ();
	const wSideDef *sideDef = level->GetSideDefs ();

	for ( int i = 0; i < level->LineDefCount (); i++ ) {
		if ( lineDef [i].sideDef [0] != NO_SIDEDEF ) {
			maxSegs++;
		}
		if ( lineDef [i].sideDef [1] != NO_SIDEDEF ) {
			maxSegs++;
		}
	}
	tempSeg  = new SEG [ maxSegs ];
	tempSegEntries = maxSegs;

	maxSegs  = ( int ) ( maxSegs * FACTOR_SEGS );
	segStart = new SEG [ maxSegs ];

	memset ( segStart, 0, sizeof ( SEG ) * maxSegs );

	SEG *seg = segStart;

	for ( int i = 0; i < level->LineDefCount (); i++, lineDef++ ) {

		if (level->extraData->lineDefsRendered[i] == false) {
			continue;
		}

		wVertex *vertS = &newVertices [ lineDef->start ];
		wVertex *vertE = &newVertices [ lineDef->end ];
		long dx = vertE->x - vertS->x;
		long dy = vertE->y - vertS->y;

		if (( dx == 0 ) && ( dy == 0 )) {
			continue;
		}

		/*
		   if (lineDef->tag == 998) {
		   continue;
		   }
		   */

		int rSide = lineDef->sideDef [0];
		int lSide = lineDef->sideDef [1];

		if (level->extraData->lineDefsSegProperties[i] == 0x01) {
			lSide = NO_SIDEDEF;
		}

		const wSideDef *sideRight = ( rSide == NO_SIDEDEF ) ? ( const wSideDef * ) NULL : &sideDef [ rSide ];
		const wSideDef *sideLeft  = ( lSide == NO_SIDEDEF ) ? ( const wSideDef * ) NULL : &sideDef [ lSide ];

		// Ignore line if both sides point to the same sector & neither side has any visible texture
		if ( options->reduceLineDefs && sideRight && sideLeft && ( sideRight->sector == sideLeft->sector )) {
			if ( * ( UINT16 * ) sideLeft->text3 == ( UINT16 ) EMPTY_TEXTURE ) {
				sideLeft = ( const wSideDef * ) NULL;
			}
			if ( * ( UINT16 * ) sideRight->text3 == ( UINT16 ) EMPTY_TEXTURE ) {
				sideRight = ( const wSideDef * ) NULL;
			}
			if ( ! sideLeft && ! sideRight ) {
				continue;
			}
		}

		if ( options->ignoreLineDef && options->ignoreLineDef [i] ) {
			continue;
		}

		BAM angle;
		/*
		 * These 4 new linedef types allow for arbitrary rotation of the ways walls are rendered compared to
		 * the base linedef. Check docs for more info.
		 */

		double a2;

		if (lineDef->type == 1081) { // linedef special to force angle to degrees
			// angle = ((BAM)lineDef->tag * BAM360) / (BAM)360;
			angle = (BAM) ((lineDef->tag * BAM360) / 360);
		} else if (lineDef->type == 1083) { // linedef special to force angle to BAM
			angle = (BAM)lineDef->tag;
		} else {
			// based on BSP's code, cleaner and simpler :)

			angle = (dy == 0) ? (BAM)((dx < 0) ? BAM180 : 0) :
				(dx == 0) ? (BAM)((dy < 0) ? BAM270 : BAM90) :
				(BAM)(atan2((float)dy, (float)dx) * BAM180 / M_PI + 0.5 * sgn(dy));

			angle = ComputeAngle(dx, dy);

			// angle = (BAM)(atan2((float)dy, (float)dx) * BAM180 / M_PI + 0.5 * sgn(dy) );
			// a2 = (atan2((float)dy, (float)dx) * 180 / M_PI + 0.5 * sgn(dy) + 90.0);

			// a2 = atan2( (double) dy, (double) dx ) * 180.0 / M_PI  ;

			if (lineDef->type == 1080) { // linedef special for additive degrees
				// angle += ((double)lineDef->tag * (double)BAM360) / 360.0;
				//printf("\n-- %u %d --\n", angle, lineDef->tag);
				//double d = ( (double) lineDef->tag / 360.0) * (double) BAM360;

				angle += (BAM) ((lineDef->tag * BAM360) / 360);
				// angle += (BAM) d;
				// printf("\n-- %u -- (%f)\n", angle, d);
			} else if (lineDef->type == 1082) { // linedef special for additive bam
				// printf("\n");
				angle += (BAM)lineDef->tag;
				// printf("\n");
			}
		}

		/*
		   BAM angle = ( dy == 0 ) ? ( BAM ) (( dx < 0 ) ? BAM180 : 0 ) :
		   ( dx == 0 ) ? ( BAM ) (( dy < 0 ) ? BAM270 : BAM90 ) :
		   ( BAM ) ( atan2 (( float ) dy, ( float ) dx ) * BAM180 / M_PI + 0.5 * sgn ( dy ));
		   */ 

		/*if (i != 758) {
			printf ("Lindef: %3d (x: %4ld y:%4ld) [BAM: %5u / radians: %lf]\n", i, dx, dy, angle, atan2(dy,dx)  );
		}
		*/
		// printf("A: %5d", angle);

		bool split = options->dontSplit ? options->dontSplit [i] : false;

		if (lineDef->type == 1087) {
			split = true;
		}

		// we do all the other jazz, except adding the actual segs :)
		if (level->extraData->lineDefsRendered[i] == false) {
			continue;
		}

		// printf("dy: %d dx: %d -> %lf \n",  dy,dx, atan2 (dy, dx));


		if ( sideRight ) {
			seg->Data.start   = lineDef->start;
			seg->Data.end     = lineDef->end;
			seg->Data.angle   = angle;
			seg->Data.lineDef = ( UINT16 ) i;
			seg->Data.flip    = 0;
			seg->LineDef      = lineDef;
			seg->Sector       = sideRight->sector;
			seg->DontSplit    = split;
			seg->start.x      = vertS->x;
			seg->start.y      = vertS->y;
			seg->start.l      = 0.0;
			seg->end.x        = vertE->x;
			seg->end.y        = vertE->y;
			seg->end.l        = 1.0;
			seg++;

			// printf(", B: %5d", angle);
		}

		if ( sideLeft ) {
			seg->Data.start   = lineDef->end;
			seg->Data.end     = lineDef->start;
			seg->Data.angle   = ( BAM ) ( angle + BAM180 );
			seg->Data.lineDef = ( UINT16 ) i;
			seg->Data.flip    = 1;
			seg->LineDef      = lineDef;
			seg->Sector       = sideLeft->sector;
			seg->DontSplit    = split;
			seg->start.x      = vertE->x;
			seg->start.y      = vertE->y;
			seg->start.l      = 0.0;
			seg->end.x        = vertS->x;
			seg->end.y        = vertS->y;
			seg->end.l        = 1.0;
			seg++;

			// printf(", C: %5d", (BAM) (angle + BAM180));
		}
	}

	segCount = seg - segStart;

	return segStart;
}

//----------------------------------------------------------------------------
//  Calculate the set of variables used frequently that are based on the
//    currently selected SEG to be used as a partition line.
//----------------------------------------------------------------------------

// static void ComputeStaticVariables ( SEG *pSeg )
static void ComputeStaticVariables (SEG *list, int offset) {
	SEG *pSeg = &list[offset];

	FUNCTION_ENTRY ( NULL, "ComputeStaticVariables", true );
#if defined ( DIAGNOSTIC )
	int different = 0;
#endif

	if ( pSeg->final == false ) {

		/* if (pSeg->Data.lineDef = 0xFFFF) {
		   currentAlias = 0;
		   currentSide = NULL;
		   } else { */
		currentAlias   = lineDefAlias [ pSeg->Data.lineDef ];
		currentSide    = sideInfo ? sideInfo [ currentAlias ] : NULL;	
		// }
	
		wVertex *vertS = &newVertices [ pSeg->AliasFlip ? pSeg->Data.end : pSeg->Data.start ];
		wVertex *vertE = &newVertices [ pSeg->AliasFlip ? pSeg->Data.start : pSeg->Data.end ];

/*
		if ( (int) lrint(pSeg->start.x) != (int) vertS->x) {
			printf("ERROR %f %d\n", pSeg->start.x, vertS->x);
			printf("ERROR %f %d\n", pSeg->start.x, vertE->x);
		}
*/
		

		X     = vertS->x;
		Y     = vertS->y;
		DX    = vertE->x - vertS->x;
		DY    = vertE->y - vertS->y;

#if defined ( DIAGNOSTIC )
		if (vertS != vertE) {
		        different++;
		}
#endif


	} else {

		currentAlias = 0;

		X     = pSeg->start.x;
		Y     = pSeg->start.y;
		DX    = pSeg->end.x - pSeg->start.x;
		DY    = pSeg->end.y - pSeg->start.y;

	}

	H = ( DX * DX ) + ( DY * DY );

#if defined ( DIAGNOSTIC )
	if (((int) DX == 0 ) && ((int) DY == 0 )) {
		fprintf ( stderr, "\nX=%f, Y=%f, DX=%f, DY=%f, H=%f \n", X, Y, DX, DY, H);
		fprintf ( stderr, "DX & DY are both 0!\n" );
		fprintf ( stderr, "Offset: %d, different: %d\n", offset, different);
	
		exit(0);
	}
#endif

	ANGLE = pSeg->Data.angle;
}

//----------------------------------------------------------------------------
//  Determine if the given SEG is co-linear (ie: they lie on the same line)
//    with the currently selected partition.
//----------------------------------------------------------------------------

static bool CoLinear ( SEG *seg ) {
	FUNCTION_ENTRY ( NULL, "CoLinear", true );

	// If they're not at the same angle ( ñ180ø ), bag it
	if (( ANGLE & ANGLE_MASK ) != ( seg->Data.angle & ANGLE_MASK )) return false;

	// Do the math stuff
	if ( DX == 0.0 ) return ( seg->start.x == X ) ? true : false;
	if ( DY == 0.0 ) return ( seg->start.y == Y ) ? true : false;

	// Rotate vertS about (X,Y) by é degrees to get y offset
	//   Y = Hùsin(é)           ³  1  0  0 ³³ cos(é)  -sin(é)  0 ³
	//   X = Hùcos(é)    ³x y 1³³  0  1  0 ³³ sin(é)   cos(é)  0 ³
	//   H = (Xý+Yý)^«         ³ -X -Y  1 ³³   0         0    1 ³

	return ( DX * ( seg->start.y - Y ) == DY * ( seg->start.x - X )) ? true : false;
}

//----------------------------------------------------------------------------
//  Given a list of SEGs, determine the bounding rectangle.
//----------------------------------------------------------------------------

static void FindBounds ( wBound *bound, SEG *seg, int noSegs ) {
	FUNCTION_ENTRY ( NULL, "FindBounds", true );

	bound->minx = bound->maxx = ( INT16 ) lrint ( seg->start.x );
	bound->miny = bound->maxy = ( INT16 ) lrint ( seg->start.y );

	for ( int i = 0; i < noSegs; i++ ) {

		int startX = lrint ( seg [i].start.x );
		int endX   = lrint ( seg [i].end.x );
		int startY = lrint ( seg [i].start.y );
		int endY   = lrint ( seg [i].end.y );

		int loX = startX, hiX = startX;
		if ( loX < endX ) hiX = endX; else loX = endX;
		int loY = startY, hiY = startY;
		if ( loY < endY ) hiY = endY; else loY = endY;

		if ( loX < bound->minx ) bound->minx = ( INT16 ) loX;
		if ( hiX > bound->maxx ) bound->maxx = ( INT16 ) hiX;
		if ( loY < bound->miny ) bound->miny = ( INT16 ) loY;
		if ( hiY > bound->maxy ) bound->maxy = ( INT16 ) hiY;
	}
}

//----------------------------------------------------------------------------
//  Determine which side of the partition line the given SEG lies.  A quick
//    check is made based on the sector containing the SEG.  If the sector
//    is split by the partition, a more detailed examination is made using
//    _WhichSide.
//
//    Returns:
//       -1 - SEG is on the left of the partition
//        0 - SEG is split by the partition
//       +1 - SEG is on the right of the partition
//----------------------------------------------------------------------------

static int _WhichSide ( SEG *seg, sBSPOptions *options) {
	FUNCTION_ENTRY ( NULL, "_WhichSide", true );

	sVertex *vertS = &seg->start;
	sVertex *vertE = &seg->end;
	double y1, y2;

	if ( DX == 0.0 ) {
		if ( DY > 0.0 ) {
			y1 = ( lrint ( X ) - lrint ( vertS->x )),    y2 = ( lrint ( X ) - lrint ( vertE->x ));
		} else {
			y1 = ( lrint ( vertS->x ) - lrint ( X )),    y2 = ( lrint ( vertE->x ) - lrint ( X ));
		}
	} else if ( DY == 0.0 ) {
		if ( DX > 0.0 ) {
			y1 = ( lrint ( vertS->y ) - lrint ( Y )),    y2 = ( lrint ( vertE->y ) - lrint ( Y ));
		} else {
			y1 = ( lrint ( Y ) - lrint ( vertS->y )),    y2 = ( lrint ( Y ) - lrint ( vertE->y ));
		}
	} else {

		y1 = DX * ( vertS->y - Y ) - DY * ( vertS->x - X );
		y2 = DX * ( vertE->y - Y ) - DY * ( vertE->x - X );

		if (( y1 * y2 != 0.0 ) && (( fabs ( y1 ) <= H ) || ( fabs ( y2 ) <= H ))) {

			const wLineDefInternal *lineDef = seg->LineDef;
			wVertex *_vertS = &newVertices [ lineDef->start ];
			wVertex *_vertE = &newVertices [ lineDef->end ];

			double dx = _vertE->x - _vertS->x;
			double dy = _vertE->y - _vertS->y;
			double det = dx * DY - dy * DX;

			if ( det != 0.0 ) {

				double num = DX * ( _vertS->y - Y ) - DY * ( _vertS->x - X );
				double l = num / det;

				if ( seg->Data.flip != 0 ) l = 1.0 - l;

				if ( l < vertS->l ) { y1 = 0.0; goto xx; }
				if ( l > vertE->l ) { y2 = 0.0; goto xx; }

				long x = lrint ( _vertS->x + num * dx / det );
				long y = lrint ( _vertS->y + num * dy / det );

				if (( lrint ( vertS->x ) == x ) && ( lrint ( vertS->y ) == y )) y1 = 0.0;
				if (( lrint ( vertE->x ) == x ) && ( lrint ( vertE->y ) == y )) y2 = 0.0;
			}
		}
	}


	if ( fabs ( y1 ) < EPSILON ) y1 = 0.0;
	if ( fabs ( y2 ) < EPSILON ) y2 = 0.0;

xx:

	// If its co-linear, decide based on direction
	if (( y1 == 0.0 ) && ( y2 == 0.0 )) {
		double x1 = DX * ( vertS->x - X ) + DY * ( vertS->y - Y );
		double x2 = DX * ( vertE->x - X ) + DY * ( vertE->y - Y );
		return ( x1 <= x2 ) ? SIDE_RIGHT : SIDE_LEFT;
	}

	// Otherwise:
	//   Left   -1 : ( y1 >= 0 ) && ( y2 >= 0 )
	//   Both    0 : (( y1 < 0 ) && ( y2 > 0 )) || (( y1 > 0 ) && ( y2 < 0 ))
	//   Right   1 : ( y1 <= 0 ) && ( y2 <= 0 )

	// printf("-- %f %f\n", DX, DY);

	return ( y1 <  0.0 ) ? (( y2 <= 0.0 ) ? SIDE_RIGHT : SIDE_SPLIT ) :
		( y1 == 0.0 ) ? (( y2 <= 0.0 ) ? SIDE_RIGHT : SIDE_LEFT  ) :
		(( y2 >= 0.0 ) ? SIDE_LEFT  : SIDE_SPLIT );
}

static int WhichSide ( SEG *seg, sBSPOptions *options ) {
	FUNCTION_ENTRY ( NULL, "WhichSide", true );

	// Treat split partition/seg differently
	if (( seg->Split == true ) || ( currentAlias == 0 )) {
		return _WhichSide ( seg, options );
	}

	// See if partition & seg lie on the same line
	int alias = lineDefAlias [ seg->Data.lineDef ];
	if ( alias == currentAlias ) {
		return seg->AliasFlip ^ SIDE_RIGHT;
	}

	// See if we've already categorized the LINEDEF for this SEG
	int side = currentSide [ seg->Data.lineDef ];
	if ( IS_LEFT_RIGHT ( side )) return side;

	side = _WhichSide ( seg, options );

	if ( seg->Split == false ) {
		currentSide [ seg->Data.lineDef ] = ( char ) side;
	}

	return side;
}

#if defined ( DEBUG )

static int dbgWhichSide ( SEG *seg, sBSPOptions *options ) {
	FUNCTION_ENTRY ( NULL, "dbgWhichSide", true );

	int side = WhichSide ( seg, options );
	if ( side != _WhichSide ( seg, options )) {
		ERROR ( "WhichSide is wigging out!" );
	}
	return side;
}

#define WhichSide dbgWhichSide

#endif

//----------------------------------------------------------------------------
//  Create a list of aliases vs LINEDEFs that indicates which side of a given
//    alias a LINEDEF is on.
//----------------------------------------------------------------------------

static void CreateSideInfo ( DoomLevel *level ) {
	FUNCTION_ENTRY ( NULL, "CreateSideInfo", true );

	long size = ( sizeof ( char * ) + level->LineDefCount ()) * ( long ) noAliases;
	char *temp = new char [ size ];
	sideInfoEntries = size;

	sideInfo = ( char ** ) temp;
	memset ( temp, 0, sizeof ( char * ) * noAliases );

	temp += sizeof ( char * ) * noAliases;
	memset ( temp, SIDE_UNKNOWN, level->LineDefCount () * noAliases );

	for ( int i = 0; i < noAliases; i++ ) {
		sideInfo [i] = ( char * ) temp;
		temp += level->LineDefCount ();
	}
}

//----------------------------------------------------------------------------
//  Return an index for a vertex at (x,y).  If an existing vertex exists,
//    return it, otherwise, create a new one if room is left.
//----------------------------------------------------------------------------

static int AddVertex ( int x, int y ) {
	FUNCTION_ENTRY ( NULL, "AddVertex", true );

	
	for ( int i = 0; i < noVertices; i++ ) {
		if (( newVertices [i].x == x ) && ( newVertices [i].y == y )) return i;
	}
	

	// let's try reverse search, it should work better!
	// If this was accessed using a hash table, we could def. speed things up!
	/*
	for (int i = noVertices - 1; i--; i >= 0) {
		if (( newVertices [i].x == x ) && ( newVertices [i].y == y )) return i;
	}
*/

	// it doesn't go faster, unfortunatly :|

	if ( noVertices == maxVertices ) {
		maxVertices = ( 102 * maxVertices ) / 100 + 1;
		// Memory handling error, very unlikely :D
		// newVertices = ( wVertex * ) realloc ( newVertices, sizeof ( wVertex ) * maxVertices );
		wVertex  *newVerticesReallocated = ( wVertex * ) realloc ( newVertices, sizeof ( wVertex ) * maxVertices );

		if (newVerticesReallocated) {
			newVertices = newVerticesReallocated;	
		} else {
			printf("FATAL ERROR: Out of memory in AddVertex()");
		}
	}

	newVertices [ noVertices ].x = ( UINT16 ) x;
	newVertices [ noVertices ].y = ( UINT16 ) y;
	
	// printf("\nAdded vertex #%3d (%d,%d)\n", noVertices + 1, x, y);

	return noVertices++;
}

//----------------------------------------------------------------------------
//  Sort two SEGS so that the one with the lowest numbered LINEDEF is first.
//----------------------------------------------------------------------------

static int SortByLineDef ( const void *ptr1, const void *ptr2 ) {
	FUNCTION_ENTRY ( NULL, "SortByLineDef", true );

	int dif1 = (( SEG * ) ptr1)->Sector - (( SEG * ) ptr2)->Sector;
	if ( dif1 ) return dif1;

	int dif = (( SEG * ) ptr1)->Data.lineDef - (( SEG * ) ptr2)->Data.lineDef;
	if ( dif ) return dif;

	return (( SEG * ) ptr1)->Data.flip - (( SEG * ) ptr2)->Data.flip;
}

//----------------------------------------------------------------------------
//  Create a SSECTOR and record the index of the 1st SEG and the total number
//  of SEGs.
//----------------------------------------------------------------------------

static UINT16 CreateSSector ( SEG *segs, int noSegs ) {
	FUNCTION_ENTRY ( NULL, "CreateSSector", true );

	// if ( ssectorsLeft-- == 0 ) {
	if (ssectorCount == ssectorPoolEntries) {
		int delta     = ( 3 * ssectorCount ) / 100 + 1;

		// mem error fix
		wSSector *ssectorPoolRealloc = ( wSSector * ) realloc ( ssectorPool, sizeof ( wSSector ) * ( ssectorCount + delta ));

		if (ssectorPoolRealloc) {
			ssectorPool = ssectorPoolRealloc;
		} else {
			printf("FATAL ERROR: Out of memory in CreateSSector()");		
		}

		//ssectorPool   = ( wSSector * ) realloc ( ssectorPool, sizeof ( wSSector ) * ( ssectorCount + delta ));
		ssectorsLeft += delta;
		ssectorPoolEntries = ssectorCount + delta;
	}

	// Splits may have 'upset' the lineDef ordering - some special effects
	//   assume the SEGS appear in the same order as the LINEDEFS
	if ( noSegs > 1 ) {
		qsort ( segs, noSegs, sizeof ( SEG ), SortByLineDef );
	}

	int count = noSegs;
	int first = segs - segStart;

#if defined ( DIAGNOSTIC )
	bool errors = false;
	for ( int i = 0; i < noSegs; i++ ) {
		double dx = segs [i].end.x - segs [i].start.x;
		double dy = segs [i].end.y - segs [i].start.y;
		double h = ( dx * dx ) + ( dy * dy );
		for ( int j = 0; j < noSegs; j++ ) {
			double y1 = ( dx * ( segs [j].start.y - segs [i].start.y ) - dy * ( segs [j].start.x - segs [i].start.x )) / h;
			double y2 = ( dx * ( segs [j].end.y - segs [i].start.y ) - dy * ( segs [j].end.x - segs [i].start.x )) / h;
			if (( y1 > 0.5 ) || ( y2 > 0.5 )) errors = true;
		}
	}
	if ( errors == true ) {
		fprintf ( stdout, "SSECTOR [%d]:\n", ssectorCount );
		double minx = segs[0].start.x;
		double miny = segs[0].start.y;
		for ( int i = 0; i < noSegs; i++ ) {
			if ( segs[i].start.x < minx ) minx = segs[i].start.x;
			if ( segs[i].start.y < miny ) miny = segs[i].start.y;
			if ( segs[i].end.x < minx ) minx = segs[i].end.x;
			if ( segs[i].end.y < miny ) miny = segs[i].end.y;
		}
		for ( int i = 0; i < noSegs; i++ ) {
			fprintf ( stdout, "  [%d] (%8.1f,%8.1f)-(%8.1f,%8.1f)  S:%5d  LD:%5d\n", i, segs[i].start.x - minx, segs[i].start.y - miny, segs[i].end.x - minx, segs[i].end.y - miny, segs[i].Sector, segs[i].Data.lineDef );
		}
		for ( int i = 0; i < noSegs; i++ ) {
			double dx = segs [i].end.x - segs [i].start.x;
			double dy = segs [i].end.y - segs [i].start.y;
			double h = ( dx * dx ) + ( dy * dy );
			for ( int j = 0; j < noSegs; j++ ) {
				double y1 = ( dx * ( segs [j].start.y - segs [i].start.y ) - dy * ( segs [j].start.x - segs [i].start.x )) / h;
				double y2 = ( dx * ( segs [j].end.y - segs [i].start.y ) - dy * ( segs [j].end.x - segs [i].start.x )) / h;
				if (( y1 > 0.5 ) || ( y2 > 0.5 )) fprintf ( stdout, "<< ERROR - seg[%d] is not to the right of seg[%d] (%9.2f,%9.2f) >>\n", j, i, y1, y2 );
			}
		}
		fprintf ( stdout, "\n" );
		exit(0);
	}
#endif

	// Eliminate zero length SEGs and assign vertices
	for ( int i = 0; i < noSegs; i++ ) {
#if defined ( DEBUG ) 
		if (( fabs ( segs->start.x - segs->end.x ) < EPSILON ) && ( fabs ( segs->start.y - segs->end.y ) < EPSILON )) {
			fprintf ( stderr, "Eliminating 0 length SEG from list\n" );
			memcpy ( segs, segs + 1, sizeof ( SEG ) * ( noSegs - i - 1 ));
			count--;
			continue;
		}
#endif
		// here we need to check if it is a 1086 type linedef!
		/*
		   if (segs->LineDef->type == 1086) {
		   printf("Skipping seg\n");
		//memcpy ( segs, segs + 1, sizeof ( SEG ) * ( noSegs - i - 1 ));
		count--;
		continue;
		}
		*/

		segs->Data.start = ( UINT16 ) AddVertex ( lrint ( segs->start.x ), lrint ( segs->start.y ));
		segs->Data.end   = ( UINT16 ) AddVertex ( lrint ( segs->end.x ),   lrint ( segs->end.y ));
		segs++;
	}
	if ( count == 0 ) {
		WARNING ( "No valid SEGS left in list!" );
	}

	wSSector *ssec = &ssectorPool [ssectorCount];
	ssec->num   = ( UINT16 ) count;
	ssec->first = ( UINT16 ) first;

	return ( UINT16 ) ssectorCount++;
}
	
static int SortByAngle ( const void *ptr1, const void *ptr2 ) {
	FUNCTION_ENTRY ( NULL, "SortByAngle", true );

	int dif = ( ANGLE_MASK & (( SEG * ) ptr1)->Data.angle ) - ( ANGLE_MASK & (( SEG * ) ptr2)->Data.angle );
	if ( dif ) return dif;
	dif = (( SEG * ) ptr1)->Data.lineDef - (( SEG * ) ptr2)->Data.lineDef;
	if ( dif ) return dif;
	return (( SEG * ) ptr1)->Data.flip - (( SEG * ) ptr2)->Data.flip;
}

//----------------------------------------------------------------------------
//  Create a list of aliases.  These are all the unique lines within the map.
//    Each linedef is assigned an alias.  All subsequent calculations are
//    based on the aliases rather than the linedefs, since there are usually
//    significantly fewer aliases than linedefs.
//----------------------------------------------------------------------------

static int GetLineDefAliases ( DoomLevel *level, SEG *segs, int noSegs ) {
	FUNCTION_ENTRY ( NULL, "GetLineDefAliases", true );

	// Reserve alias 0
	int noAliases = 1;

	lineDefAlias = new int [ level->LineDefCount () ];
	memset ( lineDefAlias, -1, sizeof ( int ) * ( level->LineDefCount ()));

	SEG **segAlias = new SEG * [ level->LineDefCount () + 2 ];

	qsort ( segs, noSegs, sizeof ( SEG ), SortByAngle );

	int lowIndex  = 1;
	int lastAngle = -1;

	for ( int i = 0; i < noSegs; i++ ) {

		// If the LINEDEF has been covered, skip this SEG
		int *alias = &lineDefAlias [ segs [i].Data.lineDef ];

		if ( *alias == -1 ) {

			// ComputeStaticVariables ( &segs [i] );
			// printf("GLA\n");
			ComputeStaticVariables (segs, i);

			// Compare against existing aliases with the same angle
			int x = lowIndex;
			while ( x < noAliases ) {
				if ( CoLinear ( segAlias [x] )) {
					break;
				}
				x++;
			}

			if ( x >= noAliases ) {
				segAlias [ x = noAliases++ ] = &segs [i];
				if ( lastAngle != ( ANGLE & ANGLE_MASK )) {
					lowIndex = x;
					lastAngle = ANGLE & ANGLE_MASK;
				}
			}

			*alias = x;
		}

		segs [i].AliasFlip = ( segs [i].Data.angle == segAlias [*alias]->Data.angle ) ? 0 : SIDE_FLIPPED;
	}

	delete [] segAlias;

	qsort ( segs, noSegs, sizeof ( SEG ), SortByLineDef );

	return noAliases;
}

#if defined ( DEBUG )

static void DumpSegs ( SEG *seg, int noSegs ) {
	FUNCTION_ENTRY ( NULL, "DumpSegs", true );

	for ( int i = 0; i < noSegs; i++ ) {
		sVertex *vertS = &seg->start;
		sVertex *vertE = &seg->end;
		int alias = lineDefAlias [ seg->Data.lineDef ];
		WARNING (( lineUsed [ alias ] ? "*" : " " ) <<
				" lineDef: " << seg->Data.lineDef <<
				" (" << vertS->x << "," << vertS->y << ") -" <<
				" (" << vertE->x << "," << vertE->y << ")" );
		seg++;
	}
}

#endif

//----------------------------------------------------------------------------
//
//  Partition line:
//    DXùx - DYùy + C = 0               ³ DX  -DY ³ ³-C³
//  rSeg line:                          ³         ³=³  ³
//    dxùx - dyùy + c = 0               ³ dx  -dy ³ ³-c³
//
//----------------------------------------------------------------------------

static void DivideSeg ( SEG *rSeg, SEG *lSeg ) {
	FUNCTION_ENTRY ( NULL, "DivideSeg", true );

	const wLineDefInternal *lineDef = rSeg->LineDef;
	wVertex *vertS = &newVertices [ lineDef->start ];
	wVertex *vertE = &newVertices [ lineDef->end ];

	// Minimum precision required to avoid overflow/underflow:
	//   dx, dy  - 16 bits required
	//   c       - 33 bits required
	//   det     - 32 bits required
	//   x, y    - 50 bits required

	double dx  = vertE->x - vertS->x;
	double dy  = vertE->y - vertS->y;
	double num = DX * ( vertS->y - Y ) - DY * ( vertS->x - X );
	double det = dx * DY - dy * DX;

	if ( det == 0.0 ) {
		ERROR ( "SEG is parallel to partition line" );
	}

	double l = num / det;
	double x = vertS->x + num * dx / det;
	double y = vertS->y + num * dy / det;

	if ( rSeg->final == true ) {
		x = lrint ( x );
		y = lrint ( y );
#if defined ( DEBUG )
		if ((( rSeg->start.x == x ) && ( rSeg->start.y == y )) ||
				(( lSeg->start.x == x ) && ( lSeg->start.y == y ))) {
			fprintf ( stderr, "\nNODES: End point duplicated in DivideSeg: LineDef #%d", rSeg->Data.lineDef );
			fprintf ( stderr, "\n       Partition: from (%f,%f) to (%f,%f)", X, Y, X + DX, Y + DY );
			fprintf ( stderr, "\n       LineDef: from (%d,%d) to (%d,%d) split at (%f,%f)", vertS->x, vertS->y, vertE->x, vertE->y, x, y );
			fprintf ( stderr, "\n       SEG: from (%f,%f) to (%f,%f)", rSeg->start.x, rSeg->start.y, rSeg->end.x, rSeg->end.y );
			fprintf ( stderr, "\n       dif: (%f,%f)  (%f,%f)", rSeg->start.x - x, rSeg->start.y - y, rSeg->end.x - x, rSeg->end.y - y );
		}
#endif
	}

	// Determine which sided of the partition line the start point is on
	double sideS = DX * ( rSeg->start.y - Y ) - DY * ( rSeg->start.x - X );

	// Get the correct endpoint of the base LINEDEF for the offset calculation
	if ( rSeg->Data.flip ) vertS = vertE;
	if ( rSeg->Data.flip ) l = 1.0 - l;

	rSeg->Split = true;
	lSeg->Split = true;

	// Fill in the parts of lSeg & rSeg that have changed
	if ( sideS < 0.0 ) {
#if defined ( DEBUG )
		if (( lrint ( x ) == lrint ( lSeg->start.x )) && ( lrint ( y ) == lrint ( lSeg->start.y ))) {
			fprintf ( stderr, "DivideSeg: split didn't work (%10.3f,%10.3f) == L(%10.3f,%10.3f) - %d\n", x, y, lSeg->start.x, lSeg->start.y, currentAlias );
		} else if (( l < lSeg->start.l ) || ( l > lSeg->end.l )) {
			fprintf ( stderr, "DivideSeg: warning - split is outside line segment (%7.5f-%7.5f) %7.5f\n", lSeg->start.l, lSeg->end.l, l );
		}
#endif
		rSeg->end.x       = x;
		rSeg->end.y       = y;
		rSeg->end.l       = l;
		lSeg->start.x     = x;
		lSeg->start.y     = y;
		lSeg->start.l     = l;
		lSeg->Data.offset = ( UINT16 ) ( hypot (( double ) ( x - vertS->x ), ( double ) ( y - vertS->y )) + 0.5 );
	} else {
#if defined ( DEBUG )
		if (( lrint ( x ) == lrint ( rSeg->start.x )) && ( lrint ( y ) == lrint ( rSeg->start.y ))) {
			fprintf ( stderr, "DivideSeg: split didn't work (%10.3f,%10.3f) == R(%10.3f,%10.3f) - %d\n", x, y, rSeg->start.x, rSeg->start.y, currentAlias  );
		} else if (( l < lSeg->start.l ) || ( l > lSeg->end.l )) {
			fprintf ( stderr, "DivideSeg: warning - split is outside line segment (%7.5f-%7.5f) %7.5f\n", lSeg->start.l, lSeg->end.l, l );
		}
#endif
		lSeg->end.x       = x;
		lSeg->end.y       = y;
		lSeg->end.l       = l;
		rSeg->start.x     = x;
		rSeg->start.y     = y;
		rSeg->start.l     = l;
		rSeg->Data.offset = ( UINT16 ) ( hypot (( double ) ( x - vertS->x ), ( double ) ( y - vertS->y )) + 0.5 );
	}

#if defined ( DEBUG )
	if ( _WhichSide ( rSeg, options ) != SIDE_RIGHT ) {
		fprintf ( stderr, "DivideSeg: %s split invalid\n", "right" );
	}
	if ( _WhichSide ( lSeg, options ) != SIDE_LEFT ) {
		fprintf ( stderr, "DivideSeg: %s split invalid\n", "left" );
	}
#endif
}

//----------------------------------------------------------------------------
//  Split the list of SEGs in two and adjust each copy to the appropriate
//    values.
//----------------------------------------------------------------------------

SEG *meh = NULL;

static void SplitSegs ( SEG *segs, int noSplits ) {
	
	FUNCTION_ENTRY ( NULL, "SplitSegs", true );

	segCount += noSplits;
	
	if ( segCount > maxSegs ) {
		fprintf ( stderr, "\nError: Too many SEGs have been split: %d > %d \n", segCount, maxSegs );
		exit ( -1 );
	}
	
/*
	if (segCount >= maxSegs) {

		int memwtf = segs - segStart;

		SEG *s = new SEG [maxSegs + noSplits];
		memset (s, 0, sizeof ( SEG ) * (maxSegs + noSplits));
		memcpy(s, segStart, maxSegs * sizeof(SEG));

		delete [] segStart;
		segStart = s;

		// printf("\nexpanding\n\n");

		maxSegs += noSplits;
		segs = segStart + memwtf;
	}
*/

	int count = segCount - ( segs - segStart ) - noSplits;
	memmove ( segs + noSplits, segs, count * sizeof ( SEG ));

	for ( int i = 0; i < noSplits; i++ ) {
		DivideSeg ( segs, segs + noSplits );
		segs++;
	}
}

// static void SortSegs ( SEG *pSeg, SEG *seg, int noSegs, int *noLeft, int *noRight, sBSPOptions *options )
static void SortSegs ( int inSeg, SEG *seg, int noSegs, int *noLeft, int *noRight, sBSPOptions *options ) {
	FUNCTION_ENTRY ( NULL, "SortSegs", true );

	if (inSeg == -1) {
		return;
	}

	SEG *pSeg = &seg[inSeg];

	int count [3];

	if ( pSeg == NULL ) {
#if defined ( DEBUG )
		for ( int x = 0; x < noSegs; x++ ) {
			// Make sure that all SEGs are actually on the right side of each other
			// ComputeStaticVariables ( &seg [x] );

			ComputeStaticVariables(seg, x);

			if (( fabs ( DX ) < EPSILON ) && ( fabs ( DY ) < EPSILON )) continue;

			count [0] = count [1] = count [2] = 0;
			for ( int i = 0; i < noSegs; i++ ) {
				count [( seg [i].Side = _WhichSide ( &seg [i], options )) + 1 ]++;
			}

			if (( count [0] * count [2] ) || count [1] ) {
				DumpSegs ( seg, noSegs );
				ERROR ( "Something weird is going on! (" << count [0] << "|" << count [1] << "|" << count [2] << ") " << noSegs );
				break;
			}
		}
#endif
		*noRight  = noSegs;
		*noLeft   = 0;
		return;
	}

	// ComputeStaticVariables ( pSeg );
	//
	ComputeStaticVariables(seg, inSeg);

	count [0] = count [1] = count [2] = 0;
	int i;
	for ( i = 0; i < noSegs; i++ ) {
		count [( seg [i].Side = WhichSide ( &seg [i], options )) + 1 ]++;
	}

	ASSERT (( count [0] * count [2] != 0 ) || ( count [1] != 0 ));

	*noLeft  = count [0];
	*noRight = count [2];

	SEG *rSeg = seg;
	for ( i = 0; seg [i].Side == SIDE_RIGHT; i++ ) rSeg++;

	if (( i < count [2] ) || count [1] ) {
		SEG *sSeg = tempSeg;
		SEG *lSeg = sSeg + count [1];
		for ( ; i < noSegs; i++ ) {
			switch ( seg [i].Side ) {
				case SIDE_LEFT  : *lSeg++ = seg [i];		break;
				case SIDE_SPLIT : *sSeg++ = seg [i];		break;
				case SIDE_RIGHT : *rSeg++ = seg [i];		break;
			}
		}
		memcpy ( rSeg, tempSeg, ( noSegs - count [2] ) * sizeof ( SEG ));
	}

	if ( count [1] != 0 ) {
		SplitSegs ( &seg [ *noRight ], count [1] );
		*noLeft  += count [1];
		*noRight += count [1];
	}

	return;
}

//----------------------------------------------------------------------------
//  Use the requested algorithm to select a partition for the list of SEGs.
//    After a valid partition is selected, the SEGs are re-ordered.  All SEGs
//    to the right of the partition are placed first, then those that will
//    be split, followed by those that are to the left.
//----------------------------------------------------------------------------

static bool ChoosePartition ( SEG *seg, int noSegs, int *noLeft, int *noRight, sBSPOptions *options, DoomLevel *level, int *width, int *wideSegs) {
	FUNCTION_ENTRY ( NULL, "ChoosePartition", true );

	bool check = true;

retry:

	if ( seg->final == false ) {
		memcpy ( lineChecked, lineUsed, sizeof ( char ) * noAliases );
	} else {
		memset ( lineChecked, 0, sizeof ( char ) * noAliases );
	}

	// Find the best SEG to be used as a partition

	// printf("width: %d\n", *width);
	// SEG *pSeg = PartitionFunction ( seg, noSegs, options, level, width, wideSegs);
	int partitionOffset = PartitionFunction ( seg, noSegs, options, level, width, wideSegs);


	/*
	 * TREE_METRIC_SUBSECTORS
	 * TREE_METRIC_SEGS 1
	 * TREE_METRIC_NODES 2
	 */
	earlyExit = false;
	/*
	   if (options->Metric == TREE_METRIC_SEGS) {
	   if (noSegs > bestSegCount) {	// abort if more segs than best
	   earlyExit = true;
	   } else if (noSegs == bestSegCount) {
	   if (level->SubSectorCount() > bestSubSectorCount) { // abort if segs same and more subsectors
	   earlyExit = true;
	   } else if (level->SubSectorCount() == bestSubSectorCount) { // abort if same segs, subsectors, but more nodes
	   if (level->NodeCount() >= bestNodeCount) {
	   earlyExit = true;
	   }
	   }
	   }

	   } else if (options->Metric == TREE_METRIC_SUBSECTORS) {
	   if (level->SubSectorCount() > bestSubSectorCount) {
	   earlyExit = true;
	   } else if (level->SubSectorCount() == bestSubSectorCount) {
	   if (noSegs > bestSegCount) {
	   earlyExit = true;
	   } else if (noSegs == bestSegCount) {
	   if (level->NodeCount() >= bestNodeCount) {
	   earlyExit = true;
	   }
	   }
	   }

	   } else if (options->Metric == TREE_METRIC_NODES) {
	   if (level->NodeCount() > bestNodeCount) {
	   earlyExit = true;
	   else if (level->NodeCount() == bestNodeCount) {
	   if (level->SubSectorCount() > bestSubSectorCount) {
	   earlyExit = true;
	   } else if (level->SubSectorCount() == bestSubSectorCount) {
	   if (noSegs >= bestSegCount) {
	   earlyExit = true;
	   }
	   }

	   }
	   }

	   if (earlyExit) {
	// printf("early\n");
	return NULL;
	}
	*/

	/*
	   if (noSegs >= bestSegCount) {
	   earlyExit = true;
	   return NULL;
	   }
	   */
	// Resort the SEGS (right followed by left) and do the splits as necessary
	//SortSegs ( pSeg, seg, noSegs, noLeft, noRight, options );
	
	if (partitionOffset != -2) {
		SortSegs ( partitionOffset, seg, noSegs, noLeft, noRight, options );
	}

	// Make sure the set of SEGs is still convex after we convert to integer coordinates
	//if (( pSeg == NULL ) && ( check == true )) {
	if (( partitionOffset == -2 ) && ( check == true )) {
		check = false;
		double error = 0.0;
		for ( int i = 0; i < noSegs; i++ ) {

			int startX = lrint ( seg [i].start.x );
			int startY = lrint ( seg [i].start.y );
			int endX   = lrint ( seg [i].end.x );
			int endY   = lrint ( seg [i].end.y );

			error += fabs ( seg [i].start.x - startX );
			error += fabs ( seg [i].start.y - startY );
			error += fabs ( seg [i].end.x - endX );
			error += fabs ( seg [i].end.y - endY );

			seg [i].start.x = startX;
			seg [i].start.y = startY;
			seg [i].end.x   = endX;
			seg [i].end.y   = endY;
			seg [i].final   = true;
		}
		if ( (error > EPSILON) && (options->SplitHandling != 0)) {
			// printf("error!\n");
			// Force a check of each line
			goto retry;
		}
	}

	// return pSeg ? true : false;
	if (partitionOffset == -2) {
		return false;
	} else {
		return true;
	}
	// return partitionOffset;
}

//----------------------------------------------------------------------------
//  ALGORITHM 1: 'ZenNode Classic'
//    This is the original algorithm used by ZenNode.  It simply attempts
//    to minimize the number of SEGs that are split.  This actually yields
//    very small BSP trees, but usually results in trees that are not well
//    balanced and run deep.
//----------------------------------------------------------------------------

static int AlgorithmFewerSplits ( SEG *segs, int noSegs, sBSPOptions *options, DoomLevel *level, int *width, int *wideSegs ) {
	FUNCTION_ENTRY ( NULL, "AlgorithmFewerSplits", true );

	SEG *pSeg = NULL, *testSeg = segs;
	int count [3];
	int &lCount = count [0], &sCount = count [1], &rCount = count [2];
	// Compute the maximum value maxMetric can possibly reach
	long maxMetric = ( noSegs / 2 ) * ( noSegs - noSegs / 2 );
	long bestMetric = LONG_MIN, bestSplits = LONG_MAX;

	bool diagonal;

	int edgeBalance = noSegs;
	int bestEdgeBalance = noSegs;

	int bestOffset = -2;

	for ( int i = 0; i < noSegs; i++ ) {
		// if ( showProgress && (( i & 15 ) == 0 )) ShowProgress (); obsolete, spammy
		int alias = testSeg->Split ? 0 : lineDefAlias [ testSeg->Data.lineDef ];
		if (( alias == 0 ) || ( lineChecked [ alias ] == false )) {
			lineChecked [ alias ] = -1;
			count [0] = count [1] = count [2] = 0;
			//ComputeStaticVariables ( testSeg );

			ComputeStaticVariables(segs, i);

			if (( fabs ( DX ) < EPSILON ) && ( fabs ( DY ) < EPSILON )) goto next;
			if ( bestMetric < 0 ) for ( int j = 0; j < noSegs; j++ ) {
				count [ WhichSide ( &segs [j], options ) + 1 ]++;
			} else for ( int j = 0; j < noSegs; j++ ) {
				count [ WhichSide ( &segs [j], options ) + 1 ]++;
				if ( sCount >= bestSplits ) goto next; // Added =
			}

			// Only consider SEG if it is not a boundary line
			if ( lCount * rCount + sCount != 0 ) {
				long metric = ( long ) lCount * ( long ) rCount;
				if ( sCount ) {
					long temp = X1 * sCount;
					if ( X2 < temp ) metric = X2 * metric / temp;
					metric -= ( X3 * sCount + X4 ) * sCount;
				}

				if ( (ANGLE & 0x0000) || ( ANGLE & 0x3FFF) || ( ANGLE & 0x7FFF) || ( ANGLE & 0xAFFF)) {
					// metric--;
					diagonal = false;
				} else {
					diagonal = true;
				}

				if ( metric == maxMetric ) {
					// return testSeg;
					return i;
				}

				bool betterBalance;
				bool decentBalance;

				edgeBalance = abs(lCount - rCount);

				if (edgeBalance < bestEdgeBalance) { // children have more equal amount of edges
					betterBalance = true;
					decentBalance = true;
				} else if (edgeBalance == bestEdgeBalance) { // same amount, maybe not diagonal?
					decentBalance = true;				
				} else { // less balanced than our previous best
					betterBalance = false;
					decentBalance = false;
				}

				if ( (metric > bestMetric) 
						|| ((metric == bestMetric) && betterBalance)
						|| ((metric == bestMetric) && !diagonal && decentBalance)
				   ) {
					bestOffset = i;
					pSeg       = testSeg;
					bestSplits = sCount + 2;
					bestMetric = metric;
					bestEdgeBalance = edgeBalance;
				}
			} else if ( alias != 0 ) {
				// Eliminate outer edges of the map from here & down
				*convexPtr++ = alias;
			}
		}
#if defined ( DEBUG )
		else if ( lineChecked [alias] > 0 ) {
			count [0] = count [1] = count [2] = 0;
			ComputeStaticVariables ( testSeg );
			int side = WhichSide ( testSeg, options );
			if (( fabs ( DX ) < EPSILON ) && ( fabs ( DY ) < EPSILON )) continue;
			for ( int j = 0; j < noSegs; j++ ) {
				switch ( WhichSide ( &segs [j], options )) {
					case SIDE_LEFT :
						if ( side == SIDE_RIGHT ) {
							WARNING ( "lineDef " << segs [j].Data.lineDef << " should not to the left of lineDef " << testSeg->Data.lineDef );
						}
						break;
					case SIDE_SPLIT :
						WARNING ( "lineDef " << segs [j].Data.lineDef << " should not be split by lineDef " << testSeg->Data.lineDef );
						break;
					case SIDE_RIGHT :
						if ( side == SIDE_LEFT ) {
							WARNING ( "lineDef " << segs [j].Data.lineDef << " should not to the right of lineDef " << testSeg->Data.lineDef );
						}
						break;
					default :
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

//----------------------------------------------------------------------------
//  ALGORITHM 2: 'ZenNode Quality'
//    This is the 2nd algorithm used by ZenNode.  It attempts to keep the
//    resulting BSP tree balanced based on the number of sectors on each side of
//    the partition line in addition to the number of SEGs.  This seems more
//    reasonable since a given SECTOR is usually made up of one or more SSECTORS.
//----------------------------------------------------------------------------

int sortTotalMetric ( const void *ptr1, const void *ptr2 ) {
	FUNCTION_ENTRY ( NULL, "sortTotalMetric", true );

	int dif;
	dif = (( sScoreInfo * ) ptr1)->invalid - (( sScoreInfo * ) ptr2)->invalid;
	if ( dif ) return dif;
	dif = (( sScoreInfo * ) ptr1)->total - (( sScoreInfo * ) ptr2)->total;
	if ( dif ) return dif;
	dif = (( sScoreInfo * ) ptr1)->index - (( sScoreInfo * ) ptr2)->index;
	return dif;
}

int sortMetric1 ( const void *ptr1, const void *ptr2 ) {
	FUNCTION_ENTRY ( NULL, "sortMetric1", true );

	if ((( sScoreInfo * ) ptr2)->metric1 < (( sScoreInfo * ) ptr1)->metric1 ) return -1;
	if ((( sScoreInfo * ) ptr2)->metric1 > (( sScoreInfo * ) ptr1)->metric1 ) return  1;
	if ((( sScoreInfo * ) ptr2)->metric2 < (( sScoreInfo * ) ptr1)->metric2 ) return -1;
	if ((( sScoreInfo * ) ptr2)->metric2 > (( sScoreInfo * ) ptr1)->metric2 ) return  1;
	return (( sScoreInfo * ) ptr1)->index - (( sScoreInfo * ) ptr2)->index;
}

int sortMetric2 ( const void *ptr1, const void *ptr2 ) {
	FUNCTION_ENTRY ( NULL, "sortMetric2", true );

	if ((( sScoreInfo * ) ptr2)->metric2 < (( sScoreInfo * ) ptr1)->metric2 ) return -1;
	if ((( sScoreInfo * ) ptr2)->metric2 > (( sScoreInfo * ) ptr1)->metric2 ) return  1;
	if ((( sScoreInfo * ) ptr2)->metric1 < (( sScoreInfo * ) ptr1)->metric1 ) return -1;
	if ((( sScoreInfo * ) ptr2)->metric1 > (( sScoreInfo * ) ptr1)->metric1 ) return  1;
	return (( sScoreInfo * ) ptr1)->index - (( sScoreInfo * ) ptr2)->index;
}


// static SEG *AlgorithmBalancedTree( SEG *segs, int noSegs, sBSPOptions *options, DoomLevel *level, int *width, int *wideSegs )
int AlgorithmBalancedTree( SEG *segs, int noSegs, sBSPOptions *options, DoomLevel *level, int *width, int *wideSegs ) {
	FUNCTION_ENTRY ( NULL, "AlgorithmBalancedTree", true );
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
	SEG *testSeg = segs;
	int count [3], noScores = 0, rank, i;
	int &lCount = count [0], &sCount = count [1], &rCount = count [2];

	memset ( score, -1, sizeof ( sScoreInfo ) * noAliases );
	score [0].index = 0;

	for ( i = 0; i < noSegs; i++ ) {
		// if ( showProgress && (( i & 15 ) == 0 )) ShowProgress (); // obsolete, spammy
		int alias = testSeg->Split ? 0 : lineDefAlias [ testSeg->Data.lineDef ];

		if (( alias == 0 ) || ( lineChecked [ alias ] == false )) {

			lineChecked [ alias ] = -1;
			count [0] = count [1] = count [2] = 0;

			// ComputeStaticVariables ( testSeg );
			
			// printf("AB tree nosegs=%d, i=%d\n", noSegs, i);
			ComputeStaticVariables(segs, i);

			if (( fabs ( DX ) < EPSILON ) && ( fabs ( DY ) < EPSILON )) goto next;
			sScoreInfo *curScore = &score [noScores];
			curScore->invalid = 0;
			memset ( usedSector, 0, sizeof ( UINT8 ) * sectorCount );
			SEG *destSeg = segs;
			for ( int j = 0; j < noSegs; j++, destSeg++ ) {
				// printf ("%d %d\n", j, noSegs);
				switch ( WhichSide ( destSeg, options )) {
					case SIDE_LEFT  : lCount++; usedSector [ destSeg->Sector ] |= 0xF0;	break;
					case SIDE_SPLIT : if ( destSeg->DontSplit ) curScore->invalid++;
								  sCount++; usedSector [ destSeg->Sector ] |= 0xFF;	break;
					case SIDE_RIGHT : rCount++; usedSector [ destSeg->Sector ] |= 0x0F;	break;
				}
			}
			// Only consider SEG if it is not a boundary line
			if ( lCount * rCount + sCount ) {
				int lsCount = 0, rsCount = 0, ssCount = 0;
				for ( int j = 0; j < sectorCount; j++ ) {
					switch ( usedSector [j] ) {
						case 0xF0 : lsCount++;	break;
						case 0xFF : ssCount++;	break;
						case 0x0F : rsCount++;	break;
					}
				}

				curScore->index = i;
				curScore->metric1 = ( long ) ( lCount + sCount ) * ( long ) ( rCount + sCount );
				curScore->metric2 = ( long ) ( lsCount + ssCount ) * ( long ) ( rsCount + ssCount );

				if ( sCount ) {
					long temp = X1 * sCount;
					if ( X2 < temp ) curScore->metric1 = X2 * curScore->metric1 / temp;
					/*
					 * 27940 curScore->metric1 -= ( X3 * sCount * (sCount / 2 ) + X4 ) * sCount;
					 * 27923 curScore->metric1 -= ( X3 * sCount * (sCount / 3 ) + X4 ) * sCount;
					 * 27922 curScore->metric1 -= ( X3 * sCount * (sCount / 3 ) + X4 ) * sCount;
					 *       curScore->metric1 = 0xffffffff - abs(lCount -rCount) ;
					 * */ 
					curScore->metric1 -= ( X3 * sCount * (sCount / 3 ) + X4 ) * sCount;
				} else if (options->SplitReduction & 0x01) {
					curScore->metric1 = 0xffffffff - abs(lCount -rCount) ;
				}

				// SubsSctorFactor

				int ssf = 1;

				if (noSegs < 200) {
					ssf = (200 / noSegs);
				}

				ssf = 1;

				// printf("ssf %d\n", ssf);

				if ( ssCount ) {
					long temp = X1 * ssCount;
					if ( X2 < temp ) curScore->metric2 = X2 * curScore->metric2 / temp;
					/*
					 * 27940 curScore->metric2 -= ( X3 * ssCount + X4 ) * sCount / 2;
					 * 27923 curScore->metric2 -= ( X3 * ssCount + X4 ) * sCount / 2;
					 * 27922 curScore->metric2 -= ( X3 * ssCount + X4 ) * sCount / 2;
					 *       curScore->metric2 = 0xffffffff - abs(lsCount -rsCount);
					 */
					curScore->metric2 -= ( X3 * ssCount * ssf + X4 ) * sCount;
					// curScore->metric2 -= ( X3 * ssCount + X4 ) * sCount * 5;
				} else if (options->SplitReduction & 0x02) {
					curScore->metric2 = 0xffffffff - abs(lsCount -rsCount);
				}

				noScores++;
			} else if ( alias != 0 ) {
				// Eliminate outer edges of the map
				*convexPtr++ = alias;
			}
		}
#if defined ( DEBUG )
		else if ( lineChecked [alias] > 0 ) {
			count [0] = count [1] = count [2] = 0;
			ComputeStaticVariables ( testSeg );
			int side = WhichSide ( testSeg, options );
			if (( fabs ( DX ) < EPSILON ) && ( fabs ( DY ) < EPSILON )) continue;
			for ( int j = 0; j < noSegs; j++ ) {
				switch ( WhichSide ( &segs [j], options )) {
					case SIDE_LEFT :
						if ( side == SIDE_RIGHT ) {
							WARNING ( "lineDef " << segs [j].Data.lineDef << " should not to the left of lineDef " << testSeg->Data.lineDef );
						}
						break;
					case SIDE_SPLIT :
						WARNING ( "lineDef " << segs [j].Data.lineDef << " should not be split by lineDef " << testSeg->Data.lineDef );
						break;
					case SIDE_RIGHT :
						if ( side == SIDE_LEFT ) {
							WARNING ( "lineDef " << segs [j].Data.lineDef << " should not to the right of lineDef " << testSeg->Data.lineDef );
						}
						break;
					default :
						break;
				}
			}
		}
#endif

next:
		testSeg++;
	}

	if ( noScores > 1 ) {
		qsort ( score, noScores, sizeof ( sScoreInfo ), sortMetric1 );
		for ( rank = i = 0; i < noScores; i++ ) {
			score [i].total = rank;
			if ( score [i].metric1 != score [i+1].metric1 ) rank++;
		}
		qsort ( score, noScores, sizeof ( sScoreInfo ), sortMetric2 );
		for ( rank = i = 0; i < noScores; i++ ) {
			score [i].total += rank;
			if ( score [i].metric2 != score [i+1].metric2 ) rank++;
		}
		qsort ( score, noScores, sizeof ( sScoreInfo ), sortTotalMetric );
	}

	if ( noScores && score [0].invalid ) {
		int noBad = 0;
		for ( int i = 0; i < noScores; i++ ) if ( score [i].invalid ) noBad++;
		WARNING ( "Non-splittable linedefs have been split! ("<< noBad << "/" << noScores << ")" );
	}

	// SEG *pSeg = noScores ? &segs [ score [0].index ] : NULL;
	// SEG *pSeg = noScores ? &segs [ score [*width].index ] : NULL;

	
	int retval; 

	if (noScores == 0) {
		retval = -2;
	} else if (noScores == 1) {
		retval =  score [0].index;
		noMoreScores = true;
	} else {
		int w = *width;

		for (int s = 0; s != noScores; s++) {
			if (score[s].index > -1) {
				retval = score[s].index;
				w--;
				if (w == 0) {
					break;
				}
			} else {
				
				printf("Dodged a bullet\n");
			}
		}
		// retval = score [0].index;
	}

	if (noScores == *width) {
		noMoreScores = true;
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

//----------------------------------------------------------------------------
//  ALGORITHM 3: 'ZenNode Lite'
//    This is a modified version of the original algorithm used by ZenNode.
//    It uses the same logic for picking the partition, but only looks at the
//    first 30 segs for a valid partition.  If none is found, the search is
//    continued until one is found or all segs have been searched.
//----------------------------------------------------------------------------

//static SEG *AlgorithmQuick ( SEG *segs, int noSegs, sBSPOptions *options, DoomLevel *level, int *width, int *wideSegs )
int AlgorithmQuick ( SEG *segs, int noSegs, sBSPOptions *options, DoomLevel *level, int *width, int *wideSegs ) {
	FUNCTION_ENTRY ( NULL, "AlgorithmQuick", true );

	SEG *pSeg = NULL, *testSeg = segs;
	int count [3];
	int &lCount = count [0], &sCount = count [1], &rCount = count [2];
	// Compute the maximum value maxMetric can possibly reach
	long maxMetric = ( long ) ( noSegs / 2 ) * ( long ) ( noSegs - noSegs / 2 );
	long bestMetric = LONG_MIN, bestSplits = LONG_MAX;

	int i = 0, max = ( noSegs < 30 ) ? noSegs : 30;

	int bestSeg = -2;

retry:

	for ( ; i < max; i++ ) {
		// if ( showProgress && (( i & 15 ) == 0 )) ShowProgress (); obsolete, spammy
		int alias = testSeg->Split ? 0 : lineDefAlias [ testSeg->Data.lineDef ];
		if (( alias == 0 ) || ( lineChecked [ alias ] == false )) {
			lineChecked [ alias ] = -1;
			count [0] = count [1] = count [2] = 0;

			//ComputeStaticVariables ( testSeg );
			ComputeStaticVariables(segs, i);

			if (( fabs ( DX ) < EPSILON ) && ( fabs ( DY ) < EPSILON )) goto next;
			if ( bestMetric < 0 ) for ( int j = 0; j < noSegs; j++ ) {
				count [ WhichSide ( &segs [j], options ) + 1 ]++;
			} else for ( int j = 0; j < noSegs; j++ ) {
				count [ WhichSide ( &segs [j], options ) + 1 ]++;
				if ( sCount > bestSplits ) goto next;
			}
			if ( lCount * rCount + sCount ) {
				long metric = ( long ) lCount * ( long ) rCount;
				if ( sCount ) {
					long temp = X1 * sCount;
					if ( X2 < temp ) metric = X2 * metric / temp;
					metric -= ( X3 * sCount + X4 ) * sCount;
				}
				if ( ANGLE & 0x3FFF ) {
					metric--;
					// printf("red\n");
				}
				/*if ( ANGLE & 0x7FFF ) metric--;
				  if ( ANGLE & 0xAFFF ) metric--;*/

				//if ( metric == maxMetric ) return testSeg;
				if ( metric == maxMetric ) {
					return i;
				}

				if ( metric > bestMetric ) {
					bestSeg = i;
					pSeg = testSeg;
					bestSplits = sCount;
					bestMetric = metric;
				}
			} else if ( alias != 0 ) {
				// Eliminate outer edges of the map from here & down
				*convexPtr++ = alias;
			}
		}
#if defined ( DEBUG )
		else if ( lineChecked [alias] > 0 ) {
			count [0] = count [1] = count [2] = 0;
			ComputeStaticVariables ( testSeg );
			int side = WhichSide ( testSeg, options );
			if (( fabs ( DX ) < EPSILON ) && ( fabs ( DY ) < EPSILON )) continue;
			for ( int j = 0; j < noSegs; j++ ) {
				switch ( WhichSide ( &segs [j], options )) {
					case SIDE_LEFT :
						if ( side == SIDE_RIGHT ) {
							WARNING ( "lineDef " << segs [j].Data.lineDef << " should not to the left of lineDef " << testSeg->Data.lineDef );
						}
						break;
					case SIDE_SPLIT :
						WARNING ( "lineDef " << segs [j].Data.lineDef << " should not be split by lineDef " << testSeg->Data.lineDef );
						break;
					case SIDE_RIGHT :
						if ( side == SIDE_LEFT ) {
							WARNING ( "lineDef " << segs [j].Data.lineDef << " should not to the right of lineDef " << testSeg->Data.lineDef );
						}
						break;
					default :
						break;
				}
			}
		}
#endif

next:
		testSeg++;
	}

	if (( pSeg == NULL ) && ( max < noSegs )) {
		max += 5;
		if ( max > noSegs ) max = noSegs;
		goto retry;
	}

	return bestSeg;
	// return pSeg;
}

int bestSplitReduction = 3;
int maxAdaptiveCutoff = 14;

// static SEG *AlgorithmMulti( SEG *segs, int noSegs, sBSPOptions *options, DoomLevel *level, int *width, int *wideSegs) {
static int AlgorithmMulti( SEG *segs, int noSegs, sBSPOptions *options, DoomLevel *level, int *width, int *wideSegs) {


	int returnSeg;
	// static wLineDefInternal *oldLine;

/*	if (*width == 1) {
		returnSeg = AlgorithmFewerSplits (segs, noSegs, options, level, width, wideSegs );
	}
*/
	returnSeg = AlgorithmBalancedTree (segs, noSegs, options, level, width, wideSegs );

	return returnSeg;
}

static int AlgorithmAdaptive( SEG *segs, int noSegs, sBSPOptions *options, DoomLevel *level, int *width, int *wideSegs) {
	if (noSegs > options->Tuning) {
		return AlgorithmBalancedTree (segs, noSegs, options, level, width, wideSegs );
	} else {
		return AlgorithmFewerSplits (segs, noSegs, options, level, width, wideSegs );
	}
}


//----------------------------------------------------------------------------
//  Check to see if the list of segs contains more than one sector and at least
//    one of them requires "unique subsectors".
//----------------------------------------------------------------------------

static bool KeepUniqueSubsectors ( SEG *segs, int noSegs ) {
	FUNCTION_ENTRY ( NULL, "KeepUniqueSubsectors", true );

	if ( uniqueSubsectors == true ) {
		bool requireUnique = false;
		int lastSector  = segs->Sector;
		for ( int i = 0; i < noSegs; i++ ) {
			if ( keepUnique [ segs [i].Sector ] == true ) requireUnique = true;
			if ( segs [i].Sector != lastSector ) {
				if ( requireUnique == true ) return true;
				lastSector = segs [i].Sector;
			}
		}
	}

	return false;
}

bool overlappingSegs;

static int SortByOrientation ( const void *ptr1, const void *ptr2 ) {
	FUNCTION_ENTRY ( NULL, "SortByOrientation", false );

	SEG *seg1 = ( SEG * ) ptr1;
	SEG *seg2 = ( SEG * ) ptr2;

	// If they're at different angles it's easy...
	int dif1 = seg2->Data.angle - seg1->Data.angle;
	if ( dif1 != 0 ) return dif1;

	// They're colinear, sort them in a clockwise direction
	double dif2 = 0.0;
	double dx = seg1->end.x - seg1->start.x;
	if ( dx > 0.0 ) {
		dif2 = seg1->start.x - seg2->start.x;
	} else if ( dx < 0.0 ) {
		dif2 = seg2->start.x - seg1->start.x;
	} else {
		double dy = seg1->end.y - seg1->start.y;
		if ( dy > 0.0 ) {
			dif2 = seg1->start.y - seg2->start.y;
		} else if ( dy < 0.0 ) {
			dif2 = seg2->start.y - seg1->start.y;
		}
	}

	if ( dif2 != 0.0 ) return ( dif2 < 0.0 ) ? -1 : 1;

	// Mark these SEGs as overlapping
	overlappingSegs = true;

	// OK, we have overlapping lines!
	return ( seg1->Data.lineDef < seg2->Data.lineDef ) ? -1 : 1;
}

#if defined ( DIAGNOSTIC )

static void PrintKeepUniqueSegs ( SEG *segs, int noSegs, const char *msg = NULL ) {
	fprintf ( stdout, "keep-unique SEGS:\n" );

	if ( msg != NULL ) fprintf ( stdout, "  << ERROR - %s >>\n", msg );

	int lastAngle = 0x10000;

	for ( int i = 0; i < noSegs; i++ ) {
		double dx = segs[i].end.x - segs [i].start.x;
		double dy = segs[i].end.y - segs [i].start.y;
		fprintf ( stdout, "  [%d] (%8.1f,%8.1f)-(%8.1f,%8.1f) S:%5d  dx:%8.1f  dy:%8.1f  %04X LD: %5d alias: %5d\n", i, segs [i].start.x, segs [i].start.y, segs [i].end.x, segs [i].end.y, segs[i].Sector, dx, dy, segs [i].Data.angle, segs[i].Data.lineDef, lineDefAlias [ segs[i].Data.lineDef ] );
		if ( segs [i].Data.angle == lastAngle ) {
			double dx = segs[i-1].end.x - segs[i-1].start.x;
			double dy = segs[i-1].end.y - segs[i-1].start.y;
			double y1 = dx * ( segs[i].start.y - segs[i-1].start.y ) - dy * ( segs[i].start.x - segs[i-1].start.x );
			if ( y1 != 0.0 ) fprintf ( stdout, "<< ERROR - seg[%d] is not colinear with seg[%d] - %f!!! >>\n", i, i-1, y1 );
		
			exit(0);
		}
		lastAngle = segs [i].Data.angle;
	}
	fprintf ( stdout, "\n" );
}

#endif

//----------------------------------------------------------------------------
//  Reorder the SEGs so that they are in order around the enclosing subsector
//----------------------------------------------------------------------------
static void ArrangeSegs ( SEG *segs, int noSegs ) {
	FUNCTION_ENTRY ( NULL, "ArrangeSegs", true );

	overlappingSegs = false;

	// Sort the around the center of the polygon
	qsort ( segs, noSegs, sizeof ( SEG ), SortByOrientation );

#if defined ( DIAGNOSTIC )
	bool badSegs = false;

	int lastAngle = 0x10000;
	for ( int i = 0; i < noSegs; i++ ) {
		if ( segs [i].Data.angle == lastAngle ) {
			double dx = segs[i-1].end.x - segs[i-1].start.x;
			double dy = segs[i-1].end.y - segs[i-1].start.y;
			double y1 = dx * ( segs[i].start.y - segs[i-1].start.y ) - dy * ( segs[i].start.x - segs[i-1].start.x );
			if ( y1 != 0.0 ) { badSegs = true; }
		}
		lastAngle = segs [i].Data.angle;
	}
	if (badSegs||overlappingSegs) PrintKeepUniqueSegs (segs, noSegs, overlappingSegs ? "Overlapping SEGs were detected" : NULL );
#endif

	// See if the 1st sector wraps at the end of the list
	int j = noSegs;
	while (( j > 0 ) && ( segs [j-1].Sector == segs [0].Sector )) j--;

	if ( j != noSegs ) {
		memcpy ( tempSeg, segs + j, sizeof ( SEG ) * ( noSegs - j ));
		memmove ( segs + ( noSegs - j ), segs, sizeof ( SEG ) * j );
		memcpy ( segs, tempSeg, sizeof ( SEG ) * ( noSegs - j ));
	}
}

static void MakePartition ( SEG *segs, int noSegs, int *noLeft, int *noRight ) {
	FUNCTION_ENTRY ( NULL, "MakePartition", true );

	// Find the end of the first run of SEGs that are unique (or at don't require
	//   unique subsectors). These will form the right SSECTOR.
	int  lastSector = segs [0].Sector;
	bool uniqueFlag = keepUnique [ lastSector ];

	int right = 0;
	while ( right < noSegs ) {
		int thisSector = segs [right+1].Sector;
		if (( thisSector != lastSector ) && 
				(( uniqueFlag == true ) || ( keepUnique [ thisSector ] == true ))) {
			break;
		}
		lastSector = thisSector;
		right++;
	}

	// Now we need to find a partition line that will isolate the 'right' SEGs

	X  = segs [right].end.x;
	Y  = segs [right].end.y;

	// Look for the easy case first
	if ( segs [0].Data.angle != segs [right].Data.angle ) {

		*noRight = right + 1;
		DX = segs [0].start.x - X;
		DY = segs [0].start.y - Y;

	} else {

		// There are three potential sets of SEGs:
		//  'target' - the SEGs at the start of the list
		//  'head'   - the SEGs immediately following the 'target' SEGs up to the first
		//             SEG in the 'tail' list
		//  'tail'   - SEGs at the end of the list that have at least one endpoint on the same
		//             line as the first 'target' SEG

		// Find the last point not on the target SEGs line
		int split  = noSegs;
		double dx = segs [0].end.x - segs [0].start.x;
		double dy = segs [0].end.y - segs [0].start.y;
		while ( split > right ) {
			double y2 = dx * ( segs [split-1].end.y - segs [0].start.y ) - dy * ( segs [split-1].end.x - segs [0].start.x );
			if ( y2 < 0.0 ) break;
			split--;
			double y1 = dx * ( segs [split].start.y - segs [0].start.y ) - dy * ( segs [split].start.x - segs [0].start.x );
			if ( y1 < 0.0 ) break;
		}

		// See if we found a candidate ending point (the dividing line between 'head' and 'tail' segments
		if ( split > right ) {
			int tail = noSegs - split;
			if ( tail != 0 ) {
				// We're going to split off the 'target'+'head' SEGs (right) from the 'tail' SEGs (left)
				*noRight = split;
				X  = ( segs [split-1].end.x + segs [split].start.x ) / 2.0;
				Y  = ( segs [split-1].end.y + segs [split].start.y ) / 2.0;
				DX = segs [0].start.x - X;
				DY = segs [0].start.y - Y;
			} else {
				// No 'tail' SEGS - split the 'target' (right) and 'head' (left) SEGs
				*noRight = right + 1;
				DX = segs [noSegs-1].end.x - X;
				DY = segs [noSegs-1].end.y - Y;
			}
		} else {
			// All the line segments are colinear
			int split = noSegs;
			double dx = segs [0].end.x - segs [0].start.x;
			if ( dx != 0.0 ) {
				while (( segs [split-1].end.x - segs [0].start.x ) / dx <= 0.0 ) split--;
			} else {
				double dy = segs [0].end.y - segs [0].start.y;
				while (( segs [split-1].end.y - segs [0].start.y ) / dy <= 0.0 ) split--;
			}

			int tail = noSegs - split;
			if ( tail != 0 ) {
				// We're going to split off the 'target'+'head' SEGs (right) from the 'tail' SEGs (left)
				*noRight = split;
				X  = segs [0].start.x;
				Y  = segs [0].start.y;
				DX  = segs [0].start.y - segs [0].end.y;
				DY  = segs [0].end.x - segs [0].start.x;
			} else {
				// No 'tail' SEGS - split the 'target' (right) and 'head' (left) SEGs
				*noRight = right + 1;
				DX  = segs [0].end.y - segs [0].start.y;
				DY  = segs [0].start.x - segs [0].end.x;
			}
		}
	}

	*noLeft  = noSegs - *noRight;
}

#if defined ( DIAGNOSTIC )

static void VerifyNode ( SEG *segs, int noSegs, double x1, double y1, double x2, double y2 ) {
	bool errors = false;
	double dx = x2 - x1;
	double dy = y2 - y1;
	double h = ( dx * dx ) + ( dy * dy );
	for ( int j = 0; j < noSegs; j++ ) {
		double p1 = ( dx * ( segs [j].start.y - y1 ) - dy * ( segs [j].start.x - x1 )) / h;
		double p2 = ( dx * ( segs [j].end.y - y1 ) - dy * ( segs [j].end.x - x1 )) / h;
		if (( p1 > 0.25 ) || ( p2 > 0.25 )) errors = true;
	}
	if ( errors == true || false) {
		if (errors) {
			fprintf ( stdout, "\nPartition line (%8.1f,%8.1f)-(%8.1f,%8.1f) is not correct!\n", x1, y1, x2, y2 );
		} else {
			fprintf ( stdout, "\nPartition line (%8.1f,%8.1f)-(%8.1f,%8.1f) is correct\n", x1, y1, x2, y2 );
		}
		for ( int j = 0; j < noSegs; j++ ) {
			double p1 = ( dx * ( segs [j].start.y - y1 ) - dy * ( segs [j].start.x - x1 )) / h;
			double p2 = ( dx * ( segs [j].end.y - y1 ) - dy * ( segs [j].end.x - x1 )) / h;
			fprintf ( stdout, " [%d] #%4d  (%8.1f,%8.1f) - #%4d (%8.1f,%8.1f)  S:%5d  LD:%5d  y1:%10.3f  y2:%10.3f %s\n", j, segs[j].Data.start,  segs[j].start.x, segs[j].start.y, segs[j].Data.end, segs[j].end.x, segs[j].end.y, segs[j].Sector, segs[j].Data.lineDef, p1, p2, (( p1 > 0.25 ) || ( p2 > 0.25 )) ? "<---" : "" );
			/*
			int datasx = newVertices[segs[j].Data.start].x;
			int datasy = newVertices[segs[j].Data.start].y;
			int dataex = newVertices[segs[j].Data.end].x;
			int dataey = newVertices[segs[j].Data.end].y;

			fprintf ( stdout, " [%d] Data: (%8.1f,%8.1f) - (%8.1f,%8.1f)\n", j,  datasx, datasy, dataex, dataey);
			*/
	
		}
		if (errors == true) {
			exit(0);
		}
	}
}

#endif

//----------------------------------------------------------------------------
//  Recursively create the actual NODEs.  This procedure is very similar to 
//    CreateNode except that we know up front that the SEGs form a convex
//    polygon and only need to be broken up by unique sectors. The main difference
//    here is that the partition line is not taken from the SEGs but is an
//    arbitrary line chosen to break up the SEGs properly to create unique SSECTORs.
//----------------------------------------------------------------------------
static UINT16 GenerateUniqueSectors ( SEG *segs, int noSegs ) {
	FUNCTION_ENTRY ( NULL, "GenerateUniqueSectors", true );

	if ( KeepUniqueSubsectors ( segs, noSegs ) == false ) {
		// if ( showProgress ) ShowDone ();
		return ( UINT16 ) ( 0x8000 | CreateSSector ( segs, noSegs ));
	}

	int noLeft, noRight;

	MakePartition ( segs, noSegs, &noLeft, &noRight );

	wNode tempNode;

	// Store the NODE info set in ComputeStaticVariables
	tempNode.x  = ( INT16 ) lrint ( X );
	tempNode.y  = ( INT16 ) lrint ( Y );
	tempNode.dx = ( INT16 ) lrint ( DX );
	tempNode.dy = ( INT16 ) lrint ( DY );

#if defined ( DIAGNOSTIC )
	double x1 = X;
	double y1 = Y;
	double x2 = X + DX;
	double y2 = Y + DY;
#endif

	// FindBounds ( &tempNode.side [0], segs, noRight );
	// FindBounds ( &tempNode.side [1], segs + noRight, noLeft );

	// if ( showProgress ) GoRight ();

	UINT16 rNode = GenerateUniqueSectors ( segs, noRight );

	// if ( showProgress ) GoLeft ();

	UINT16 lNode = GenerateUniqueSectors ( segs + noRight, noLeft );

#if defined ( DIAGNOSTIC )
	VerifyNode ( segs, noRight, x1, y1, x2, y2 );
	VerifyNode ( segs+noRight, noLeft, x2, y2, x1, y1 );
#endif

	// if ( showProgress ) Backup ();

	if ( nodesLeft-- == 0 ) {
		int delta  = ( 3 * nodeCount ) / 100 + 1;
		// nodePool   = ( wNode * ) realloc ( nodePool, sizeof ( wNode ) * ( nodeCount + delta ));

		wNode *nodePoolRealloc = ( wNode * ) realloc ( nodePool, sizeof ( wNode ) * ( nodeCount + delta ));

		nodePoolEntries = nodeCount + delta;

		if (nodePoolRealloc) {
			nodePool = nodePoolRealloc;
		} else {
			printf("FATAL ERROR: Out of memory in GenerateUniqueSectors()");
		}

		nodesLeft += delta;
	}

	wNode *node = &nodePool [nodeCount];
	*node           = tempNode;
	node->child [0] = rNode;
	node->child [1] = lNode;

	// if ( showProgress ) ShowDone ();

	return ( UINT16 ) nodeCount++;
}

//----------------------------------------------------------------------------
//  Recursively create the actual NODEs.  The given list of SEGs is analyzed
//    and a partition is chosen.  If no partition can be found, a leaf node
//    is created.  Otherwise, the right and left SEGs are analyzed.  Features:
//    - A list of 'convex' aliases is maintained.  These are lines that border
//      the list of SEGs and can never be partitions.  A line is marked as
//      convex for this and all children, and unmarked before returing.
//    - Similarly, the alias chosen as the partition is marked as convex
//      since it will be convex for all children.
//----------------------------------------------------------------------------

#define PROGRESS_DEPTH 8
int depthProgress[PROGRESS_DEPTH] = {0, 0, 0, 0, 0, 0, 0, 0};

long progMax = 0;
int cutoff = 1;

int MaxWidth(int depth, int segs, sBSPOptions *options) {


	if (segs < 10) {
		return 1;
	}

	if (options->Width > 2) {
		if (segs < 30) {
			return 2;
		}
	} 
	if (options->Width > 3) {
		if (segs < 60) {
                        return 3;
                }
	}


	return options->Width;

/*
	int ret = sqrt(segs / 8);

	if (ret < 1) {
		ret = 1;
	}

	return ret;
*/
}

#define COLOR true

void ProgressBar(char *, double, int);

void DepthProgress(int depth, int segs, sBSPOptions *o) {

	segs = 30000;

	// We only update if it's fairly shallow...
	if (depth > PROGRESS_DEPTH) {
		return;
	}

	// Set all depths BELOW to 0 
	for (int i = depth; i != PROGRESS_DEPTH; i++) {
		depthProgress[i] = 0;
	}

	if (depth != -1) {
		// we increment the current depth by one
		depthProgress[depth - 1 ]++;
	}

	double progress = 	
		depthProgress[0] / ((double) MaxWidth(1, segs, o) * 2.0) +
		depthProgress[1] / ((double) MaxWidth(1, segs, o) * (double) MaxWidth(2, segs, o) * 4.0) +
		depthProgress[2] / ((double) MaxWidth(1, segs, o) * (double) MaxWidth(2, segs, o) * (double) MaxWidth(3, segs, o) * 8.0) +
		depthProgress[3] / ((double) MaxWidth(1, segs, o) * (double) MaxWidth(2, segs, o) * (double) MaxWidth(3, segs, o) *(double) MaxWidth(4, segs, o) * 16.0) +
		depthProgress[4] / ((double) MaxWidth(1, segs, o) * (double) MaxWidth(2, segs, o) * (double) MaxWidth(3, segs, o) *(double) MaxWidth(4, segs, o) *(double) MaxWidth(5, segs, o) * 32.0) +	
		depthProgress[5] / ((double) MaxWidth(1, segs, o) * (double) MaxWidth(2, segs, o) * (double) MaxWidth(3, segs, o) *(double) MaxWidth(4, segs, o) *(double) MaxWidth(5, segs, o) * (double) MaxWidth(6, segs, o) * 64.0) + 
		depthProgress[6] / ((double) MaxWidth(1, segs, o) * (double) MaxWidth(2, segs, o) * (double) MaxWidth(3, segs, o) *(double) MaxWidth(4, segs, o) *(double) MaxWidth(5, segs, o) * (double) MaxWidth(6, segs, o) * (double) MaxWidth(7, segs, o) * 128.0) +
		depthProgress[7] / ((double) MaxWidth(1, segs, o) * (double) MaxWidth(2, segs, o) * (double) MaxWidth(3, segs, o) *(double) MaxWidth(4, segs, o) *(double) MaxWidth(5, segs, o) * (double) MaxWidth(6, segs, o) * (double) MaxWidth(7, segs, o) * (double) MaxWidth(7, segs, o)  * 256.0);

	ProgressBar((char *) "BSP       ", progress, 51);

}

int deep[256];
bool deepinit = false;

int CreateNode ( int inSeg, int *noSegs, sBSPOptions *options, DoomLevel *level, int segGoal, int subSectorGoal) {
/*
	if (deepinit == false) {
		for (int i = 0; i != 256; i++) {
			deep[i] = 0;
		}
		deepinit = true;
	} else {
		if (nodeDepth < 256) {
			deep[nodeDepth]++;
			printf("\n");
			for (int i = 0; i != 35; i++) {
				printf("%d,", deep[i]);
			}
		} else {
			printf("very deep!");
		}
		printf("\n");
	}
*/

	bool noMoreScores = false; // stupid hack

	FUNCTION_ENTRY ( NULL, "CreateNode", true );

	int noLeft, noRight;
	int *cptr = convexPtr;


	// speedup!

	double sc = segCount;

	int expander = ((int) (sc * 1.02)) + 4;

        if (expander > maxSegs) {

		expander = ((double) maxSegs * 1.02) + 4;

                SEG *s = new SEG [expander];
                memcpy(s, segStart, maxSegs * sizeof(SEG));

                delete [] segStart;
                segStart = s;

	        maxSegs = expander;
	}

	SEG *segs = &segStart[inSeg];

	int width = 1; // we start at tree 1, not 0
	int maxWidth; //= MaxWidth(nodeDepth, *noSegs, options ); // 1 or higher :)

	// performance
	int initialSegs = *noSegs;	

	if (options->Metric == TREE_METRIC_SEGS) {
		if (segGoal < segCount) {
			maxWidth = 1;
		} else if ((segGoal == segCount) && (subSectorGoal < (ssectorCount + 1))) {
			maxWidth = 1;
		} else {
			maxWidth = MaxWidth(nodeDepth, initialSegs, options );
		}
	} else if (options->Metric == TREE_METRIC_SUBSECTORS) {
		if (subSectorGoal < (ssectorCount + 1)) { // we will always make at least one more sub sector
			maxWidth = 1;
		} else if ((subSectorGoal == (ssectorCount + 1)) && (segGoal < segCount)) {
			maxWidth = 1;
		} else {
			maxWidth = MaxWidth(nodeDepth, initialSegs, options );
		}
	} else {
		maxWidth = MaxWidth(nodeDepth, initialSegs, options );
	}

	int nodesLeftBackup = nodesLeft;
	int segCountBackup = segCount;
	int ssectorCountBackup = ssectorCount;
	int ssectorsLeftBackup = ssectorsLeft;
	int noSegsBackup = *noSegs;
	int noVerticesBackup = noVertices;
	int nodeCountBackup = nodeCount;
	int nodePoolEntriesBackup = nodePoolEntries; // how many did we have at start
	int ssectorPoolEntriesBackup = ssectorPoolEntries;
	int maxVerticesBackup = maxVertices;
	int maxSegsBackup = maxSegs;
	int *convexPtrBackup = convexPtr;
	int *cptrBackup = cptr;

	SEG *CNbestSegs = NULL;

	char *currentSideBackup = currentSide;

	wVertex *newVerticesBackup;
	wSSector *ssectorPoolBackup;
	wNode *nodePoolBackup;

	SEG *segsStartBackup = NULL;
	int *convexListBackup;
	char *lineCheckedBackup;
	char *lineUsedBackup;

	char **sideInfoBackup;

	int noLeftBackup = noLeft;
	int noRightBackup = noRight;
	
	int currentAliasBackup = currentAlias;

	double XBackup = X;
	double YBackup = Y;
	double DXBackup = DX;
	double DYBackup = DY;
	
	long ANGLEBackup = ANGLE;

	// We only backup if we plan to do more than one line pick
	if (maxWidth > 1) {
		// segsStartBackup = new SEG [segCount];
		//memcpy ( segsStartBackup, segStart, sizeof ( SEG) * (segCount) );
		
		segsStartBackup = new SEG [maxSegs];
		// memset ( segsStartBackup, 0, sizeof ( SEG ) * maxSegs );

		//memcpy(segsStartBackup, segStart, sizeof segStart);

		memcpy ( segsStartBackup, segStart, sizeof ( SEG) * maxSegs);

		// memcpy(segsStartBackup, segStart, maxSegs);

		/*
		tempSegBackup = new SEG [tempSegEntries];
		memcpy( tempSegBackup, tempSeg, sizeof(SEG) * (tempSegEntries));
		*/

		convexListBackup = new int[noAliases];
		memcpy (convexListBackup, convexList, sizeof( int) * (noAliases));

		lineCheckedBackup = new char [ noAliases ];
		lineUsedBackup    = new char [ noAliases ];

		memcpy(lineCheckedBackup, lineChecked, sizeof(char) * (noAliases));
		memcpy(lineUsedBackup, lineUsed, sizeof(char) * (noAliases));

		char *temp = new char [sideInfoEntries];
		sideInfoBackup = (char **) temp;
		memcpy(sideInfoBackup, sideInfo, sizeof(char) * (sideInfoEntries));
	
		ANGLE = ANGLEBackup;

		noVerticesBackup = noVertices;

		ssectorPoolBackup = new wSSector [ssectorPoolEntries];
		memcpy ( ssectorPoolBackup, ssectorPool, sizeof ( wSSector) * (ssectorPoolEntries) );
		   
		nodePoolBackup = new wNode [nodePoolEntries];
		memcpy (nodePoolBackup, nodePool, sizeof ( wNode) * (nodePoolEntries));
		
		/*
	 	newVerticesBackup = new wVertex[maxVertices];
		memcpy (newVerticesBackup, newVertices, sizeof (wVertex) * maxVertices);
		*/

	}
	nodeDepth++;

	int CNbestSegsCount = 65536;
	int CNbestTempSegEntries;
	int CNbestSegStartEntries;
	int CNbestSsectorsCount = 65536;
	int CNbestNodesCount;
	int CNbestNodesLeft;
	int CNbestNoVertices;
	int CNbestNoSegs;
	int CNbestSsectorsLeft;
	int CNbestMaxSegs;
	int *CNbestCptr;
	int *CNbestConvexPtr;
	int CNbestCurrentAlias;
	int CNbestNodePoolEntries;
	int CNBestNodeCount;

	int CNbestNoRight, CNbestNoLeft;

	char **CNbestSideInfo;
	char *CNbestCurrentSide;

	UINT16 CNbestRNode;
	UINT16 CNbestLNode;

	wNode CNbestTempNode; 

	double CNbestX;
	double CNbestY;
	double CNbestDX;
	double CNbestDY;

	long CNbestANGLE;

	SEG *bestSegs = NULL;
	SEG *bestTempSeg = NULL;
	wSSector *bestSSectors = NULL;
	wNode *bestNodes = NULL;
	wVertex *bestVertices = NULL;
	int *bestConvexList = NULL;
	char *bestLineUsed = NULL;
	char *bestLineChecked = NULL;
	char **bestSideInfo = NULL;

	int wideSegs[options->Width];

	for (int k = 0; k < options->Width; k++) {
		wideSegs[k] = -1;
	}

differentpartition:

	// segs = &segStart[inSeg];

	if (width != 1) {
	
		subSectorGoal = ssectorCount;
		segGoal = segCount;

		delete [] segStart;
		segStart = new SEG [maxSegs];
		memcpy (segStart, segsStartBackup, sizeof ( SEG) * maxSegsBackup);

		segs = &segStart[inSeg];		

		// restore data from our saved backup

		currentAlias = currentAliasBackup;

		nodeCount = nodeCountBackup;
		*noSegs = noSegsBackup;
		ssectorCount = ssectorCountBackup;
		ssectorsLeft = ssectorsLeftBackup;
		segCount = segCountBackup;
		noVertices = noVerticesBackup;

		nodesLeft =  nodesLeftBackup;
		// nodePoolEntries = nodePoolEntriesBackup;
		// ssectorPoolEntries = ssectorPoolEntriesBackup;

		// maxSegs = maxSegsBackup;

		memcpy (convexList, convexListBackup, sizeof( int) * (convexListEntries));
		memcpy (lineChecked, lineCheckedBackup, sizeof(char) * ( noAliases));
		memcpy (lineUsed, lineUsedBackup, sizeof(char) * ( noAliases));

/*		if (memcmp(newVertices, newVerticesBackup, sizeof (wVertex) * noVertices)) {
			printf("forskjell\n");
		}*/

		// memcpy(newVertices, newVerticesBackup, sizeof (wVertex) * noVertices);

		convexPtr = convexPtrBackup;
		cptr = cptrBackup;

		memcpy(sideInfo, sideInfoBackup, sizeof(char) * (sideInfoEntries));
		currentSide = currentSideBackup;

		noLeft = noLeftBackup;
		noRight = noRightBackup;	

		memcpy ( ssectorPool, ssectorPoolBackup, sizeof ( wSSector) * (ssectorPoolEntriesBackup) );
		memcpy ( nodePool, nodePoolBackup, sizeof ( wNode) * (nodePoolEntriesBackup));
/*
		   if (maxVerticesBackup != maxVertices) {
		   memcpy ( newVertices, newVerticesBackup, sizeof ( wVertex) * (maxVerticesBackup));
		   memcpy ( newVertices, newVerticesBackup, sizeof ( wVertex) * (noVerticesBackup));
		   } 
		   */

		X = XBackup;
		Y = YBackup;
		DX = DXBackup;
		DY = DYBackup;
		ANGLE = ANGLEBackup;
	}

	// printf("Choose p. noSegs=%d, width=%d\n", *noSegs, width);

	if (( *noSegs <= 1 ) || ( ChoosePartition ( segs, *noSegs, &noLeft, &noRight, options, level, &width, wideSegs) == false )) {

		// cleanup!
		if (maxWidth > 1) {
			if (segsStartBackup) {
				delete [] segsStartBackup;
			}
			delete [] convexListBackup;
			delete [] lineUsedBackup;
			delete [] lineCheckedBackup;
			delete [] sideInfoBackup;
			// delete [] newVerticesBackup;
			delete [] nodePoolBackup;
			delete [] ssectorPoolBackup;
		}

		nodeDepth--;

		if (width > 1) {
			if (width < 900) {
				printf("mega bug\n");
			}
		}

		convexPtr = cptr;
		if ( KeepUniqueSubsectors ( segs, *noSegs ) == true ) {
			ArrangeSegs ( segs, *noSegs );
			return ( UINT16 )  GenerateUniqueSectors ( segs, *noSegs );
		}
		// if ( showProgress ) ShowDone ();
		return ( UINT16 ) ( 0x8000 | CreateSSector ( segs, *noSegs ));
	}
	/*
	   if (nodeDepth != 999) {
	   printf("Not subsector, depth: %d, width %d, segs: %d (r: %d, l: %d)\n", nodeDepth, 
	   width, 
	 *noSegs, 
	 noRight, 
	 noLeft);
	 }
	 */



	bool betterTree = false;

	// we set width to -1 IF we have a faulty non-first line pick.

	// scope fix:

	UINT16 rNode, lNode;


	if (width != -1) {

		wNode tempNode;

		// Store the NODE info set in ComputeStaticVariables
		tempNode.x  = ( INT16 ) lrint ( X );
		tempNode.y  = ( INT16 ) lrint ( Y );
		tempNode.dx = ( INT16 ) lrint ( DX );
		tempNode.dy = ( INT16 ) lrint ( DY );


#if defined ( DIAGNOSTIC )
		double x1 = X;
		double y1 = Y;
		double x2 = X + DX;
		double y2 = Y + DY;
#endif

		// printf("\nwidth: %d, depth %d nosegs %d |%d|%d|\n", width, nodeDepth, *noSegs, noRight, noLeft);

		// FindBounds ( &tempNode.side [0], segs, noRight );
		// FindBounds ( &tempNode.side [1], segs + noRight, noLeft );

#if defined ( DIAGNOSTIC )		
		// printf("\nPartition line: width: %d, depth %d, %d,%d to %d,%d. nosegs %d (%d + %d) \n", width, nodeDepth, tempNode.x, tempNode.y, tempNode.x + tempNode.dx, tempNode.y + tempNode.dy, *noSegs, noRight, noLeft);

		/*
		   for (int s = 0 ; s < (noRight + noLeft); s++) {

		   int startX = lrint ( segs [s].start.x );
		   int endX   = lrint ( segs [s].end.x );

		   int startY = lrint ( segs [s].start.y );
		   int endY   = lrint ( segs [s].end.y );

		   printf("%5d seg. %4d,%4d -> %4d,%4d | ", inSeg + s, startX, startY, endX, endY );

		   if (s % 7 == 6) {
		   printf("\n");
		   }
		   }
		   */
#endif

		int alias = currentAlias;

		lineUsed [ alias ] = 1;
		for ( int *tempPtr = cptr; tempPtr != convexPtr; tempPtr++ ) {
			lineUsed [ *tempPtr ] = 2;
		}

		// UINT16 rNode = CreateNode ( inSeg, &noRight, options, level);

		bool swapped = false;	

		rNode = CreateNode ( inSeg, &noRight, options, level, segGoal, subSectorGoal);
		DepthProgress(nodeDepth, initialSegs, options);

		// UINT16 lNode = CreateNode ( inSeg + noRight, &noLeft, options, level);
		lNode = CreateNode ( inSeg + noRight, &noLeft, options, level, segGoal, subSectorGoal);
		DepthProgress(nodeDepth, initialSegs, options);

		// Restore segs!
		segs = &segStart[inSeg];

		if (!((lNode & 0x8000) && ((rNode & 0x8000)))) {
			// printf("\nnone of the sub nodes are subsectors, %d\n", nodeDepth );
			// printf("\nR min x: %d\n", tempNode.side [0].minx);
			// printf("R min x2: %d %d\n" tempNode.

		} else {
			// printf("\nOne or more are sub sectors...\n");
		}

		*noSegs = noLeft + noRight;
		
#if defined ( DIAGNOSTIC )

		//		printf("\nVerifying right #%d, width %d, depth %d\n", inSeg, width, nodeDepth);
		VerifyNode ( segs, noRight, x1, y1, x2, y2 );

		//		printf("\nVerifying left #%d, width %d, depth %d\n", inSeg + noRight, width, nodeDepth);
		VerifyNode ( segs + noRight, noLeft, x2, y2, x1, y1 );

#endif
		while ( convexPtr != cptr ) lineUsed [ *--convexPtr ] = false;
		lineUsed [ alias ] = false;

		// if ( nodesLeft-- == 0 ) {
		if (nodeCount == nodePoolEntries) {
			int delta  = ( 5 * nodeCount ) / 100 + 1;
			// printf("Delta %d\n", delta);

			nodePoolEntries = nodeCount + delta; // zokum 2017

			wNode *nodePoolRealloc = ( wNode * ) realloc ( nodePool, sizeof ( wNode ) * ( nodePoolEntries ));

			if (nodePoolRealloc) {
				nodePool = nodePoolRealloc;
			} else {
				printf("FATAL ERROR: Out of memory in CreateNode()");
			}

			nodesLeft += delta;
		}


		wNode *node = &nodePool [nodeCount];
		*node           = tempNode;
		node->child [0] = rNode;
		node->child [1] = lNode;

		if (options->Metric == TREE_METRIC_SUBSECTORS) {
			if (ssectorCount < CNbestSsectorsCount) {
				betterTree = true;
			} else if (ssectorCount == CNbestSsectorsCount) {
				if ((segCount < CNbestSegsCount)) {
					betterTree = true;
				} else if ((segCount == CNbestSegsCount) && (width == maxWidth)) {
					betterTree = true;
				}
			}
		} else if (options->Metric == TREE_METRIC_SEGS) {
			if (segCount < CNbestSegsCount) {
				betterTree = true;
			} else if (segCount == CNbestSegsCount) {
				if (ssectorCount < CNbestSsectorsCount) {
					betterTree = true;
				} else if ((ssectorCount == CNbestSsectorsCount) && (width == maxWidth)) {
					betterTree = true;
				}
			}
		} 

		// Was it a better set of sub trees, and do we have more to check?
		if (betterTree && (width < maxWidth))  {

			// printf("more trees\n");

			if (bestSegs) {
				delete [] bestSegs;
				delete [] bestSSectors;
				delete [] bestNodes;
				delete [] bestVertices;
				delete [] bestConvexList;
				delete [] bestLineUsed;
				delete [] bestLineChecked;
				delete [] bestTempSeg;
				delete [] bestSideInfo;
			}

			memcpy(&CNbestTempNode, &tempNode, sizeof(wNode));

			bestSegs = new SEG [maxSegs];
			bestSSectors = new wSSector [ssectorPoolEntries];

			//bestNodes = new wNode [nodePoolEntries];
			
			CNbestNodePoolEntries = nodePoolEntries;
			bestNodes = new wNode [nodePoolEntries];

			bestVertices = new wVertex[maxVertices];
			bestConvexList = new int[convexListEntries];
			bestLineUsed = new char[noAliases];
			bestLineChecked = new char[noAliases];

			memcpy (bestSegs, segStart, sizeof ( SEG) * (maxSegs) );
			memcpy (bestSSectors, ssectorPool, sizeof ( wSSector) * (ssectorCount) );
			memcpy (bestVertices, newVertices, sizeof (wVertex) * noVertices);

			memcpy (bestNodes, nodePool, sizeof ( wNode) * nodePoolEntries);
			// memcpy (bestNodes, nodePool, sizeof ( wNode) * nodeCount + 1);

			memcpy (bestConvexList, convexList, sizeof ( int ) * (convexListEntries));
			memcpy (bestLineChecked, lineChecked, sizeof (char) * (noAliases));
			memcpy (bestLineUsed, lineUsed, sizeof (char) * (noAliases));

			char *temp = new char [sideInfoEntries];
			bestSideInfo = (char **) temp;
			memcpy(bestSideInfo, sideInfo, sizeof(char) * (sideInfoEntries));

			CNbestRNode = rNode;
			CNbestLNode = lNode;

			CNbestSegsCount = segCount;
			CNbestSsectorsCount = ssectorCount;
			CNbestSsectorsLeft = ssectorsLeft;
			CNbestNoVertices = noVertices;

			CNbestNodesCount = nodeCount;
			CNbestNodesLeft = nodesLeft;
			CNbestSegs = segs;
			CNbestNoSegs = *noSegs;
			CNbestMaxSegs = maxSegs;
			CNbestCptr = cptr;
			CNbestConvexPtr = convexPtr;

			CNbestNoRight = noRight;
			CNbestNoLeft = noLeft;

			CNbestCurrentAlias = currentAlias;
			CNbestCurrentSide = currentSide;

			CNbestX = X;
			CNbestY = Y;
			CNbestDX = DX;
			CNbestDY = DY;
			CNbestANGLE = ANGLE;
		}

		width++;

		if (		(width <= maxWidth) 
				&& 	(width < *noSegs) 
				&&	(noMoreScores == false)
		   ) {
			goto differentpartition;
		}

	}

	// Cleanup if we looked for several trees
	if (maxWidth > 1) {
		if (segsStartBackup) {
			delete [] segsStartBackup;
		}
		delete [] convexListBackup;
		delete [] lineUsedBackup;
		delete [] lineCheckedBackup;
		delete [] sideInfoBackup;
		// delete [] newVerticesBackup;
		delete [] nodePoolBackup;
		delete [] ssectorPoolBackup;
	}

	// Last tree was not best, restore that one
	//
	
	bool reassigned = false;
	
	if (betterTree == false) {
		maxSegs = CNbestMaxSegs;


		// printf("%d %d\n", nodePoolEntries, CNbestNodePoolEntries);

		memcpy(nodePool,	bestNodes, sizeof ( wNode) * 		(CNbestNodePoolEntries) );
		//memcpy(nodePool,	bestNodes, sizeof ( wNode) *		(CNbestNodesCount + 1));
		memcpy(ssectorPool,  	bestSSectors, sizeof ( wSSector) *    	(CNbestSsectorsCount));
		memcpy(convexList,      bestConvexList, sizeof ( int ) *        (convexListEntries));
		memcpy(lineChecked,     bestLineChecked, sizeof( char ) *       (noAliases));
		memcpy(lineUsed,        bestLineUsed, sizeof( char ) *          (noAliases));
		memcpy(sideInfo,	bestSideInfo, sizeof (char) *           (sideInfoEntries));

		// Uses malloc / free:
//		memcpy(newVertices,  bestVertices, sizeof ( wVertex) *       (CNbestNoVertices) );

		memcpy(newVertices + noVerticesBackup,  bestVertices + noVerticesBackup, sizeof ( wVertex) *  (CNbestNoVertices - noVerticesBackup) );

/*
		if (memcmp(newVertices,  bestVertices, sizeof ( wVertex) *       (CNbestNoVertices) )) {
			printf("error\n");
		}	
*/

// Working
		reassigned = true;

		// delete [] nodePool;
		// nodePool = bestNodes;

		/*delete [] newVertices;
		newVertices = bestVertices;
*/

/*		delete ssectorPool;
		ssectorPool = bestSSectors;
*/
		// memcpy(segStart,      bestSegs, sizeof ( SEG) *               (CNbestMaxegs));
		delete [] segStart;
		segStart = bestSegs;
/*
		delete [] tempSeg;
		tempSeg = bestTempSeg;
*/
//		delete [] ssectorPool;
//		ssectorPool =  bestSSectors;
		
//		delete [] bestConvexList;
//		convexList = bestConvexList;
		
//		delete [] lineChecked;
//		lineChecked = bestLineChecked;
		
//		delete [] lineUsed;
//		lineUsed = bestLineUsed;

//		delete [] sideInfo;
//		sideInfo = bestSideInfo;

		nodeCount = CNbestNodesCount;
		nodesLeft = CNbestNodesLeft;
		ssectorCount = CNbestSsectorsCount;
		ssectorsLeft = CNbestSsectorsLeft;
		*noSegs = CNbestNoSegs;
		segCount = CNbestSegsCount;
		noVertices = CNbestNoVertices;
		cptr = CNbestCptr;
		convexPtr = CNbestConvexPtr;
		noRight = CNbestNoRight;
		noLeft = CNbestNoLeft;
		segs = &segStart[inSeg];
		currentAlias = CNbestCurrentAlias;
		currentSide = CNbestCurrentSide;
		
		X = CNbestX;
		Y = CNbestY;
		DX = CNbestDX;
		DY = CNbestDY;
		ANGLE = CNbestANGLE;
		
		// FindBounds ( &node->side [0], segs, noRight );
		// FindBounds ( &node->side [1], segs + noRight, noLeft );
	}

	// Find the bounds of our current node
	/*
	   wNode *node = &nodePool [nodeCount];

	   node->child [0] = rNode;
	   node->child [1] = lNode;

	   node->x  = ( INT16 ) lrint ( X );
	   node->y  = ( INT16 ) lrint ( Y );
	   node->dx = ( INT16 ) lrint ( DX );
	   node->dy = ( INT16 ) lrint ( DY );


	   FindBounds ( &node->side [0], segs, noRight );
	   FindBounds ( &node->side [1], segs + noRight, noLeft );
	   */

	nodeDepth--;

//	if(segStart != bestSegs) {

	delete [] bestNodes;

	if (reassigned == false) {
		delete [] bestSegs;
		/* delete [] bestTempSeg; */
		// delete [] bestSSectors;
		// delete [] bestVertices;
		// delete [] bestConvexList;
		// delete [] bestLineChecked;
		// delete [] bestLineUsed;
		// delete [] bestSideInfo;
		// delete [] bestNodes;
	}
	//delete [] bestTempSeg;
	delete [] bestSSectors;
	delete [] bestVertices;
	delete [] bestConvexList;
	delete [] bestLineChecked;
	delete [] bestLineUsed;
	delete [] bestSideInfo;

	return ( UINT16 ) nodeCount++;
}

wVertex  *GetVertices () {
	FUNCTION_ENTRY ( NULL, "GetVertices", true );

	wVertex *vert = new wVertex [ noVertices ];
	memcpy ( vert, newVertices, sizeof ( wVertex ) * noVertices );

	return vert;
}

wNode *GetNodes ( wNode *nodeList, int noNodes ) {
	FUNCTION_ENTRY ( NULL, "GetNodes", true );

	wNode *nodes = new wNode [ noNodes ];

	for ( int i = 0; i < noNodes; i++ ) {
		nodes [i] = nodeList [i];
	}

	return nodes;
}

static wSegs *finalSegs;

wSSector *GetSSectors ( wSSector *ssectorList, int noSSectors ) {
	FUNCTION_ENTRY ( NULL, "GetSSectors", true );

	wSegs *segs = finalSegs = new wSegs [ segCount ];

	for ( int i = 0; i < noSSectors; i++ ) {

		int start = ssectorList[i].first;
		ssectorList[i].first = ( UINT16 ) ( segs - finalSegs );

		// Copy the used SEGs to the final SEGs list
		for ( int x = 0; x < ssectorList [i].num; x++ ) {
			segs [x] = segStart [start+x].Data;
		}

		segs += ssectorList [i].num;
	}

	delete [] segStart;
	segCount = segs - finalSegs;

	wSSector *ssector = new wSSector [ noSSectors ];
	memcpy ( ssector, ssectorList, sizeof ( wSSector ) * noSSectors );

	return ssector;
}

wSegs *GetSegs () {
	FUNCTION_ENTRY ( NULL, "GetSegs", true );

	// The list of wSegs is generated in GetSSectors

	return finalSegs;
}


void PostFindBounds( wNode *node) {
	
	wBound bounds[2];

	// If it's a subSector, calculate from segs!
	// If not, recursively figure it out.

	for (int nodeChild = 0; nodeChild != 2; nodeChild++) {
		
		// If a child is a subsector, we can use the segs to get bounds
		
		// printf("Checking side %d\n", nodeChild);
		if (node->child[nodeChild] & 0x8000) {

			int subSector = node->child[nodeChild] - 0x8000;

			// printf(" FindBounds: child: %d, subsector: %d, first: %d, num %d\n", &node->side[nodeChild], subSector, ssectorPool[subSector].first, ssectorPool[subSector].num);

			FindBounds( &node->side[nodeChild], &segStart [ssectorPool[subSector].first], ssectorPool[subSector].num);

			// The child is another node, we need to figure out it's bounds and then use the bounds
		} else { 

			// printf(" PostFind recursive on node %d\n", node->child[nodeChild]);

			PostFindBounds( &nodePool[node->child[nodeChild]]); // recursion

			// Check the children's two sides on this side to get this node's side

			// printf("\nBefore: %d, %d, %d, %d\n", node->side[nodeChild].minx, node->side[nodeChild].miny, node->side[nodeChild].maxx, node->side[nodeChild].maxy );			

			if (nodePool[node->child[nodeChild]].side[0].minx < nodePool[node->child[nodeChild]].side[1].minx) {
				node->side[nodeChild].minx = nodePool[node->child[nodeChild]].side[0].minx;
			} else {
				node->side[nodeChild].minx = nodePool[node->child[nodeChild]].side[1].minx;
			}

			if (nodePool[node->child[nodeChild]].side[0].miny < nodePool[node->child[nodeChild]].side[1].miny) {
				node->side[nodeChild].miny = nodePool[node->child[nodeChild]].side[0].miny;
			} else {
				node->side[nodeChild].miny = nodePool[node->child[nodeChild]].side[1].miny;
			}

			if (nodePool[node->child[nodeChild]].side[0].maxx > nodePool[node->child[nodeChild]].side[1].maxx) {
				node->side[nodeChild].maxx = nodePool[node->child[nodeChild]].side[0].maxx;
			} else {
				node->side[nodeChild].maxx = nodePool[node->child[nodeChild]].side[1].maxx;
			}

			if (nodePool[node->child[nodeChild]].side[0].maxy > nodePool[node->child[nodeChild]].side[1].maxy) {
				node->side[nodeChild].maxy = nodePool[node->child[nodeChild]].side[0].maxy;
			} else {
				node->side[nodeChild].maxy = nodePool[node->child[nodeChild]].side[1].maxy;
			}


			//printf("After:  %d, %d, %d, %d\n", node->side[nodeChild].minx, node->side[nodeChild].miny, node->side[nodeChild].maxx, node->side[nodeChild].maxy );

		}
	}
}


//static void FindBounds ( wBound *bound, SEG *seg, int noSegs ) {



//----------------------------------------------------------------------------
//  Wrapper function that calls all the necessary functions to prepare the
//    BSP tree and insert the new data into the level.  All screen I/O is
//    done in this routine (with the exception of progress indication).
//----------------------------------------------------------------------------
/*
   bool lowestSegCountFound = false;
   */
void CreateNODES ( DoomLevel *level, sBSPOptions *options ) {
	FUNCTION_ENTRY ( NULL, "CreateNODES", true );

	TRACE ( "Processing " << level->Name ());

	// Sanity check on environment variables
	if ( X2 <= 0 ) X2 = 1;
	if ( Y2 <= 0 ) Y2 = 1;

	// Copy options to global variables
	showProgress     = options->showProgress;
	uniqueSubsectors = options->keepUnique ? true : false;

	if ( options->algorithm == 2 ) {
		PartitionFunction = AlgorithmBalancedTree;
	} else if ( options->algorithm == 3 ) {
		PartitionFunction = AlgorithmQuick;
	} else if ( options->algorithm == 4 ) {
		PartitionFunction = AlgorithmAdaptive;
	} else if ( options->algorithm == 5 ) {
		PartitionFunction = AlgorithmMulti;
	} else { // usually == 1
		PartitionFunction = AlgorithmFewerSplits;
	}

	if (PartitionFunction != AlgorithmMulti) {
		options->Width = 1; // never build wide trees unless we have to :)
	}


restart:

	earlyExit = false;

	nodeCount    = 0;
	ssectorCount = 0;

	// Get rid of old SEGS and associated vertices
	level->NewSegs ( 0, NULL );
	level->TrimVertices ();
	level->PackVertices ();

	// Put em here after vertex packing etc

	noVertices  = level->VertexCount ();
	sectorCount = level->SectorCount ();
	usedSector  = new UINT8 [ sectorCount ];
	keepUnique  = new bool [ sectorCount ];
	if ( options->keepUnique ) {
		uniqueSubsectors = true;
		memcpy ( keepUnique, options->keepUnique, sizeof ( bool ) * sectorCount );
	} else {
		memset ( keepUnique, true, sizeof ( bool ) * sectorCount );
	}
	maxVertices = ( int ) ( noVertices * FACTOR_VERTEX );
	newVertices = ( wVertex * ) malloc ( sizeof ( wVertex ) * maxVertices );
	memcpy ( newVertices, level->GetVertices (), sizeof ( wVertex ) * noVertices );

	Status ( (char *) "Creating SEGS ... " );
	segStart = CreateSegs ( level, options );

	Status ( (char *) "Getting LineDef Aliases ... " );
	noAliases = GetLineDefAliases ( level, segStart, segCount );

	lineChecked = new char [ noAliases ];
	lineUsed    = new char [ noAliases ];
	memset ( lineUsed, false, sizeof ( char ) * noAliases );

	Status ( (char *) "Creating Side Info ... " );
	CreateSideInfo ( level );

	// Score is used by balanced and multi
	if ((options->algorithm == 2) || (options->algorithm == 5) || (options->algorithm == 4) ) {
		score = new sScoreInfo [ noAliases ] ;
	} else {
		score = NULL;
	}

	convexList = new int [ noAliases ];
	convexListEntries = noAliases;
	convexPtr  = convexList;

	// reset progressWide %
	progressWide = 0;

	if ( options->algorithm == 1 ) {
		Status ( (char *) "Nodes (split): " );
	} else if ( options->algorithm == 2 ) {
		Status ( (char *) "Nodes (balance): " );
	} else if ( options->algorithm == 3 ) {
		Status ( (char *) "Nodes (quick): " );
	} else if ( options->algorithm == 4 ) {
		Status ( (char *) "Nodes (adaptive): " );
	} else if (options->algorithm == 5 ) {
		Status ( (char *) "Nodes (wide): " );
	}


	nodesLeft   = ( int ) ( FACTOR_NODE * level->SideDefCount ());
	nodePool    = ( wNode * ) malloc( sizeof ( wNode ) * nodesLeft );
	nodePoolEntries = nodesLeft; // zokum 2017


	ssectorsLeft = ( int ) ( FACTOR_NODE * level->SideDefCount ());
	ssectorPool  = ( wSSector * ) malloc ( sizeof ( wSSector ) * ssectorsLeft );
	ssectorPoolEntries = ssectorsLeft;

	int noSegs = segCount;

	DepthProgress(-1, 1, options);	

	CreateNode ( 0, &noSegs, options, level, 99999, 99999 );

	// new 2018 may 29 code
	PostFindBounds(&nodePool[nodeCount - 1]);

	delete [] convexList;
	if ( score ) delete [] score;

	// Clean up temporary buffers
	Status ( (char *) "Cleaning up ... " );
	delete [] sideInfo;
	delete [] lineDefAlias;
	delete [] lineChecked;
	delete [] lineUsed;
	delete [] keepUnique;
	delete [] usedSector;

	sideInfo = NULL;

	level->NewVertices ( noVertices, GetVertices ());
	level->NewNodes ( nodeCount, GetNodes ( nodePool, nodeCount ));
	level->NewSubSectors ( ssectorCount, GetSSectors ( ssectorPool, ssectorCount ));
	level->NewSegs ( segCount, GetSegs ());

	free ( newVertices );
	free ( ssectorPool );
	free ( nodePool );

	delete [] tempSeg;

}
