//----------------------------------------------------------------------------
//
// File:        ZenNode.hpp
// Date:        26-Oct-1994
// Programmer:  Marc Rousseau
//
// Description: Definitions of structures used by the ZenNode routines
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
//   06-??-95	Added LineDef alias list to speed up the process
//
//----------------------------------------------------------------------------

#ifndef ZENNODE_HPP_
#define ZENNODE_HPP_

#if ! defined ( LEVEL_HPP_ )
  #include "level.hpp"
#endif

struct sBlockRemap{
    int block;
    int criteria;
};

struct sBlockMapOptions {
	bool  Rebuild;
	bool  Compress;
	bool  IdCompatible;
	bool  OffsetEight;
	bool  OffsetZero;
	bool  OffsetThirtySix;
	bool  OffsetHeuristic;
	bool  OffsetBruteForce;
	int   OffsetCommandLineX;
	int   OffsetCommandLineY;
	bool  OffsetUser;
	int   ZeroHeader;
	bool  SubBlockOptimization;
	int  GeometrySimplification;
	bool  BlockMerge;
	bool  RemoveNonCollidable;
	bool  HTMLOutput;
	bool  blockBig;
	bool autoDetectBacksideRemoval;
};

struct sNodeOptions {
	bool  Rebuild;
	int   Method;
	int   Thoroughness;
	bool  Quiet;
	bool  Unique;
	bool  ReduceLineDefs;
};


struct sBlockList {
	int     firstIndex;			// Index of 1st blockList element matching this one
	int     offset;
	int     count;
	int    *line;
	int     subBlockOffset;		// add this to the final address to speed up when using subBlockCompression
	int     uniqueLinedefs;
	int     sharedLinedefs;
	int     hash;                       // simple hash of the block, for comparisons
	int	lineDefBlocks; 		// The smallest amount of blocks one of the linedefs exist in, 1 or higher
	int     lineDefBlocksX;		// same, but X only
	int     lineDefBlocksY;		// y only
};

struct sBlockMap {
	int         xOrigin;
	int         yOrigin;
	int         noColumns;
	int         noRows;
	sBlockList *data;
};

typedef unsigned short BAM;
typedef long double REAL;		// Must have at least 50 significant bits

struct sVertex {
	double x;
	double y;
	double l;
};

struct SEG {
	wSegs           Data;
	const wLineDefInternal *LineDef;
	int             Sector;
	int             Side;
	int             AliasFlip;
	bool            Split;
	bool            DontSplit;
	bool            final;
	sVertex         start;
	sVertex         end;
};

struct sBSPOptions {
	int       algorithm;
	int 	thoroughness;
	bool      showProgress;
	bool      reduceLineDefs;		// global flag for invisible linedefs
	bool     *ignoreLineDef;		// linedefs that can be left out
	bool     *dontSplit;		// linedefs that can't be split
	bool     *keepUnique;		// unique sector requirements
};

struct sScoreInfo {
	int       index;
	long      metric1;
	long      metric2;
	int       invalid;
	int       total;
};

#define sgn(a)		((0<(a))-((a)<0))

#define BAM90		(( BAM ) 0x4000 )	// BAM:  90ψ ( «γ)
#define BAM180		(( BAM ) 0x8000 )	// BAM: 180ψ ( ργ)
#define BAM270		(( BAM ) 0xC000 )	// BAM: 270ψ (-«γ)
#define BAM360		         0x10000 // one bit more than a 16bit 0xFFFF BAM
#define M_PI        	3.14159265358979323846

#define SIDE_UNKNOWN		-2
#define SIDE_LEFT		-1
#define SIDE_SPLIT		 0
#define SIDE_RIGHT		 1

#define SIDE_FLIPPED		0xFFFFFFFE
#define SIDE_NORMAL		0

#define LEFT			0
#define SPLIT			1
#define RIGHT			2

#define IS_LEFT_RIGHT(s)	( s & 1 )
#define FLIP(c,s)		( c ^ s )

