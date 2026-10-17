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
#include <time.h>
#include <vector>
#include <memory>

#ifdef _WIN32
// Windows-specific includes and defines
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "common.hpp"
#include "logger.hpp"

#include "level.hpp"
#include "bsp/zennode.hpp"
#include "blockmap/blockmap.hpp"
#include "console/console.hpp"
#include "constants.hpp"
#include "bsp/bspGeometry.hpp"
#include "bsp/bspCommon.hpp"
#include "bsp/algorithmBalancedTree.hpp"
#include "bsp/algorithmFewerSplits.hpp"
#include "bsp/algorithmVertexSpace.hpp"
#include "bsp/algorithmQuick.hpp"


DBG_REGISTER ( __FILE__ );

static int       maxSegs;
static int       maxVertices;
static int       nodesLeft;
static NODE    *nodePool;
static wNode	*doomNodes;

static int       nodeCount;                     // Number of NODES stored

static int	nodePoolEntries;

static SEG      *tempSeg;
int 		tempSegEntries;

static SEG      *segStart;
static int       segCount;                      // Number of SEGS stored

static int       ssectorsLeft;
static wSSector *ssectorPool;

static int 	ssectorPoolEntries;


static int 	convexListEntries;

static int       showProgress;
static bool     *keepUnique;
static char     *lineUsed;
static int       noAliases;

// options 
static bool      uniqueSubsectors;

long sideInfoEntries;
int fakeLineDef = 0;
long progressWide = 0;
int nodeDepth = 0;

BSPState bsp{};
BSPState bspBackup{};

extern double oldProgress[2];

static long GetEnvLong(const char* name, long defaultValue) {
	char buffer[64];
	size_t len = 0;

	if (getenv_s(&len, buffer, sizeof(buffer), name) == 0 && len > 0) {
		return atol(buffer);
	}
	return defaultValue;
}

static int (*PartitionFunction) ( SEG *, int, nodeBuilderData *nbd);

int bestSegCount = 32767;
int bestNodeCount = 32767;
int bestSubSectorCount = 32767;

bool earlyExit = false;

//----------------------------------------------------------------------------
//  Create a list of SEGs from the *important* sidedefs.  A sidedef is
//    considered important if:
//     - It has non-zero length
//     - It's linedef has different sectors on each side
//     - It has at least one visible texture
//----------------------------------------------------------------------------
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

		wVertex *vertS = &bsp.newVertices [ lineDef->start ];
		wVertex *vertE = &bsp.newVertices [ lineDef->end ];
		long dx = vertE->x - vertS->x;
		long dy = vertE->y - vertS->y;

		if (( dx == 0 ) && ( dy == 0 )) {
			continue;
		}

		int rSide = lineDef->sideDef [0];
		int lSide = lineDef->sideDef [1];

		if (level->extraData->lineDefsSegProperties[i] == 0x01) {
			lSide = NO_SIDEDEF;
		}

		const wSideDef *sideRight = ( rSide == NO_SIDEDEF ) ? ( wSideDef * ) NULL : &sideDef [ rSide ];
		const wSideDef *sideLeft  = ( lSide == NO_SIDEDEF ) ? ( wSideDef * ) NULL : &sideDef [ lSide ];

		// Ignore line if both sides point to the same sector & neither side has any visible texture
		if ( options->reduceLineDefs && sideRight && sideLeft && ( sideRight->sector == sideLeft->sector )) {

			// This fix allows people to create invisible floors by putting lower textures on those linedefs.

			if ( * ( UINT16 * ) sideLeft->text3 == ( UINT16 ) EMPTY_TEXTURE ) {
				if ( * ( UINT16 * ) sideLeft->text2 == ( UINT16 ) EMPTY_TEXTURE ) {
					sideLeft = ( const wSideDef * ) NULL;
					// printf("cut\n");
				} else {
					// printf("oops\n");
				}
			}
			if ( * ( UINT16 * ) sideRight->text3 == ( UINT16 ) EMPTY_TEXTURE ) {
				if ( * ( UINT16 * ) sideRight->text2 == ( UINT16 ) EMPTY_TEXTURE ) {
					sideRight = ( const wSideDef * ) NULL;
					// printf("cut\n");
				} else {
					// printf("oops2\n");
				}
			}

			if ( ! sideLeft && ! sideRight ) {
				continue;
			}
		}

		if ( options->ignoreLineDef && options->ignoreLineDef [i] ) {
			continue;
		}

		sideLeft = HandleSegRender(sideLeft);
		sideRight = HandleSegRender(sideRight);

		BAM angle = HandleAngle(lineDef, dx, dy);

		if (lineDef->type == 1087) {
			seg->flags |= SEG_DONT_SPLIT;
		}
		else {
			bool split = options->dontSplit ? options->dontSplit[i] : false;
			if (split) {
				seg->flags |= SEG_DONT_SPLIT;
			}
		}

		// we do all the other jazz, except adding the actual segs :)
		if (level->extraData->lineDefsRendered[i] == false) {
			continue;
		}

		if ( sideRight ) {

			seg->Dataangle   = angle;
			seg->DatalineDef = ( UINT16 ) i;
			seg->Sector       = sideRight->sector;
			seg->start.x      = vertS->x;
			seg->start.y      = vertS->y;
			seg->startL      = 0.0;
			seg->end.x        = vertE->x;
			seg->end.y        = vertE->y;
			seg->endL        = 1.0;
			seg++;
		}

		if ( sideLeft ) {
			seg->Dataangle   = ( BAM ) ( angle + BAM180 );
			seg->DatalineDef = ( UINT16 ) i;
			seg->flags |= SEG_BACKSIDE;
			seg->Sector       = sideLeft->sector;
			seg->start.x      = vertE->x;
			seg->start.y      = vertE->y;
			seg->startL      = 0.0;
			seg->end.x        = vertS->x;
			seg->end.y        = vertS->y;
			seg->endL        = 1.0;
			seg++;
		}
	}
	segCount = seg - segStart;

	return segStart;
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

	bsp.sideInfo = ( char ** ) temp;
	memset ( temp, 0, sizeof ( char * ) * noAliases );

	temp += sizeof ( char * ) * noAliases;
	memset ( temp, SIDE_UNKNOWN, level->LineDefCount () * noAliases );

	for ( int i = 0; i < noAliases; i++ ) {
		bsp.sideInfo [i] = ( char * ) temp;
		temp += level->LineDefCount ();
	}
}

//----------------------------------------------------------------------------
//  Return an index for a vertex at (x,y).  If an existing vertex exists,
//    return it, otherwise, create a new one if room is left.
//----------------------------------------------------------------------------
static int AddVertex ( int x, int y ) {
	FUNCTION_ENTRY ( NULL, "AddVertex", true );

	
	for ( int i = 0; i < bsp.noVertices; i++ ) {
		if (( bsp.newVertices [i].x == x ) && ( bsp.newVertices [i].y == y )) return i;
	}
	
	if ( bsp.noVertices == maxVertices ) {
		maxVertices = ( 102 * maxVertices ) / 100 + 1;
		wVertex  *newVerticesReallocated = ( wVertex * ) realloc ( bsp.newVertices, sizeof ( wVertex ) * maxVertices );

		if (newVerticesReallocated) {
			bsp.newVertices = newVerticesReallocated;	
		} else {
			printf("FATAL ERROR: Out of memory in AddVertex()");
		}
	}

	bsp.newVertices [ bsp.noVertices ].x = ( UINT16 ) x;
	bsp.newVertices [ bsp.noVertices ].y = ( UINT16 ) y;
	
	// printf("\nAdded vertex #%3d (%d,%d)\n", bsp.noVertices + 1, x, y);

	return bsp.noVertices++;
}

//----------------------------------------------------------------------------
//  Sort two SEGS so that the one with the lowest numbered LINEDEF is first.
//----------------------------------------------------------------------------
static int SortByLineDef ( const void *ptr1, const void *ptr2 ) {
	FUNCTION_ENTRY ( NULL, "SortByLineDef", true );

	int dif1 = (( SEG * ) ptr1)->Sector - (( SEG * ) ptr2)->Sector;
	if ( dif1 ) return dif1;

	int dif = (( SEG * ) ptr1)->DatalineDef - (( SEG * ) ptr2)->DatalineDef;
	if ( dif ) return dif;


	// What a nice hack for sorting things.
	int a, b;

	if (((SEG *) ptr1)->flags & SEG_BACKSIDE) {
		a = 1;
	} else {
		a = 0;
	}

        if (((SEG *) ptr2)->flags & SEG_BACKSIDE) {
                b = 1;
        } else {
                b = 0;
        }

	return a - b;
}