// ---- ZenReject structures to support RMB options ----

enum REJECT_OPTION_E {
	OPTION_UNKNOWN,
	OPTION_MAP_1,
	OPTION_MAP_2,
	OPTION_BAND,
	OPTION_BLIND,
	OPTION_BLOCK,
	OPTION_DISTANCE,
	OPTION_DOOR,
	OPTION_EXCLUDE,
	OPTION_GROUP,
	OPTION_INCLUDE,
	OPTION_LEFT,
	OPTION_LENGTH,
	OPTION_LINE,
	OPTION_NODOOR,
	OPTION_NOMAP,
	OPTION_NOPROCESS,
	OPTION_ONE,
	OPTION_PERFECT,
	OPTION_PREPROCESS,
	OPTION_PROCESS,
	OPTION_REPORT,
	OPTION_RIGHT,
	OPTION_SAFE,
	OPTION_TRACE
};

struct sRejectOptionRMB;
struct sOptionTableInfo;

typedef bool (*PARSE_FUNCTION) ( char *, const sOptionTableInfo &, sRejectOptionRMB * );

struct sOptionTableInfo {
	const char              *Name;
	const char              *Syntax;
	REJECT_OPTION_E          Type;
	PARSE_FUNCTION           ParseFunction;
};

struct sRejectOptionRMB {
	const sOptionTableInfo  *Info;
	bool                     Inverted;
	bool                     Banded;
	int                      Data [2];
	int                     *List [2];
};

struct sRejectOptions {
	bool                     Rebuild;
	bool                     Empty;
	bool                     Force;
	bool                     FindChildren;
	bool                     UseGraphs;
	bool                     UseRMB;
	const sRejectOptionRMB  *rmb;
};

struct sGeometryOptions {
	bool			Perform;
	int			Simplification;
};

struct sStatisticsOptions {
	bool			ShowVertices;
	bool			ShowLineDefs;
	bool			ShowSectors;
	bool			ShowThings;
	bool			ShowTotals;
};


bool ParseOptionRMB ( int, const char *, sRejectOptionRMB * );

// ----- C99 routines from <math.h> Required by ZenNode -----

#if ( defined ( __GNUC__ ) && ( __GNUC__ < 3 )) || defined ( __INTEL_COMPILER ) || defined ( __BORLANDC__ )

#if defined ( X86 )

__inline long lrint ( double flt )
{
	int intgr;

	__asm__ __volatile__ ("fistpl %0" : "=m" (intgr) : "t" (flt) : "st");

	return intgr;
}

#else
__inline long lrint ( double flt )
{
	//        return ( long ) ( flt + 0.5 * sgn ( flt ));
	return ( long ) ( flt + 0.5 );
}

#endif

#elif defined ( _MSC_VER )

__inline long lrint ( double flt )
{
	int intgr;

	_asm
	{
		fld   flt
			fistp intgr
	};

	return intgr;
}

#endif

struct sOptions {
	sBlockMapOptions BlockMap;
	sNodeOptions     Nodes;
	sRejectOptions   Reject;
	sGeometryOptions Geometry;
	sStatisticsOptions Statistics;
	bool             WriteWAD;
	bool             Extract;
	bool		 OutputWad;
};

struct sBlockMapExtraData;
struct sBlockMapOptions;
extern sBlockMap *GenerateBLOCKMAP ( DoomLevel *level, int, int, const sBlockMapOptions &options );
extern int  CreateBLOCKMAP ( DoomLevel *level, sBlockMapOptions &options );
extern void CreateNODES ( DoomLevel *level, sBSPOptions *options );
extern bool CreateREJECT ( DoomLevel *level, const sRejectOptions &options, const sBlockMapOptions &blockMapOptions );
extern void HTMLOutput(wBlockMap *map, sBlockMap *blockMap, sBlockList *blockList, const sBlockMapOptions &options, int blockSize, int savings, int totalSize);
#endif