//----------------------------------------------------------------------------
//  Create a SSECTOR and record the index of the 1st SEG and the total number
//  of SEGs.
//----------------------------------------------------------------------------
static UINT16 CreateSSector ( SEG *segs, int noSegs ) {
	FUNCTION_ENTRY ( NULL, "CreateSSector", true );

	// if ( ssectorsLeft-- == 0 ) {
	if (bsp.subSectorCount == ssectorPoolEntries) {
		int delta     = ( 3 * bsp.subSectorCount ) / 100 + 2;
	
		wSSector *ssectorPoolRealloc = new wSSector [bsp.subSectorCount + delta];
		memcpy (ssectorPoolRealloc, ssectorPool, sizeof ( wSSector) * (bsp.subSectorCount) );

		// printf("ssector realloc\n");

		delete [] ssectorPool;

		if (ssectorPoolRealloc) {
			ssectorPool = ssectorPoolRealloc;
		} else {
			printf("FATAL ERROR: Out of memory in CreateSSector()");
			exit(0);
		}

		ssectorsLeft += delta;
		ssectorPoolEntries = bsp.subSectorCount + delta;
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
		fprintf ( stdout, "SSECTOR [%d]:\n", bsp.ssectorCount );
		double minx = segs[0].start.x;
		double miny = segs[0].start.y;
		for ( int i = 0; i < noSegs; i++ ) {
			if ( segs[i].start.x < minx ) minx = segs[i].start.x;
			if ( segs[i].start.y < miny ) miny = segs[i].start.y;
			if ( segs[i].end.x < minx ) minx = segs[i].end.x;
			if ( segs[i].end.y < miny ) miny = segs[i].end.y;
		}
		for ( int i = 0; i < noSegs; i++ ) {
			fprintf ( stdout, "  [%d] (%8.1f,%8.1f)-(%8.1f,%8.1f)  S:%5d  LD:%5d\n", i, segs[i].start.x - minx, segs[i].start.y - miny, segs[i].end.x - minx, segs[i].end.y - miny, segs[i].Sector, segs[i].DatalineDef );
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
		segs++;
	}
	if ( count == 0 ) {
		WARNING ( "No valid SEGS left in list!" );
	}

	wSSector *ssec = &ssectorPool [bsp.subSectorCount];
	ssec->num   = ( UINT16 ) count;
	ssec->first = ( UINT16 ) first;

	return ( UINT16 ) bsp.subSectorCount++;
}
	
static int SortByAngle ( const void *ptr1, const void *ptr2 ) {
	FUNCTION_ENTRY ( NULL, "SortByAngle", true );

	int dif = ( ANGLE_MASK & (( SEG * ) ptr1)->Dataangle ) - ( ANGLE_MASK & (( SEG * ) ptr2)->Dataangle );
	if ( dif ) return dif;
	dif = (( SEG * ) ptr1)->DatalineDef - (( SEG * ) ptr2)->DatalineDef;
	if ( dif ) return dif;

        int a, b;

        if (((SEG *) ptr1)->flags & SEG_BACKSIDE) {
                a = 1;
        } else {
                a = 0;
        }

        if (((SEG *) ptr2)->flags & SEG_BACKSIDE) {
                b = 1;
        } else {
                b = 0;
        }

        return a - b;
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

	bsp.lineDefAlias = new int [ level->LineDefCount () ];
	memset ( bsp.lineDefAlias, -1, sizeof ( int ) * ( level->LineDefCount ()));

	SEG **segAlias = new SEG * [ level->LineDefCount () + 2 ];

	qsort ( segs, noSegs, sizeof ( SEG ), SortByAngle );

	int lowIndex  = 1;
	int lastAngle = -1;

	for ( int i = 0; i < noSegs; i++ ) {

		// If the LINEDEF has been covered, skip this SEG
		int *alias = &bsp.lineDefAlias [ segs [i].DatalineDef ];

		if ( *alias == -1 ) {

			// ComputeStaticVariables ( &segs [i] );
			// printf("GLA\n");
			ComputeStaticVariables (segs, i);

			// Compare against existing aliases with the same angle
			int x = lowIndex;
			while ( x < noAliases ) {
				if ( CoLinear ( segAlias [x], bsp.X, bsp.Y, bsp.DX, bsp.DY, bsp.ANGLE)) {
					break;
				}
				x++;
			}

			if ( x >= noAliases ) {
				segAlias [ x = noAliases++ ] = &segs [i];
				if ( lastAngle != ( bsp.ANGLE & ANGLE_MASK )) {
					lowIndex = x;
					lastAngle = bsp.ANGLE & ANGLE_MASK;
				}
			}

			*alias = x;
		}
		if ( segs [i].Dataangle == segAlias [*alias]->Dataangle ){
			segs [i].flags &= ~SEG_ALIAS_FLIP;
		} else {
			segs [i].flags |= SEG_ALIAS_FLIP;
		}
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
		int alias = lineDefAlias [ seg->DatalineDef ];
		WARNING (( lineUsed [ alias ] ? "*" : " " ) <<
				" lineDef: " << seg->DatalineDef <<
				" (" << vertS->x << "," << vertS->y << ") -" <<
				" (" << vertE->x << "," << vertE->y << ")" );
		seg++;
	}
}

#endif

//----------------------------------------------------------------------------
//
//  Partition line:
//    DXùx - bsp.DYùy + C = 0               ³ DX  -DY ³ ³-C³
//  rSeg line:                          ³         ³=³  ³
//    dxùx - dyùy + c = 0               ³ dx  -dy ³ ³-c³
//
//----------------------------------------------------------------------------

static void DivideSeg ( SEG *rSeg, SEG *lSeg, DoomLevel *level ) {
	FUNCTION_ENTRY ( NULL, "DivideSeg", true );

	// const wLineDefInternal *lineDef = rSeg->LineDef;
	
	const wLineDefInternal *lineDef = &level->GetLineDefs ()[rSeg->DatalineDef] ;
	
	wVertex *vertS = &bsp.newVertices [ lineDef->start ];
	wVertex *vertE = &bsp.newVertices [ lineDef->end ];

	// Minimum precision required to avoid overflow/underflow:
	//   dx, dy  - 16 bits required
	//   c       - 33 bits required
	//   det     - 32 bits required
	//   x, y    - 50 bits required

	double dx  = vertE->x - vertS->x;
	double dy  = vertE->y - vertS->y;
	double num = bsp.DX * ( vertS->y - bsp.Y ) - bsp.DY * ( vertS->x - bsp.X );
	double det = dx * bsp.DY - dy * bsp.DX;

	if ( det == 0.0 ) {
		ZOKUMBSP_ERROR ( "SEG is parallel to partition line" );
	}

	double l = num / det;
	double x = vertS->x + num * dx / det;
	double y = vertS->y + num * dy / det;

	// if ( rSeg->final == true ) {
	if (rSeg->flags & SEG_FINAL) {
		x = lrint ( x );
		y = lrint ( y );
#if defined ( DEBUG )
		if ((( rSeg->start.x == x ) && ( rSeg->start.y == y )) ||
				(( lSeg->start.x == x ) && ( lSeg->start.y == y ))) {
			fprintf ( stderr, "\nNODES: End point duplicated in DivideSeg: LineDef #%d", rSeg->DatalineDef );
			fprintf ( stderr, "\n       Partition: from (%f,%f) to (%f,%f)", bsp.X, bsp.Y, bsp.X + bsp.DX, bsp.Y + bsp.DY );
			fprintf ( stderr, "\n       LineDef: from (%d,%d) to (%d,%d) split at (%f,%f)", vertS->x, vertS->y, vertE->x, vertE->y, x, y );
			fprintf ( stderr, "\n       SEG: from (%f,%f) to (%f,%f)", rSeg->start.x, rSeg->start.y, rSeg->end.x, rSeg->end.y );
			fprintf ( stderr, "\n       dif: (%f,%f)  (%f,%f)", rSeg->start.x - x, rSeg->start.y - y, rSeg->end.x - x, rSeg->end.y - y );
		}
#endif
	}

	// Determine which sided of the partition line the start point is on
	double sideS = bsp.DX * ( rSeg->start.y - bsp.Y ) - bsp.DY * ( rSeg->start.x - bsp.X );

	// Get the correct endpoint of the base LINEDEF for the offset calculation
/*
	if ( rSeg->Dataflip ) vertS = vertE;
	if ( rSeg->Dataflip ) l = 1.0 - l;
*/

	if (rSeg->flags & SEG_BACKSIDE) {
		// vertS = vertE;
		l = 1.0 - l;
	}

	rSeg->flags |= SEG_SPLIT;
	lSeg->flags |= SEG_SPLIT;

	// Fill in the parts of lSeg & rSeg that have changed
	if ( sideS < 0.0 ) {
#if defined ( DEBUG )
		if (( lrint ( x ) == lrint ( lSeg->start.x )) && ( lrint ( y ) == lrint ( lSeg->start.y ))) {
			fprintf ( stderr, "DivideSeg: split didn't work (%10.3f,%10.3f) == L(%10.3f,%10.3f) - %d\n", x, y, lSeg->start.x, lSeg->start.y, bsp.currentAlias );
		} else if (( l < lSeg->startL ) || ( l > lSeg->endL )) {
			fprintf ( stderr, "DivideSeg: warning - split is outside line segment (%7.5f-%7.5f) %7.5f\n", lSeg->startL, lSeg->endL, l );
		}
#endif
		rSeg->end.x       = x;
		rSeg->end.y       = y;
		rSeg->endL       = l;
		lSeg->start.x     = x;
		lSeg->start.y     = y;
		lSeg->startL     = l;
		
		lSeg->flags |= SEG_SPLIT_START;
		rSeg->flags |= SEG_SPLIT_END;


		// xmas 2018 fix
		rSeg->Dataangle = ComputeAngle(rSeg->end.x - rSeg->start.x, rSeg->end.y - rSeg->start.y);
		lSeg->Dataangle = ComputeAngle(lSeg->end.x - lSeg->start.x, lSeg->end.y - lSeg->start.y);

	} else {
#if defined ( DEBUG )
		if (( lrint ( x ) == lrint ( rSeg->start.x )) && ( lrint ( y ) == lrint ( rSeg->start.y ))) {
			fprintf ( stderr, "DivideSeg: split didn't work (%10.3f,%10.3f) == R(%10.3f,%10.3f) - %d\n", x, y, rSeg->start.x, rSeg->start.y, bsp.currentAlias  );
		} else if (( l < lSeg->startL ) || ( l > lSeg->endL )) {
			fprintf ( stderr, "DivideSeg: warning - split is outside line segment (%7.5f-%7.5f) %7.5f\n", lSeg->startL, lSeg->endL, l );
		}
#endif
		lSeg->end.x       = x;
		lSeg->end.y       = y;
		lSeg->endL       = l;
		rSeg->start.x     = x;
		rSeg->start.y     = y;
		rSeg->startL     = l;

		rSeg->flags |= SEG_SPLIT_START;
		lSeg->flags |= SEG_SPLIT_END;

		rSeg->Dataangle = ComputeAngle(rSeg->end.x - rSeg->start.x, rSeg->end.y - rSeg->start.y);
		lSeg->Dataangle = ComputeAngle(lSeg->end.x - lSeg->start.x, lSeg->end.y - lSeg->start.y);
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
static void SplitSegs ( SEG *segs, int noSplits, DoomLevel *level ) {
	
	FUNCTION_ENTRY ( NULL, "SplitSegs", true );

	segCount += noSplits;
	
	if ( segCount > maxSegs ) {
		fprintf ( stderr, "\nError: Too many SEGs have been split: %d > %d \n", segCount, maxSegs );
		exit ( -1 );
	}
	
	int count = segCount - ( segs - segStart ) - noSplits;
	memmove ( segs + noSplits, segs, count * sizeof ( SEG ));

	for ( int i = 0; i < noSplits; i++ ) {
		DivideSeg ( segs, segs + noSplits, level );
		segs++;
	}
}

static void SortSegs ( int inSeg, SEG *seg, int noSegs, int *noLeft, int *noRight, sBSPOptions *options, DoomLevel *level, nodeBuilderData *nbd) {
	FUNCTION_ENTRY ( NULL, "SortSegs", true );

	if (inSeg == -1) {
		return;
	}

	SEG *pSeg;

	if (options->vertexPartition ) {
		// printf("vertex\n");
		pSeg = &nbd->pSeg;

		// printf("P-line       : %5.0f %5.0f %5.0f %5.0f\n", pSeg->start.x, pSeg->start.y, pSeg->end.x, pSeg->end.y); 
		ComputeStaticVariables(&nbd->pSeg, 0);

	} else {
		pSeg = &seg[inSeg];
		ComputeStaticVariables(seg, inSeg);
	}

	int count [3];

	if ( pSeg == NULL ) {
#if defined ( DEBUG )
		for ( int x = 0; x < noSegs; x++ ) {
			// Make sure that all SEGs are actually on the right side of each other
			// ComputeStaticVariables ( &seg [x] );

			ComputeStaticVariables(seg, x);

			if (( fabs ( bsp.DX ) < EPSILON ) && ( fabs ( bsp.DY ) < EPSILON )) continue;

			count [0] = count [1] = count [2] = 0;
			for ( int i = 0; i < noSegs; i++ ) {
				count [( seg [i].Side = _WhichSide ( &seg [i], options, level )) + 1 ]++;
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

	// printf("Pre-S1 - - - (%d) %f %f -> %f %f\n", inSeg, bsp.X, bsp.Y, bsp.DX, bsp.DY);

	count [0] = count [1] = count [2] = 0;
	int i;
	int side;

	// printf("Pre-S2 %d %d %d (%d) %f %f -> %f %f\n", count [0],  count [1],  count [2], inSeg, bsp.X, bsp.Y, bsp.DX, bsp.DY);

	for ( i = 0; i < noSegs; i++ ) {
		if (nbd->options->vertexPartition ) {
			side = _WhichSide ( &seg [i], options, level );
		} else {
			side = WhichSide ( &seg [i], options, level );
		}
		count [side + 1 ]++;

		// seg [i].Side = side;

		if (side == 1) {
			seg [i].flags |= SEG_RIGHT;
			seg [i].flags &= ~SEG_LEFT;
		} else if (side) {
			seg [i].flags |= SEG_LEFT;
			seg [i].flags &= ~SEG_RIGHT;
		} else {
			seg [i].flags |= SEG_RIGHT;
			seg [i].flags |= SEG_LEFT;
		}
		// printf("seg %f %f -> %f %f | side %d\n", seg[i].start.x, seg[i].start.y, seg[i].end.x, seg[i].end.y, side);
	}

	// printf("Best line was: %5.0f %5.0f -> %5.0f %5.0f\n", nbd->pSeg.start.x, nbd->pSeg.start.y, nbd->pSeg.end.x, nbd->pSeg.end.y);

	// printf("Sorted %d %d %d (%d) %f %f -> %f %f\n", count [0],  count [1],  count [2], inSeg, bsp.X, bsp.Y, bsp.DX, bsp.DY);

	ASSERT (( count [0] * count [2] != 0 ) || ( count [1] != 0 ));

	*noLeft  = count [0];
	*noRight = count [2];

	SEG *rSeg = seg;
	// for ( i = 0; seg [i].Side == SIDE_RIGHT; i++ ) rSeg++;
	
	for ( i = 0; (seg [i].flags & SEG_RIGHT) && !(seg [i].flags & SEG_LEFT) ; i++) rSeg++;

	if (( i < count [2] ) || count [1] ) {
		SEG *sSeg = tempSeg;
		SEG *lSeg = sSeg + count [1];
		for ( ; i < noSegs; i++ ) {
			if ((seg [i].flags & SEG_RIGHT) && !(seg [i].flags & SEG_LEFT)) {
				side = 1;
			} else if ((seg [i].flags & SEG_RIGHT) && (seg [i].flags & SEG_LEFT)) {
				side = 0;
			} else {
				side = -1;
			}
		
			// switch ( seg [i].Side ) {
			switch (side) {
				case SIDE_LEFT  : *lSeg++ = seg [i];		break;
				case SIDE_SPLIT : *sSeg++ = seg [i];		break;
				case SIDE_RIGHT : *rSeg++ = seg [i];		break;
			}
		}
		memcpy ( rSeg, tempSeg, ( noSegs - count [2] ) * sizeof ( SEG ));
	}

	if ( count [1] != 0 ) {
		SplitSegs ( &seg [ *noRight ], count [1], level );
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
static bool ChoosePartition ( SEG *seg, int noSegs, int *noLeft, int *noRight, nodeBuilderData *nbd) {
	FUNCTION_ENTRY ( NULL, "ChoosePartition", true );

	bool check = true;

retry:

	//if ( seg->final == false ) {
	if (! (seg->flags & SEG_FINAL)) {
		memcpy ( bsp.lineChecked, lineUsed, sizeof ( char ) * noAliases );
	} else {
		memset ( bsp.lineChecked, 0, sizeof ( char ) * noAliases );
	}
	// Find the best SEG to be used as a partition
	int partitionOffset;
	int w = nbd->width - 1;

	if (w != 0 && (nbd->wideSegs[w] != -1)) {
		// printf("save\n");
		partitionOffset = nbd->wideSegs[w];

	} else { 
		partitionOffset = PartitionFunction ( seg, noSegs, nbd);
	}


	SEG *pSeg = &nbd->pSeg;

//	printf("P-line CP    : %5.0f %5.0f %5.0f %5.0f\n", pSeg->start.x, pSeg->start.y, pSeg->end.x, pSeg->end.y);


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
		
		// if (nbd->options->algorithm == 6 ) {
		if (nbd->options->vertexPartition ) {
			SortSegs ( -2, seg, noSegs, noLeft, noRight, nbd->options, nbd->level, nbd);
		} else {
			SortSegs ( partitionOffset, seg, noSegs, noLeft, noRight, nbd->options, nbd->level, nbd);
		}
	}

	// Make sure the set of SEGs is still convex after we convert to integer coordinates
	//if (( pSeg == NULL ) && ( check == true )) {
	
	if (( partitionOffset == -2 ) && ( check == true )) {
		check = false;
		double error = 0.0;
		
		// printf("\nlrint\n");

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
			//seg [i].final   = true;
			seg[i].flags |= SEG_FINAL;
		}
		if ( (error > EPSILON) && (nbd->options->SplitHandling != 0)) {
			// printf("error!\n");
			// Force a check of each line
			goto retry;
		}
	} else {
		// printf("poff %d\n", partitionOffset);
	}
	if (partitionOffset == -2) {
		return false;
	} else {
		return true;
	}
	// return partitionOffset;
}

int bestSplitReduction = 3;
int maxAdaptiveCutoff = 14;

static int AlgorithmMulti( SEG *segs, int noSegs, nodeBuilderData *nbd) {
	int returnSeg;
	// static wLineDefInternal *oldLine;
	returnSeg = AlgorithmBalancedTree (segs, noSegs, nbd);
	return returnSeg;
}

static int AlgorithmAdaptive( SEG *segs, int noSegs, nodeBuilderData *nbd) {
	if (noSegs > nbd->options->Tuning) {
		//return AlgorithmBalancedTree (segs, noSegs, options, level, width, wideSegs, picks );
		nbd->options->vertexPartition = false;
		return AlgorithmBalancedTree (segs, noSegs, nbd);
	} else {
		// return AlgorithmFewerSplits (segs, noSegs, options, level, width, wideSegs, picks );
		// return AlgorithmFewerSplits (segs, noSegs, nbd);
		nbd->options->vertexPartition = true;
		return  AlgorithmVertex(segs, noSegs, nbd);
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
	int dif1 = seg2->Dataangle - seg1->Dataangle;
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
	return ( seg1->DatalineDef < seg2->DatalineDef ) ? -1 : 1;
}

#if defined ( DIAGNOSTIC )

static void PrintKeepUniqueSegs ( SEG *segs, int noSegs, const char *msg = NULL ) {
	fprintf ( stdout, "keep-unique SEGS:\n" );

	if ( msg != NULL ) fprintf ( stdout, "  << ERROR - %s >>\n", msg );

	int lastAngle = 0x10000;

	for ( int i = 0; i < noSegs; i++ ) {
		double dx = segs[i].end.x - segs [i].start.x;
		double dy = segs[i].end.y - segs [i].start.y;
		fprintf ( stdout, "  [%d] (%8.1f,%8.1f)-(%8.1f,%8.1f) S:%5d  dx:%8.1f  dy:%8.1f  %04X LD: %5d alias: %5d\n", i, segs [i].start.x, segs [i].start.y, segs [i].end.x, segs [i].end.y, segs[i].Sector, dx, dy, segs [i].Dataangle, segs[i].DatalineDef, lineDefAlias [ segs[i].DatalineDef ] );
		if ( segs [i].Dataangle == lastAngle ) {
			double dx = segs[i-1].end.x - segs[i-1].start.x;
			double dy = segs[i-1].end.y - segs[i-1].start.y;
			double y1 = dx * ( segs[i].start.y - segs[i-1].start.y ) - dy * ( segs[i].start.x - segs[i-1].start.x );
			if ( y1 != 0.0 ) fprintf ( stdout, "<< ERROR - seg[%d] is not colinear with seg[%d] - %f!!! >>\n", i, i-1, y1 );

			exit(0);
		}
		lastAngle = segs [i].Dataangle;
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
		if ( segs [i].Dataangle == lastAngle ) {
			double dx = segs[i-1].end.x - segs[i-1].start.x;
			double dy = segs[i-1].end.y - segs[i-1].start.y;
			double y1 = dx * ( segs[i].start.y - segs[i-1].start.y ) - dy * ( segs[i].start.x - segs[i-1].start.x );
			if ( y1 != 0.0 ) { badSegs = true; }
		}
		lastAngle = segs [i].Dataangle;
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

	bsp.X  = segs [right].end.x;
	bsp.Y  = segs [right].end.y;

	// Look for the easy case first
	if ( segs [0].Dataangle != segs [right].Dataangle ) {

		*noRight = right + 1;
		bsp.DX = segs [0].start.x - bsp.X;
		bsp.DY = segs [0].start.y - bsp.Y;

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
				bsp.X  = ( segs [split-1].end.x + segs [split].start.x ) / 2.0;
				bsp.Y  = ( segs [split-1].end.y + segs [split].start.y ) / 2.0;
				bsp.DX = segs [0].start.x - bsp.X;
				bsp.DY = segs [0].start.y - bsp.Y;
			} else {
				// No 'tail' SEGS - split the 'target' (right) and 'head' (left) SEGs
				*noRight = right + 1;
				bsp.DX = segs [noSegs-1].end.x - bsp.X;
				bsp.DY = segs [noSegs-1].end.y - bsp.Y;
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
				bsp.X  = segs [0].start.x;
				bsp.Y  = segs [0].start.y;
				bsp.DX  = segs [0].start.y - segs [0].end.y;
				bsp.DY  = segs [0].end.x - segs [0].start.x;
			} else {
				// No 'tail' SEGS - split the 'target' (right) and 'head' (left) SEGs
				*noRight = right + 1;
				bsp.DX  = segs [0].end.y - segs [0].start.y;
				bsp.DY  = segs [0].start.x - segs [0].end.x;
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
			fprintf ( stdout, " [%d] #%4d  (%8.1f,%8.1f) - #%4d (%8.1f,%8.1f)  S:%5d  LD:%5d  y1:%10.3f  y2:%10.3f %s\n", j, segs[j].Datastart,  segs[j].start.x, segs[j].start.y, segs[j].Dataend, segs[j].end.x, segs[j].end.y, segs[j].Sector, segs[j].DatalineDef, p1, p2, (( p1 > 0.25 ) || ( p2 > 0.25 )) ? "<---" : "" );
			/*
			   int datasx = bsp.newVertices[segs[j].Data.start].x;
			   int datasy = bsp.newVertices[segs[j].Data.start].y;
			   int dataex = bsp.newVertices[segs[j].Data.end].x;
			   int dataey = bsp.newVertices[segs[j].Data.end].y;

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

	//NODE tempNode;
	NODE tempNode;

	// Store the NODE info set in ComputeStaticVariables
	tempNode.x  = ( INT16 ) lrint ( bsp.X );
	tempNode.y  = ( INT16 ) lrint ( bsp.Y );
	tempNode.dx = ( INT16 ) lrint ( bsp.DX );
	tempNode.dy = ( INT16 ) lrint ( bsp.DY );

#if defined ( DIAGNOSTIC )
	double x1 = bsp.X;
	double y1 = bsp.Y;
	double x2 = bsp.X + bsp.DX;
	double y2 = bsp.Y + bsp.DY;
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

	// if ( nodesLeft-- == 0 ) {
	if (nodeCount == nodePoolEntries) {
		// int delta  = ( 3 * nodeCount ) / 100 + 1;

		int delta  = ( 5 * nodeCount ) / 100 + 1;

		// nodePool   = ( NODE * ) realloc ( nodePool, sizeof ( NODE ) * ( nodeCount + delta ));

		// NODE *nodePoolRealloc = ( NODE * ) realloc ( nodePool, sizeof ( NODE ) * ( nodeCount + delta ));
		// NODE *nodePoolRealloc = ( NODE * ) realloc ( nodePool, sizeof ( NODE ) * ( nodeCount + delta ));
		NODE *nodePoolRealloc = new NODE [nodeCount + delta];
		memcpy (nodePoolRealloc, nodePool, sizeof ( NODE) * (nodeCount) );


		nodePoolEntries = nodeCount + delta;

		if (nodePoolRealloc) {
			nodePool = nodePoolRealloc;
		} else {
			printf("FATAL ERROR: Out of memory in GenerateUniqueSectors()");
		}

		nodesLeft += delta;
	}

	NODE *node = &nodePool [nodeCount];
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

#define PROGRESS_DEPTH 10
int depthProgress[PROGRESS_DEPTH] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

long progMax = 0;
int cutoff = 1;

int MaxWidth(int depth, int segs, sBSPOptions *options) {


	if (segs < 10) {
		return 1;
	}

	if (options->Width > 2) {
		if (segs < 20) {
			return 2;
		}
	} 
	/*	if (options->Width > 3) {
		if (segs < 60) {
		return 3;
		}
		}
		*/

	//	printf("%d\n", options->Width);

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

// void ProgressBar(char *, double, int, int);

void DepthProgress(int depth, int segs, sBSPOptions *o) {


	/* 2018 more sensible code */

	double progress;

	// Subsector based progress, smoother, but unpredictable for multi tree
	if (o->Width == 1) {

		double max = segCount;
		double prog;

		if (bsp.subSectorCount) {
			prog = ssectorPool [bsp.subSectorCount - 1].first + ssectorPool [bsp.subSectorCount -1].num;
		} else {
			prog = 0;
		}

		// printf("%f %f\n", prog, max);

		progress = prog / max;

		// Tree based progress, the best solution for multi tree, but looks odd if tree is badly balanced.
	} else {

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

		progress = 	
			depthProgress[0] / ((double) MaxWidth(1, segs, o) * 2.0) +
			depthProgress[1] / ((double) MaxWidth(1, segs, o) * (double) MaxWidth(2, segs, o) * 4.0) +
			depthProgress[2] / ((double) MaxWidth(1, segs, o) * (double) MaxWidth(2, segs, o) * (double) MaxWidth(3, segs, o) * 8.0) +
			depthProgress[3] / ((double) MaxWidth(1, segs, o) * (double) MaxWidth(2, segs, o) * (double) MaxWidth(3, segs, o) *(double) MaxWidth(4, segs, o) * 16.0) +
			depthProgress[4] / ((double) MaxWidth(1, segs, o) * (double) MaxWidth(2, segs, o) * (double) MaxWidth(3, segs, o) *(double) MaxWidth(4, segs, o) *(double) MaxWidth(5, segs, o) * 32.0) +	
			depthProgress[5] / ((double) MaxWidth(1, segs, o) * (double) MaxWidth(2, segs, o) * (double) MaxWidth(3, segs, o) *(double) MaxWidth(4, segs, o) *(double) MaxWidth(5, segs, o) * (double) MaxWidth(6, segs, o) * 64.0) + 
			depthProgress[6] / ((double) MaxWidth(1, segs, o) * (double) MaxWidth(2, segs, o) * (double) MaxWidth(3, segs, o) *(double) MaxWidth(4, segs, o) *(double) MaxWidth(5, segs, o) * (double) MaxWidth(6, segs, o) * (double) MaxWidth(7, segs, o) * 128.0) +
			depthProgress[7] / ((double) MaxWidth(1, segs, o) * (double) MaxWidth(2, segs, o) * (double) MaxWidth(3, segs, o) *(double) MaxWidth(4, segs, o) *(double) MaxWidth(5, segs, o) * (double) MaxWidth(6, segs, o) * (double) MaxWidth(7, segs, o) * (double) MaxWidth(7, segs, o)  * 256.0) +
			depthProgress[8] / ((double) MaxWidth(1, segs, o) * (double) MaxWidth(2, segs, o) * (double) MaxWidth(3, segs, o) *(double) MaxWidth(4, segs, o) *(double) MaxWidth(5, segs, o) * (double) MaxWidth(6, segs, o) * (double) MaxWidth(7, segs, o) * (double) MaxWidth(7, segs, o) * (double) MaxWidth(8, segs, o) * 512.0);


		;

	}
	ProgressBar((char *) "BSP       ", progress, 50, 0);
}

int nodeCounter = 0;

long long wideStats[6];

int deep[256];
bool deepinit = false;

int maxDepth = 0;

time_t updated = 0;


bool segInited = false;

#define SEG_POOL_SIZE 200
pool segPool[SEG_POOL_SIZE];

SEG *AllocateSeg(int minimum) {
	if (!segInited) {
		for (int i = 0; i != SEG_POOL_SIZE; i++) {
			segPool[i].address = NULL;
			segPool[i].size = 0;
			segPool[i].used = false;
		}
		segInited = true;
	}

	// try to find an unused allocated memory location
	int bestFit = -1;	

	for (int i = 0; i != SEG_POOL_SIZE; i++) {
		if (segPool[i].address && (segPool[i].used == false)) { 
			if ((segPool[i].size > minimum)) {
				if (bestFit != -1) {
					// find the smallest pool that is big enough
					if (segPool[bestFit].size > segPool[i].size) {
						bestFit = i;
					}
				} else {
					bestFit = i;
				}	
			}
		} else if (segPool[i].address == NULL) {
			break;
		}		
	}

	if (bestFit != -1) {
		// printf("reused\n");
		segPool[bestFit].used = true;
		return (SEG *) segPool[bestFit].address;
	}

	// no entry big enough, let's try to allocate one

	for (int i = 0; i != SEG_POOL_SIZE; i++) {
		if (segPool[i].used == false) {
			size_t newSize = (minimum * 110) / 100;

			segPool[i].address = new SEG [newSize];
			segPool[i].used = true;
			segPool[i].size = newSize;
			return (SEG *) segPool[i].address;
		}
	}

	// last resort, since we've used all 200 slots.

	return new SEG [minimum];

}


void FreeSeg(SEG *address) {
	for (int i = 0; i != 200; i++) {
		if (address == segPool[i].address) {
			segPool[i].used = false;
			return;
		}
	}
	// last resort
	delete [] address;
}


int goodScoresUsed = 0;

// int CreateNode ( int inSeg, int *noSegs, sBSPOptions *options, DoomLevel *level, int segGoal, int subSectorGoal, int *maxPicks) {
int CreateNode ( int inSeg, int *noSegs, nodeBuilderData *nbd) {

	sBSPOptions *options = nbd->options;
	DoomLevel *level = nbd->level;


	// nodeBuilderData *lnbd = new nodeBuilderData;
	// memcpy(lnbd, nbd, sizeof(nodeBuilderData));
	// auto lnbd = std::make_unique<nodeBuilderData>(*nbd);
	nodeBuilderData* lnbd = new nodeBuilderData(*nbd); // calls copy constructor


	lnbd->localMaxWidth = nbd->options->Width;

	bool noMoreScores = false; // stupid hack

	FUNCTION_ENTRY ( NULL, "CreateNode", true );

	int noLeft, noRight;
	int *cptr = bsp.convexPtr;

	// int widthOffset = 0;

	double sc = segCount;

	int expander = ((int) (sc * 1.02)) + 3;

	if (expander > maxSegs) {

		expander = ((double) maxSegs * 1.02) + 3;

		// SEG *s = new SEG [expander];
		SEG *s = AllocateSeg(expander);

		memcpy(s, segStart, maxSegs * sizeof(SEG));

		//delete [] segStart;
		FreeSeg(segStart);
		segStart = s;

		maxSegs = expander;
	}

	SEG *segs = &segStart[inSeg];

	lnbd->width = 1; // we start at tree width 1, not 0
	int maxWidth = 1; //= MaxWidth(nodeDepth, *noSegs, options ); // 1 or higher :)

	// performance
	int initialSegs = *noSegs;	

	if (options->Metric == TREE_METRIC_SEGS) {
		if (lnbd->segGoal < segCount) {
			maxWidth = 1;
		} else if ((lnbd->segGoal == segCount) && (lnbd->subSectorGoal < (bsp.subSectorCount + 1))) {
			maxWidth = 1;
		} else {
			maxWidth = MaxWidth(nodeDepth, initialSegs, options );
		}
	} else if (options->Metric == TREE_METRIC_SUBSECTORS) {
		if (lnbd->subSectorGoal <= (bsp.subSectorCount + 1)) { // we will always make at least one more sub sector
			maxWidth = 1;
		} else if ((lnbd->subSectorGoal == (bsp.subSectorCount + 1)) && (lnbd->segGoal <= segCount)) {
			maxWidth = 1;
		} else {
			maxWidth = MaxWidth(nodeDepth, initialSegs, options );
		}
  	} else if (options->Metric == TREE_METRIC_BALANCED) {
		int mScore = ((lnbd->subSectorGoal - 1 ) * 2) + lnbd->segGoal;

		if (mScore <= ((bsp.subSectorCount * 2 ) + segCount )) {
			// printf("hmm\n");
			maxWidth = 1;
		} else {
			//printf("noo\n");
			//printf("%d and %d\n", mScore, ((ssectorCount * 2 ) + segCount ));
			maxWidth = MaxWidth(nodeDepth, initialSegs, options );
		}
	} else {
		maxWidth = MaxWidth(nodeDepth, initialSegs, options );
	}

	if (maxWidth > 1) {
		if (maxWidth > lnbd->localMaxWidth) {
			maxWidth = lnbd->localMaxWidth;
		}
	}

	/*
	   if (lnbd->goodScoresFound < maxWidth) {
	   maxWidth = lnbd->goodScoresFound + 2;
	   }
	   */

	// printf("maxwidth %d\n", maxWidth);

	// maxWidth = 2;

	if (maxWidth > 1) {
		int noneEdges = 0;
		for (int cnt = 0; cnt != *noSegs; cnt++) {
			if (!(segs[cnt].flags & SEG_EDGE)) {
				noneEdges++;
				if (noneEdges == maxWidth) {
					break;
				}
			}
		}
		maxWidth = noneEdges;
	}

	if (maxWidth < 2) {
		maxWidth = 1;
	}

	/*
	   if (deepinit == false) {
	   for (int i = 0; i != 256; i++) {
	   deep[i] = 0;
	   deepinit = true;
	   }

	   } else {

	   if (updated < time(NULL)) {

	   if (nodeDepth < 256) {
	   deep[nodeDepth]++;
	   printf("\n");
	   for (int i = 0; i != 250; i++) {
	   printf("%d", deep[i]);
	   }
	   } else {
	   printf("very deep!");
	   }
	   printf("\n");

	   }
	   */


int nodesLeftBackup = nodesLeft;
int segCountBackup = segCount;
int ssectorCountBackup = bsp.subSectorCount;
int ssectorsLeftBackup = ssectorsLeft;
int noSegsBackup = *noSegs;
int nodeCountBackup = nodeCount;
int nodePoolEntriesBackup = nodePoolEntries; // how many did we have at start
//int ssectorPoolEntriesBackup = ssectorPoolEntries;
// int maxVerticesBackup = maxVertices;
int maxSegsBackup = maxSegs;
int *convexPtrBackup = bsp.convexPtr;
int *cptrBackup = cptr;

// SEG *CNbestSegs = NULL;

char *currentSideBackup = bsp.currentSide;

wSSector *ssectorPoolBackup = NULL;
NODE *nodePoolBackup = NULL;

SEG *segsStartBackup = NULL;
int *convexListBackup = NULL;
char *lineUsedBackup = NULL;;

char **sideInfoBackup = NULL;
int currentAliasBackup = bsp.currentAlias;

double XBackup = bsp.X;
double YBackup = bsp.Y;
double DXBackup = bsp.DX;
double DYBackup = bsp.DY;

long ANGLEBackup = bsp.ANGLE;

// We only backup if we plan to do more than one line pick
if (maxWidth > 1) {
	// segsStartBackup = new SEG [segCount];

	segsStartBackup = (SEG *) AllocateSeg(segCount);

	memcpy(segsStartBackup, segStart, sizeof ( SEG) * (segCount));
	//

	// printf("copied seg %d, ", sizeof ( SEG) * (segCount));


	convexListBackup = new int[noAliases];
	memcpy (convexListBackup, bsp.convexList, sizeof( int) * (noAliases));

	// printf("cvxlist & lineused %d, ", sizeof( int) * (noAliases) * 2);

	lineUsedBackup    = new char [ noAliases ];
	memcpy(lineUsedBackup, lineUsed, sizeof(char) * (noAliases));

	bsp.ANGLE = ANGLEBackup;

	ssectorPoolBackup = new wSSector [ssectorPoolEntries];
	// memcpy ( ssectorPoolBackup, ssectorPool, sizeof ( wSSector) * (ssectorPoolEntries) );
	memcpy ( ssectorPoolBackup, ssectorPool, sizeof ( wSSector) * (bsp.subSectorCount) );

	// printf("ssectors %d, ", sizeof ( wSSector) * (ssectorCount) );

	nodePoolBackup = new NODE [nodePoolEntries];
	// memcpy (nodePoolBackup, nodePool, sizeof ( NODE) * (nodePoolEntries));
	memcpy (nodePoolBackup, nodePool, sizeof ( NODE) * (nodeCount));

	// printf("nodes %d", sizeof ( NODE) * (nodeCount));

	// printf("\n");
}
nodeDepth++;

int CNbestSegsCount = 65536;
// int CNbestTempSegEntries;
// int CNbestSegStartEntries;
int CNbestSsectorsCount = 65536;
int CNbestNodesCount;
int CNbestNodesLeft;
int CNbestNoVertices;
int CNbestNoSegs;
int CNbestSsectorsLeft;
int CNbestMaxSegs;
int *CNbestCptr = NULL;
int *CNbestConvexPtr = NULL;
int CNbestCurrentAlias;
// int CNBestNodeCount;
int CNbestNodePoolEntries;
int CNbestSsectorPoolEntries;

int CNbestNoRight, CNbestNoLeft;

char *CNbestCurrentSide = NULL;

//	double CNbestX;
//	double CNbestY;
//	double CNbestDX;
//	double CNbestDY;

//	long CNbestANGLE;

SEG *bestSegs = NULL;
SEG *bestTempSeg = NULL;
wSSector *bestSSectors = NULL;
NODE *bestNodes = NULL;
int *bestConvexList = NULL;
char *bestLineUsed = NULL;

// int wideSegs[options->Width];


std::vector<int> wideSegs(options->Width);

for (int k = 0; k < options->Width; k++) {
	wideSegs[k] = -1;
}


differentpartition:
if (lnbd->width != 1) {

	lnbd->subSectorGoal = bsp.subSectorCount;
	// subSectorGoal = bsp.ssectorCount;

	lnbd->segGoal = segCount;
	// segGoal = segCount;


	memcpy (segStart, segsStartBackup, sizeof ( SEG) * (segCountBackup));

	// memcpy (segStart + (inSeg * sizeof(SEG)), segsStartBackup, sizeof ( SEG) * (segCountBackup - inSeg));

	segs = &segStart[inSeg];	

	// restore data from our saved backup

	bsp.currentAlias = currentAliasBackup;

	nodeCount = nodeCountBackup;
	*noSegs = noSegsBackup;
	bsp.subSectorCount = ssectorCountBackup;
	ssectorsLeft = ssectorsLeftBackup;
	segCount = segCountBackup;

	nodesLeft =  nodesLeftBackup;

	if (lnbd->width == maxWidth) {
		delete [] lineUsed;
		lineUsed = lineUsedBackup;
		lineUsedBackup = NULL;

	} else {
		memcpy (lineUsed, lineUsedBackup, sizeof(char) * ( noAliases));
	}
	memcpy (bsp.convexList, convexListBackup, sizeof( int) * (convexListEntries));


	bsp.convexPtr = convexPtrBackup;
	cptr = cptrBackup;
	bsp.currentSide = currentSideBackup;

	memcpy ( nodePool, nodePoolBackup, sizeof ( NODE) * (nodePoolEntriesBackup));

	bsp.X = XBackup;
	bsp.Y = YBackup;
	bsp.DX = DXBackup;
	bsp.DY = DYBackup;
	bsp.ANGLE = ANGLEBackup;
}

// printf("Choose p. noSegs=%d, width=%d\n", *noSegs, width);

// if (( *noSegs <= 1 ) || ( ChoosePartition ( segs, *noSegs, &noLeft, &noRight, options, level, &width, wideSegs, &localPicks) == false )) {

// lnbd->width = width;
std::vector<int> oldWideSegs = lnbd->wideSegs;
lnbd->wideSegs = wideSegs;

#undef DEPTHINFO

#ifdef DEPTHINFO


nodeCounter++;

if (deepinit == false) {
	for (int i = 0; i != 256; i++) {
		deep[i] = 0;
		deepinit = true;
	}

	printf("\n%c[2A\n", 27);

	for (int i = 0; i != 6; i++) {
		wideStats[i] = 0;
	}


} else {
	deep[nodeDepth] = (lnbd->width * 2) - 1;

	for (int i = nodeDepth + 1; i != 180; i++) {
		deep[i] = 0;
	}		

	if (nodeCounter == 2000000) {

		time_t nuh = time(NULL);

		nodeCounter = 0;


		if ((updated < nuh) || false) {

			if (nodeDepth < 256) {

				deep[nodeDepth] = lnbd->width;

				// printf("%c[s%c[0;0H", 27, 27);

				printf("\n%c[B%c[200D", 27, 27);

				int lowTwo = 200;
				int lowOne = 200;

				// int maxValue = 0;

				char output[512];

				for (int i = 0; i != 180; i++) {
					if (deep[i] > 0) {

						if (i > maxDepth) {
							maxDepth = i;
						}
					}

					if (deep[i] > 2) {
						if (i < lowTwo) {
							lowTwo = i;	
						}
					}

				}

				int i;

				for (i = 1; i != maxDepth; i++) {

					if (deep[i] < 10) {
						output[i - 1] = 48 + deep[i];
					} else if (deep[i] < 37) {
						output[i - 1] = 87 + deep[i];
					} else {
						output[i - 1] = 46 + deep[i];
					}

					// printf("%d", deep[i]);
				}

				output[i] = 0;
				printf(output);

				// printf("%c[B", 27);
				// printf(" |");
				//

				// the trailing space is actually important when going from 10 to 9 etc
				printf(" | 1st wide: %d |                                  Good scores %d   ", lowTwo, goodScoresUsed);

				printf("%c[2A\n", 27);

			}
			updated = nuh;

			// printf("\n\n1111\n\n");

		}
	}
}
#endif








if (( *noSegs <= 1 ) || ( ChoosePartition ( segs, *noSegs, &noLeft, &noRight, lnbd) == false)) {

	// delete lnbd;

	// restore wideSegs
	// nbd->wideSegs = oldWideSegs;		

	// *lnbd->maxPicks = 0;

	// cleanup!
	if (maxWidth > 1) {
		if (segsStartBackup) {
			// delete [] segsStartBackup;
			FreeSeg(segsStartBackup);
		}
		if (convexListBackup) {
			delete [] convexListBackup;
		}

		if (lineUsedBackup) {
			delete [] lineUsedBackup;
		}
		//			delete [] sideInfoBackup;
		delete [] nodePoolBackup;
		delete [] ssectorPoolBackup;
	}

	nodeDepth--;

	if (lnbd->width > 1) {
		if (lnbd->width < 900) {
			// printf("mega bug\n");
		}
	}

	bsp.convexPtr = cptr;
	if ( KeepUniqueSubsectors ( segs, *noSegs ) == true ) {
		ArrangeSegs ( segs, *noSegs );
		return ( UINT16 )  GenerateUniqueSectors ( segs, *noSegs );
	}
	// if ( showProgress ) ShowDone ();
	return ( UINT16 ) ( 0x8000 | CreateSSector ( segs, *noSegs ));
}

// printf("%d\n", lnbd->goodScoresFound);


bool goodScoreAbort = false;


if ((lnbd->width != 0) && ( lnbd->goodScoresFound < maxWidth)) {

	if (maxWidth - 1 == lnbd->width) {
		goodScoreAbort = true;
	}

	goodScoresUsed++;

}

lnbd->wideSegs = oldWideSegs;

// *lnbd->maxPicks = localPicks;

bool betterTree = false;

// we set width to -1 IF we have a faulty non-first line pick.

UINT16 rNode, lNode;

if (lnbd->width != -1) {

	NODE tempNode;

	// Store the NODE info set in ComputeStaticVariables
	tempNode.x  = ( INT16 ) lrint ( bsp.X );
	tempNode.y  = ( INT16 ) lrint ( bsp.Y );
	tempNode.dx = ( INT16 ) lrint ( bsp.DX );
	tempNode.dy = ( INT16 ) lrint ( bsp.DY );


#if defined ( DIAGNOSTIC )
	double x1 = bsp.X;
	double y1 = bsp.Y;
	double x2 = bsp.X + bsp.DX;
	double y2 = bsp.Y + bsp.DY;
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

	int alias = bsp.currentAlias;

	lineUsed [ alias ] = 1;
	for ( int *tempPtr = cptr; tempPtr != bsp.convexPtr; tempPtr++ ) {
		lineUsed [ *tempPtr ] = 2;
	}

	bool swapped = false;	
	// int rightPicks = *lnbd->maxPicks;



	// rNode = CreateNode ( inSeg, &noRight, options, level, segGoal, subSectorGoal, &rightPicks);
	rNode =  CreateNode ( inSeg, &noRight, lnbd);
	DepthProgress(nodeDepth, initialSegs, options);

	/*
	   leftPicks = localPicks - rightPicks;
	   nbd->maxPicks = &leftPicks;
	   */

	int oldLMW = lnbd->localMaxWidth;
	lnbd->localMaxWidth = nbd->scoresFound - lnbd->scoresFound - 1;
	// lnbd->localMaxWidth = nbd->scoresFound - 1;

	// 	printf("lmw: %d, scores: %d, local scores: %d\n", lnbd->localMaxWidth,  nbd->scoresFound, lnbd->scoresFound);


	deep[nodeDepth] = (lnbd->width * 2) - 0 ;
	/*
	   printf("%d\n", lnbd->width);

	   for (int i = nodeDepth + 1; i != 180; i++) {
	   deep[i] = 0;
	   }
	   */
	// lNode = CreateNode ( inSeg + noRight, &noLeft, options, level, segGoal, subSectorGoal, &leftPicks);
	lNode = CreateNode ( inSeg + noRight, &noLeft, lnbd);

	// restore
	lnbd->localMaxWidth = oldLMW;

	DepthProgress(nodeDepth, initialSegs, options);

	// Restore segs!
	//


	segs = &segStart[inSeg];

	*noSegs = noLeft + noRight;

#if defined ( DIAGNOSTIC )

	//		printf("\nVerifying right #%d, width %d, depth %d\n", inSeg, width, nodeDepth);
	VerifyNode ( segs, noRight, x1, y1, x2, y2 );

	//		printf("\nVerifying left #%d, width %d, depth %d\n", inSeg + noRight, width, nodeDepth);
	VerifyNode ( segs + noRight, noLeft, x2, y2, x1, y1 );

#endif
	while ( bsp.convexPtr != cptr ) lineUsed [ *--bsp.convexPtr ] = false;
	lineUsed [ alias ] = false;

	// if ( nodesLeft-- == 0 ) {

	if (nodeCount == nodePoolEntries) {
		int delta  = ( 5 * nodeCount ) / 100 + 1;

		nodePoolEntries = nodeCount + delta; // zokum 2017

		NODE *nodePoolRealloc = new NODE[nodePoolEntries];
		memcpy (nodePoolRealloc, nodePool, sizeof ( NODE) * (nodeCount) );
		delete [] nodePool;

		if (nodePoolRealloc) {
			nodePool = nodePoolRealloc;
		} else {
			printf("FATAL ERROR: Out of memory in CreateNode()");
		}

		nodesLeft += delta;
	}

	tempNode.child [0] = rNode;
	tempNode.child [1] = lNode;

	nodePool [nodeCount] = tempNode;

	if (options->Metric == TREE_METRIC_SUBSECTORS) {
		if (bsp.subSectorCount < CNbestSsectorsCount) {
			betterTree = true;
			wideStats[lnbd->width]++;
		} else if (bsp.subSectorCount == CNbestSsectorsCount) {
			if ((segCount < CNbestSegsCount)) {
				betterTree = true;
				wideStats[lnbd->width]++;
			} else if ((segCount == CNbestSegsCount) && (lnbd->width == maxWidth)) {
				betterTree = true;
			}
		}
	} else if (options->Metric == TREE_METRIC_SEGS) {
		if (segCount < CNbestSegsCount) {
			betterTree = true;
			wideStats[lnbd->width]++;
		} else if (segCount == CNbestSegsCount) {
			if (bsp.subSectorCount < CNbestSsectorsCount) {
				betterTree = true;
				wideStats[lnbd->width]++;
			} else if ((bsp.subSectorCount == CNbestSsectorsCount) && (lnbd->width == maxWidth)) {
				betterTree = true;
			}
		}
	} else if (options->Metric == TREE_METRIC_BALANCED) {
		int segdiff = segCount - CNbestSegsCount;
		int ssdiff = (bsp.subSectorCount - CNbestSsectorsCount) * 2;

		if ((segdiff + ssdiff) < 0) {
			betterTree = true;
		}

		if ((segdiff + ssdiff) == 0) {
			if (lnbd->width == maxWidth) {
				betterTree = true;
			}
		}

	} 

	// Sneaky optimization 2018, nov 26
	if (nodeCount == nodeCountBackup) {
		/*if (width != maxWidth) {
		  printf("optimizing\n");
		  }*/

	//	lnbd->width = maxWidth;
	}


	if (goodScoreAbort) {
		// printf("exiting\n");

		if (bestSegs) {
			delete [] bestSegs;
			delete [] bestSSectors;
			delete [] bestNodes;
			delete [] bestTempSeg;
		}

		if (bestLineUsed) {
			delete [] bestLineUsed;
		}

		if (bestConvexList) {
			if (bestConvexList != bsp.convexList) {
				delete [] bestConvexList;
			}
		}

		if (segsStartBackup) {
			FreeSeg(segsStartBackup);
		}
		if (convexListBackup) {
			delete [] convexListBackup;
		}
		if (lineUsedBackup) {
			delete [] lineUsedBackup;
		}
		delete [] nodePoolBackup;
		delete [] ssectorPoolBackup;

		delete lnbd;

		nodeDepth--;

		return ( UINT16 ) nodeCount++;
	}

	// Was it a better set of sub trees, and do we have more to check?
	if (betterTree && ((lnbd->width /* + widthOffset*/) < maxWidth)) {

		// printf("more trees\n");

		if (bestSegs) {
			delete [] bestSegs;
			// FreeSeg(bestSegs);
			delete [] bestSSectors;
			delete [] bestNodes;
			// delete [] bestConvexList;
			// delete [] bestLineUsed;
			delete [] bestTempSeg;
		} 

		// October 2018
		bestSegs = segStart;
		segStart = new SEG [maxSegs];
		//segStart = AllocateSeg(maxSegs);

		bestSSectors = ssectorPool;
		ssectorPool = new wSSector [ssectorPoolEntries];

		// moved down here
		memcpy ( ssectorPool, ssectorPoolBackup, sizeof ( wSSector) * ( ssectorCountBackup));

		// CNbestNodePoolEntries = nodePoolEntries;

		bestNodes = nodePool;
		nodePool = new NODE [nodePoolEntries];

		if (bestConvexList == NULL) {
			bestConvexList = new int[convexListEntries];
		}
		memcpy (bestConvexList, bsp.convexList, sizeof ( int ) * (convexListEntries));

		if (bestLineUsed == NULL) {
			bestLineUsed = lineUsed;
			lineUsed = new char[noAliases];
		} else {
			char *tmp = bestLineUsed;
			bestLineUsed = lineUsed;
			lineUsed = tmp;
		}
		CNbestSegsCount = segCount;
		CNbestSsectorsCount = bsp.subSectorCount;
		CNbestSsectorsLeft = ssectorsLeft;
		CNbestNoVertices = bsp.noVertices;

		CNbestNodesCount = nodeCount;
		CNbestNodesLeft = nodesLeft;
		// CNbestSegs = segs;
		CNbestNoSegs = *noSegs;
		CNbestMaxSegs = maxSegs;
		CNbestCptr = cptr;
		CNbestConvexPtr = bsp.convexPtr;

		CNbestNoRight = noRight;
		CNbestNoLeft = noLeft;

		CNbestCurrentAlias = bsp.currentAlias;
		CNbestCurrentSide = bsp.currentSide;

		// CNbestX = bsp.X;
		// CNbestY = bsp.Y;
		// CNbestDX = bsp.DX;
		// CNbestDY = bsp.DY;
		// CNbestANGLE = bsp.ANGLE;

		CNbestNodePoolEntries = nodePoolEntries;
		CNbestSsectorPoolEntries = ssectorPoolEntries;
	}
	// rerite logic here to better reflect width 
	lnbd->width++;

	if (		((lnbd->width /* + widthOffset*/) <= maxWidth)
			&& 	(lnbd->width < *noSegs) 
			&&	(noMoreScores == false)
	   ) {
		goto differentpartition;
	}
	lnbd->width--;
}

// Cleanup if we looked for several trees
if (maxWidth > 1) {
	if (segsStartBackup) {
		// delete [] segsStartBackup;
		FreeSeg(segsStartBackup);
	}
	if (convexListBackup) {
		delete [] convexListBackup;
	}
	if (lineUsedBackup) {
		delete [] lineUsedBackup;
	}
	delete [] nodePoolBackup;
	delete [] ssectorPoolBackup;
}

// Last tree was not best, restore that one

bool reassigned = false;

if (betterTree == false) {
	maxSegs = CNbestMaxSegs;

	memcpy(bsp.convexList,      bestConvexList, sizeof ( int ) *        (convexListEntries));
	memcpy(lineUsed,        bestLineUsed, sizeof( char ) *          (noAliases));

	// Working
	reassigned = true;

	delete [] segStart;
	//FreeSeg (segStart);
	segStart = bestSegs;

	delete [] nodePool;
	nodePool = bestNodes;

	delete [] ssectorPool;
	ssectorPool = bestSSectors;

	nodeCount = CNbestNodesCount;
	nodesLeft = CNbestNodesLeft;
	bsp.subSectorCount = CNbestSsectorsCount;
	ssectorsLeft = CNbestSsectorsLeft;
	*noSegs = CNbestNoSegs;
	segCount = CNbestSegsCount;
	bsp.noVertices = CNbestNoVertices;
	cptr = CNbestCptr;
	bsp.convexPtr = CNbestConvexPtr;
	noRight = CNbestNoRight;
	noLeft = CNbestNoLeft;
	// segs = &segStart[inSeg];
	bsp.currentAlias = CNbestCurrentAlias;
	bsp.currentSide = CNbestCurrentSide;

	nodePoolEntries = CNbestNodePoolEntries;
	ssectorPoolEntries = CNbestSsectorPoolEntries;
}

nodeDepth--;
// printf("depth %d, width %d, segs %d, ssectors %d, nodes %d\n", nodeDepth, lnbd->width, segCount, ssectorCount, nodeCount);

if (reassigned == false) {
	delete [] bestSegs;
	// FreeSeg(bestSegs);
	delete [] bestNodes;
	delete [] bestSSectors;		
}
delete [] bestConvexList;
delete [] bestLineUsed;

delete lnbd;

return ( UINT16 ) nodeCount++;
}

wVertex  *GetVertices () {
	FUNCTION_ENTRY ( NULL, "GetVertices", true );

	wVertex *vert = new wVertex [ bsp.noVertices ];
	memcpy ( vert, bsp.newVertices, sizeof ( wVertex ) * bsp.noVertices );

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

wSSector *GetSSectors ( DoomLevel *level, wSSector *ssectorList, int noSSectors, sBSPOptions *options) {
	FUNCTION_ENTRY ( NULL, "GetSSectors", true );

	wSegs *segs = finalSegs = new wSegs [ segCount ];

	wLineDefInternal *ld; //  = level->GetLineDefs ();


	for ( int i = 0; i < noSSectors; i++ ) {

		int start = ssectorList[i].first;
		ssectorList[i].first = ( UINT16 ) ( segs - finalSegs );

		// Copy the used SEGs to the final SEGs list
		for ( int x = 0; x < ssectorList [i].num; x++ ) {
			// 11.06.2018 adding vertex			

			// segs [x] = segStart [start+x].Data;

			segs [x].start = ( UINT16 ) AddVertex ( lrint ( segStart[start+x].start.x), lrint ( segStart[start+x].start.y ));
			segs [x].end   = ( UINT16 ) AddVertex ( lrint ( segStart[start+x].end.x),   lrint ( segStart[start+x].end.y ));			

			segs[x].angle = segStart [start+x].Dataangle;
			segs[x].lineDef = segStart [start+x].DatalineDef;

			// segs[x].flip = segStart [start+x].Dataflip;

			if (segStart [start+x].flags & SEG_BACKSIDE) {
				segs[x].flip = 1;
			} else {
				segs[x].flip = 0;
			}

			UINT16 offset = 0;

		
			// moved out of if blocks
			ld = &level->GetLineDefs ()[segStart[start+x].DatalineDef] ;

			if (ld->type == 1088) {
				int oldLD = segStart[start+x].DatalineDef;
				int duplicatedLineDef = ld->tag;

				segs[x].lineDef = ld->tag;

				// printf("Linedef changedfrom %d to %d.\n", oldLD, duplicatedLineDef );

			}


			if (segs[x].flip == 0) {

				// ld = &level->GetLineDefs ()[segStart[start+x].DatalineDef] ;

				// if (segs [x].start != segStart[start+x].LineDef->start) {
				if (segs [x].start != ld->start) {
					// printf("SS %3d, seg %4d Frontside offset: %4d | ", i, x,  segStart [start+x].Dataoffset);

					wVertex *vertA = &bsp.newVertices [segs [x].start];
					// wVertex *vertB = &bsp.newVertices [segStart[start+x].LineDef->start];
					wVertex *vertB = &bsp.newVertices [ ld->start];

					offset = ( hypot (( double ) ( vertB->x - vertA->x ), ( double ) ( vertB->y - vertA->y )) + 0.5 );

					//printf("%4d (%d)\n", offset, segStart [start+x].Dataoffset - offset);

				} 
			} else {
				// ld = &level->GetLineDefs ()[segStart[start+x].DatalineDef] ;

				//if (segs [x].start != segStart[start+x].LineDef->end) {
				if (segs [x].start != ld->end) {
					// printf("SS %3d, seg %4d Backside  offset: %4d | ", i, x, segStart [start+x].Dataoffset);

					wVertex *vertA = &bsp.newVertices [segs [x].start];
					// wVertex *vertB = &bsp.newVertices [segStart[start+x].LineDef->end];

					wVertex *vertB = &bsp.newVertices [ ld->end];

					offset = ( hypot (( double ) ( vertB->x - vertA->x ), ( double ) ( vertB->y - vertA->y )) + 0.5 );

					// printf("%4d (%d)\n", offset, segStart [start+x].Dataoffset - offset);




				}

			}


			// segs[x].offset = segStart [start+x].Dataoffset;

			segs[x].offset = offset;


			/*
			   wVertex *vertS = &bsp.newVertices [ ld[segStart[start+x].LineDef]->start ];
			   wVertex *vertE = &bsp.newVertices [ ld[ ]->end ];
			// long dx = vertE->x - vertS->x;
			// long dy = vertE->y - vertS->y;



			if (segs[x].flip == 0) {
			if (segStart[start+x].start.x != ld[segStart[start+x].LineDef].x)

			}					
			DatalineDef
			 */


			// lSeg->Dataoffset = ( UINT16 ) ( hypot (( double ) ( x - vertS->x ), ( double ) ( y - vertS->y )) + 0.5 );
			// rSeg->Dataoffset = ( UINT16 ) ( hypot (( double ) ( x - vertS->x ), ( double ) ( y - vertS->y )) + 0.5 );


			// segs [x].start = ( UINT16 ) AddVertex ( lrint ( segStart[start+x].start.x), lrint ( segStart[start+x].start.y ));
			//segs [x].end   = ( UINT16 ) AddVertex ( lrint ( segStart[start+x].end.x),   lrint ( segStart[start+x].end.y ));


			/*
			   if (	((int) 	segStart[start+x].start.x != bsp.newVertices[segStart[start+x].Data.start].x) ||
			   ((int)	segStart[start+x].start.y != bsp.newVertices[segStart[start+x].Data.start].y) ||
			   ((int)	segStart[start+x].end.x != bsp.newVertices[segStart[start+x].Data.end].x) ||
			   ((int)	segStart[start+x].end.y != bsp.newVertices[segStart[start+x].Data.end].y)
			   ) {	
			   printf("\nStart: bsp.X %d|%f  Y: %d|%f\nEnd:   bsp.X: %d|%f   Y: %d|%f\n", 
			   segStart[start+x].start.x, 
			   bsp.newVertices[segStart[start+x].Data.start].x,

			   segStart[start+x].start.y,
			   bsp.newVertices[segStart[start+x].Data.start].y,

			   segStart[start+x].end.x,
			   bsp.newVertices[segStart[start+x].Data.end].x,

			   segStart[start+x].end.y,
			   bsp.newVertices[segStart[start+x].Data.end].y


			   );
			 *//*
			      } else {*/
			/*
			   struct wSegs {
			   UINT16      start;      
			   UINT16      end;        
			   UINT16      angle;      
			   UINT16      lineDef;    
			   UINT16      flip;       
			   UINT16      offset;     
			   };
			 */



			// segs[x].start = ( UINT16 ) AddVertex ( lrint ( segStart[start+x].start.x ), lrint ( segStart[start+x].start.y ));
			// segs[x].end   = ( UINT16 ) AddVertex ( lrint ( segStart[start+x].end.x ), lrint ( segStart[start+x].end.y ));

			// Angle code, c&p
			BAM angle;
			/*
			 * These 4 new linedef types allow for arbitrary rotation of the ways walls are rendered compared to
			 * the base linedef. Check docs for more info.
			 */

			// double a2;

			// const wLineDefInternal *lineDef = segStart[start+x].LineDef;
			const wLineDefInternal *lineDef = &level->GetLineDefs ()[segStart[start+x].DatalineDef] ;

			long dx;
			long dy;

			if (options->segBAMs) {
				// printf("seg\n");
				dx = segStart[start+x].end.x - segStart[start+x].start.x;
				dy = segStart[start+x].end.y - segStart[start+x].start.y;
			} else {
				// printf("seg:%2d l:%d r:%d - ", segStart[start+x].Side, SIDE_RIGHT, SIDE_LEFT);

				//if (segStart[start+x].Side == -1) {
				dx = (long) (bsp.newVertices[lineDef->end].x - bsp.newVertices[lineDef->start].x);
				dy = (long) (bsp.newVertices[lineDef->end].y - bsp.newVertices[lineDef->start].y); /*
													      } else {
													      dx = (long) (newVertices[lineDef->start].x - bsp.newVertices[lineDef->end].x);
													      dy = (long) (newVertices[lineDef->start].y - bsp.newVertices[lineDef->end].y);
													      }
													    */
			}

			if (lineDef->type == 1081) { // linedef special to force angle to degrees
				// angle = ((BAM)lineDef->tag * BAM360) / (BAM)360;
				angle = (BAM) ((lineDef->tag * BAM360) / 360);
			} else if (lineDef->type == 1083) { // linedef special to force angle to BAM
				angle = (BAM)lineDef->tag;
			} else {
				// based on BSP's code, cleaner and simpler :)

				/*
				   angle = (dy == 0) ? (BAM)((dx < 0) ? BAM180 : 0) :
				   (dx == 0) ? (BAM)((dy < 0) ? BAM270 : BAM90) :
				   (BAM)(atan2((float)dy, (float)dx) * BAM180 / M_PI + 0.5 * sgn(dy));
				 */

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

			if (!options->segBAMs) {
				// if (segStart[start+x].Dataflip) {
				if (segStart[start+x].flags & SEG_BACKSIDE) {
					angle =	( BAM ) ( angle + BAM180 );
				}
			}

			if (segs[x].angle != angle) {

				//printf("Bleh 1 %d\n", ComputeAngle(2752 - 3048, -3048 - (-2880)));
				// printf("Bleh 2 %d\n", ComputeAngle(

				// printf("Angle change %4d,%-4d -> %5d != %5d | %3d \n", dx,dy, segs[x].angle, angle, segs[x].angle - angle);
			}

			// angle += 256;

			/*else {
			// printf("angle ok\n");
			 */
			segs[x].angle = angle;
			// }

			/*			} */


			// segs [x] = segStart [start+x].Data;
			}

			segs += ssectorList [i].num;
			}

			// delete [] segStart;
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

			void PostAddVertices(void) {

				for ( int i = 0; i < segCount; i++ ) {
					// segStart[i].Datastart = ( UINT16 ) AddVertex ( lrint ( segStart[i].start.x ), lrint ( segStart[i].start.y ));
					// segStart[i].Dataend   = ( UINT16 ) AddVertex ( lrint ( segStart[i].end.x ),  lrint ( segStart[i].end.y ));
				}

			}

			void PostSortSegs( wSSector *ssectors) {
				for (int i = 0; i != bsp.subSectorCount; i++) {

					if (ssectors[i].num > 1) {
						qsort ( &segStart[ssectors[i].first], ssectors[i].num, sizeof ( SEG ), SortByLineDef );
					}
				}
			}

			void PostMakeDoomNodes(wNode *doom) {
				for (int i = 0; i != nodeCount; i++) {
					doom[i].x = nodePool[i].x;
					doom[i].y = nodePool[i].y;

					doom[i].dx = nodePool[i].dx;
					doom[i].dy = nodePool[i].dy;	

					doom[i].child[0] = nodePool[i].child[0];
					doom[i].child[1] = nodePool[i].child[1];
				}
			}

			void PostFindBounds( wNode *node) {

				// wBound bounds[2];

				// If it's a subSector, calculate from segs!
				// If not, recursively figure it out.

				for (int nodeChild = 0; nodeChild != 2; nodeChild++) {

					// If a child is a subsector, we can use the segs to get bounds

					// printf("Checking side %d\n", nodeChild);
					//

					if (node->child[nodeChild] & 0x8000) {

						int subSector = node->child[nodeChild] - 0x8000;

						// printf(" FindBounds: child: %d, subsector: %d, first: %d, num %d\n", &node->side[nodeChild], subSector, ssectorPool[subSector].first, ssectorPool[subSector].num);

						FindBounds( &node->side[nodeChild], &segStart [ssectorPool[subSector].first], ssectorPool[subSector].num);
						// printf("segs in ss %d %d\n", subSector, ssectorPool[subSector].num);

						// The child is another node, we need to figure out it's bounds and then use the bounds
					} else { 

						// printf(" PostFind recursive on node %d\n", node->child[nodeChild]);

						PostFindBounds( &doomNodes[node->child[nodeChild]]); // recursion

						// Check the children's two sides on this side to get this node's side

						// printf("\nBefore: %d, %d, %d, %d\n", node->side[nodeChild].minx, node->side[nodeChild].miny, node->side[nodeChild].maxx, node->side[nodeChild].maxy );			

						if (doomNodes[node->child[nodeChild]].side[0].minx < doomNodes[node->child[nodeChild]].side[1].minx) {
							node->side[nodeChild].minx = doomNodes[node->child[nodeChild]].side[0].minx;
						} else {
							node->side[nodeChild].minx = doomNodes[node->child[nodeChild]].side[1].minx;
						}

						if (doomNodes[node->child[nodeChild]].side[0].miny < doomNodes[node->child[nodeChild]].side[1].miny) {
							node->side[nodeChild].miny = doomNodes[node->child[nodeChild]].side[0].miny;
						} else {
							node->side[nodeChild].miny = doomNodes[node->child[nodeChild]].side[1].miny;
						}

						if (doomNodes[node->child[nodeChild]].side[0].maxx > doomNodes[node->child[nodeChild]].side[1].maxx) {
							node->side[nodeChild].maxx = doomNodes[node->child[nodeChild]].side[0].maxx;
						} else {
							node->side[nodeChild].maxx = doomNodes[node->child[nodeChild]].side[1].maxx;
						}

						if (doomNodes[node->child[nodeChild]].side[0].maxy > doomNodes[node->child[nodeChild]].side[1].maxy) {
							node->side[nodeChild].maxy = doomNodes[node->child[nodeChild]].side[0].maxy;
						} else {
							node->side[nodeChild].maxy = doomNodes[node->child[nodeChild]].side[1].maxy;
						}


						//printf("After:  %d, %d, %d, %d\n", node->side[nodeChild].minx, node->side[nodeChild].miny, node->side[nodeChild].maxx, node->side[nodeChild].maxy );

					}
				}
			}

			//----------------------------------------------------------------------------
			//  Wrapper function that calls all the necessary functions to prepare the
			//    BSP tree and insert the new data into the level.  All screen I/O is
			//    done in this routine (with the exception of progress indication).
			//----------------------------------------------------------------------------
			/*
			   bool lowestSegCountFound = false;
			 */
			void CreateNodesStructure ( DoomLevel *level, sBSPOptions *options ) {
				FUNCTION_ENTRY ( NULL, "CreateNODES", true );

				TRACE ( "Processing " << level->Name ());

				BSPData bd;
				bd.options = options;

				// Sanity check on environment variables
				if ( bsp.X2 <= 0 ) bsp.X2 = 1;
				if ( bsp.Y2 <= 0 ) bsp.Y2 = 1;

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
				} else if ( options->algorithm == 6 ) {
					PartitionFunction = AlgorithmVertex;
				} else { // usually == 1
					PartitionFunction = AlgorithmFewerSplits;
				}

				if ( !((PartitionFunction == AlgorithmMulti) || (PartitionFunction == AlgorithmVertex)  )) {
					options->Width = 1; // never build wide trees unless we have to :)
				}
// restart:

				InitBSPState();

				earlyExit = false;

				nodeCount    = 0;
				bsp.subSectorCount = 0;

				// Get rid of old SEGS and associated vertices
				level->NewSegs ( 0, NULL );
				level->TrimVertices ();
				level->PackVertices ();

				// Put em here after vertex packing etc

				bsp.noVertices  = level->VertexCount ();
				bsp.sectorCount = level->SectorCount ();
				bsp.usedSector  = new UINT8 [ bsp.sectorCount ];
				keepUnique  = new bool [ bsp.sectorCount ];
				if ( options->keepUnique ) {
					uniqueSubsectors = true;
					memcpy ( keepUnique, options->keepUnique, sizeof ( bool ) * bsp.sectorCount );
				} else {
					memset ( keepUnique, true, sizeof ( bool ) * bsp.sectorCount );
				}
				maxVertices = ( int ) ( bsp.noVertices * FACTOR_VERTEX );
				bsp.newVertices = ( wVertex * ) malloc ( sizeof ( wVertex ) * maxVertices );
				memcpy ( bsp.newVertices, level->GetVertices (), sizeof ( wVertex ) * bsp.noVertices );

				Status ( (char *) "Creating SEGS ... " );
				segStart = CreateSegs ( level, options );

				Status ( (char *) "Getting LineDef Aliases ... " );
				noAliases = GetLineDefAliases ( level, segStart, segCount );

				bsp.lineChecked = new char [ noAliases ];
				lineUsed    = new char [ noAliases ];
				memset ( lineUsed, false, sizeof ( char ) * noAliases );

				Status ( (char *) "      Creating Side Info ... " );
				CreateSideInfo ( level );

				bsp.convexList = new int [ noAliases ];
				convexListEntries = noAliases;
				bsp.convexPtr  = bsp.convexList;

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
					Status ( (char *) "Nodes (multi): " );
				} else if (options->algorithm == 6 ) {
					Status ( (char *) "Nodes (vertex): " );
				}


				nodesLeft   = ( int ) ( FACTOR_NODE * level->SideDefCount ());
				// nodePool    = ( NODE * ) malloc( sizeof ( NODE ) * nodesLeft );
				nodePool = new NODE [nodesLeft];
				nodePoolEntries = nodesLeft; // zokum 2017


				ssectorsLeft = ( int ) ( FACTOR_NODE * level->SideDefCount ());
				ssectorPool = new wSSector[ssectorsLeft];

				ssectorPoolEntries = ssectorsLeft;

				int noSegs = segCount;

				DepthProgress(-1, 1, options);	

				int picks = 32768;
				oldProgress[0] = -1.0; // reset progress in case we do many maps

				nodeBuilderData nbd;
				nbd.goodScoresFound = 999;

				nbd.options = options;
				nbd.level = level;
				nbd.segGoal = 999999;
				nbd.subSectorGoal = 999999;
				nbd.localMaxWidth = 999;

				nbd.width = 1;

				// Score is used by balanced and multi
				if ((options->algorithm == 2) || (options->algorithm == 5)) {
					bsp.score = new sScoreInfo [ noAliases ] ; // original
				} else if ((options->algorithm == 6) || (options->algorithm == 4)) {

					// how many possible partition lines do we have from root of the map?
					vertexSpace *vs = new  vertexSpace [noSegs * 2];

					// The amount is stored here
					nbd.vertices = GenerateVertexSpace(vs, segStart, noSegs );

					delete [] vs;

					bsp.score = new sScoreInfo [MAX_SCORES + 1];

				} else {
					bsp.score = NULL;
				}


				CreateNode (0, &noSegs, &nbd);

				// CreateNode ( 0, &noSegs, options, level, 99999, 99999, &picks);

				// Let's create Doom compatible nodes and then convert them
				doomNodes = (wNode *) malloc (sizeof( wNode) * nodeCount);

				// new 22.09.2018 memory optimization
				PostMakeDoomNodes(doomNodes);

				// new 22.09.2018 code
				// PostAddVertices();

				// new 29.05.2018 code
				PostFindBounds(&doomNodes[nodeCount - 1]);

				// new 21.09.2018 code
				PostSortSegs(ssectorPool);

				// new 02.10.2018 code
				// This doesn't quite work, yet :)
				// PostVisplaneReduction();

				delete [] bsp.convexList;
				if ( bsp.score ) delete [] bsp.score;

				// Clean up temporary buffers
				Status ( (char *) "         BSP           Cleaning up ... " );
				delete [] bsp.sideInfo;
				delete [] bsp.lineDefAlias;
				delete [] bsp.lineChecked;
				delete [] lineUsed;
				delete [] keepUnique;
				delete [] bsp.usedSector;

				bsp.sideInfo = NULL;

				// level->NewVertices ( bsp.noVertices, GetVertices ());
				level->NewNodes ( nodeCount, GetNodes ( doomNodes, nodeCount ));
				level->NewSubSectors ( bsp.subSectorCount, GetSSectors ( level, ssectorPool, bsp.subSectorCount, options ));

				level->NewVertices ( bsp.noVertices, GetVertices ());

				// New 21.09.2020 code
				// Post process seg stuff, duplicate textures for effects etc, 1088 tag

				wSegs *finalSegList = GetSegs();

				// Functiopnality moved to GetSSectors
				// PostSegDuplication(level, finalSegList);

				level->NewSegs ( segCount, finalSegList);

				free ( bsp.newVertices );
				// free ( ssectorPool );
				delete [] ssectorPool;
				// free ( nodePool );
				delete [] nodePool;

				delete [] tempSeg;

			}

			void InitBSPState() {
				bsp.X1 = GetEnvLong("ZEN_X1", 20);
				bsp.X2 = GetEnvLong("ZEN_X2", 10);
				bsp.X3 = GetEnvLong("ZEN_X3", 1);
				bsp.X4 = GetEnvLong("ZEN_X4", 25);

				bsp.Y1 = GetEnvLong("ZEN_Y1", 1);
				bsp.Y2 = GetEnvLong("ZEN_Y2", 7);
				bsp.Y3 = GetEnvLong("ZEN_Y3", 1);
				bsp.Y4 = GetEnvLong("ZEN_Y4", 0);
			}