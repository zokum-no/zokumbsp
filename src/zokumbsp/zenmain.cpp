//----------------------------------------------------------------------------
//
// File:        ZenMain.cpp
// Date:        26-Oct-1994
// Programmer:  Marc Rousseau
//
// Description: The application specific code for ZenNode
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
//   06-??-95	Added Win32 support
//   07-19-95	Updated command line & screen logic
//   11-19-95	Updated command line again
//   12-06-95	Add config & customization file support
//   11-??-98	Added Linux support
//   01-31-04   Disabled unique sectors as a default option in the BSP options
//   01-31-04   Added RMB option support
//
//----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <fstream>
#include <cctype>
#include <memory>
#include <cstring>
#include <istream>

#ifndef _MSC_VER
#include <strings.h>
#endif

#if defined(_WIN32)
#include <io.h>
#define ISATTY _isatty
#define FILENO _fileno
#else
#include <unistd.h>
#define ISATTY isatty
#define FILENO fileno
#endif

#if defined ( _WIN32)
    #include <conio.h>
    #include <io.h>
#elif defined ( __LINUX__ )
    #include <unistd.h>
#endif

#include "common.hpp"
#include "logger.hpp"
#include "wad.hpp"
#include "level.hpp"
#include "console/console.hpp"
#include "bsp/zennode.hpp"
#include "blockmap/blockmap.hpp"
#include "geometry/preprocess.hpp"
#include "misc/endoom.hpp"
#include "console/help.hpp"

DBG_REGISTER ( __FILE__ );

#define ZENVERSION              "1.2.1"
#define ZOKVERSION		"1.1.2-alpha1"
#define ZOKVERSIONSHORT		"1.1.2"

const char ZOKBANNER []         = "ZokumBSP Version: " ZOKVERSION " (c) 2016-2026 Kim Roar Foldøy Hauge";
const char BANNER []            = "Based on: ZenNode Version " ZENVERSION " (c) 1994-2004 Marc Rousseau";
const char CONFIG_FILENAME []   = "zokumbsp.cfg";
const int  MAX_LEVELS           = 99;
const int  MAX_OPTIONS          = 256;
const int  MAX_WADS             = 32;
const int  VANILLA_LIMIT        = 32768;
const int  SOURCE_PORT_LIMIT    = 65535;

char HammingTable [ 256 ];

sOptions config;

struct sOptionsRMB {
    const char         *wadName;
    sRejectOptionRMB   option [MAX_OPTIONS];
};

sOptionsRMB rmbOptionTable [MAX_WADS];

#if defined ( __GNUC__ ) || defined ( __INTEL_COMPILER )
    extern char *strupr ( char * );
#endif

long long totalNodes = 0;
long long oldTotalNodes = 0;

long long totalSegs = 0;
long long oldTotalSegs = 0;

long long totalBlockmap = 0;
long long oldTotalBlockmap = 0;

long long totalLineDefs = 0;
long long oldTotalLineDefs = 0;

long long totalVertices = 0;
long long oldTotalVertices = 0;


/*
void printHelp (sOptions config) {
	FUNCTION_ENTRY ( NULL, "printHelp", true );

	fprintf ( stdout, "Usage: zokumbsp {-options} filename[.wad] [level{+level}] {-o|x output[.wad]}\n" );
	fprintf ( stdout, "\n" );
	fprintf ( stdout, " -x+ turn on option   -x- turn off option  %c = default\n", DEFAULT_CHAR );
	fprintf ( stdout, "\n" );
// Geometry	
	fprintf ( stdout, "Switches to change level geometry and optimize geometry.\n\n");
	
	fprintf ( stdout, "%c -g      Geometry pass, 1 suboption.\n\n", config.Geometry.Perform ? DEFAULT_CHAR : ' ' );

	fprintf ( stdout, "    s=    Simplify collision geometry.\n");
	fprintf ( stdout, "%c     0   No simplification.\n", (config.Geometry.Simplification == 0) ? DEFAULT_CHAR : ' ' );
	fprintf ( stdout, "%c     1   Only if same sector.\n", (config.Geometry.Simplification == 1) ? DEFAULT_CHAR : ' ' );
	fprintf ( stdout, "%c     2   Also 1-sided lines in different sectors.\n", (config.Geometry.Simplification == 2) ? DEFAULT_CHAR : ' ' );
// Blockmap	
	fprintf ( stdout, "\nSwitches to control BLOCKMAP strucure in a map.\n\n");

	fprintf ( stdout, "%c -b      Rebuild BLOCKMAP, 8 suboptions.\n", config.BlockMap.Rebuild ? DEFAULT_CHAR : ' ' );
	fprintf ( stdout, "%c   b     Build big 32bit BLOCKMAP, N/A.\n", config.BlockMap.blockBig ? DEFAULT_CHAR : ' ' );
	fprintf ( stdout, "%c   c     Compress BLOCKMAP.\n", config.BlockMap.Compress ? DEFAULT_CHAR : ' ' );
	fprintf ( stdout, "%c   h     Output BLOCKMAP data as HTML.\n", config.BlockMap.HTMLOutput ?  DEFAULT_CHAR : ' ' );
	fprintf ( stdout, "%c   i     Id-compatible BLOCKMAP. Sets 'o=1n=2g=0' and 's-r-'.\n\n", config.BlockMap.IdCompatible ? DEFAULT_CHAR : ' ' );
	fprintf ( stdout, "    o=    Offset configuration.\n");
	fprintf ( stdout, "%c     0   ZenNode 0,0 offset BLOCKMAP.\n", config.BlockMap.OffsetZero ? DEFAULT_CHAR : ' ' );
	fprintf ( stdout, "%c     1   DooMBSP / BSP 8,8 offset BLOCKMAP.\n", config.BlockMap.OffsetEight ? DEFAULT_CHAR : ' ' );
	fprintf ( stdout, "%c     2   Best of 36 offset combinations.\n", config.BlockMap.OffsetThirtySix ? DEFAULT_CHAR : ' ' );
	fprintf ( stdout, "%c     3   Heuristic method to reduce from 65536 offsets.\n", config.BlockMap.OffsetHeuristic ? DEFAULT_CHAR : ' ' );
	fprintf ( stdout, "%c     4   Best of all 65536 offset combinations.\n", config.BlockMap.OffsetBruteForce ? DEFAULT_CHAR : ' ' );
	fprintf ( stdout, "%c     x,y Specify specific offsets.\n\n", config.BlockMap.OffsetUser ? DEFAULT_CHAR : ' ' );

	fprintf ( stdout, "%c   r     Remove non-collidable lines from BLOCKMAP.\n\n", config.BlockMap.RemoveNonCollidable ? DEFAULT_CHAR : ' ' );
	fprintf ( stdout, "%c   s     Subset compress BLOCKMAP.\n\n", config.BlockMap.SubBlockOptimization ? DEFAULT_CHAR : ' ' );
	fprintf ( stdout, "    z=    Zero header configuration.\n");
	fprintf ( stdout, "%c     0   No zero header.\n", (config.BlockMap.ZeroHeader == 0) ? DEFAULT_CHAR : ' ' );
	fprintf ( stdout, "%c     1   Conventional zero header.\n", (config.BlockMap.ZeroHeader == 1) ? DEFAULT_CHAR : ' ' );
	fprintf ( stdout, "%c     2   Zero footer.\n", (config.BlockMap.ZeroHeader == 2) ? DEFAULT_CHAR : ' ' );
// Nodes
	fprintf ( stdout, "\nSwitches to control BSP tree and related structures in a map.\n\n");

	fprintf ( stdout, "%c -n      Rebuild NODES.\n\n", config.Nodes.Rebuild ? DEFAULT_CHAR : ' ' );
	
	fprintf ( stdout, "%c   b     Remove backside seg on on some linedefs.\n", config.BlockMap.autoDetectBacksideRemoval ? DEFAULT_CHAR : ' ' );
	fprintf ( stdout, "%c   c     Calculate BAM from SEGs instead of lineDefs.\n", ( config.Nodes.SegBAMs == 1 ) ? DEFAULT_CHAR : ' ' );
	fprintf ( stdout, "%c   i     Ignore non-visible lineDefs.\n", config.Nodes.ReduceLineDefs ? DEFAULT_CHAR : ' ' );
	fprintf ( stdout, "%c   q     Don't display progress bar.\n", config.Nodes.Quiet ? DEFAULT_CHAR : ' ' );
	fprintf ( stdout, "%c   u     Ensure all sub-sectors contain only 1 sector.\n", config.Nodes.Unique ? DEFAULT_CHAR : ' ' );
	fprintf ( stdout, "    1     Alias for a=s.\n" );
	fprintf ( stdout, "    2     Alias for a=d.\n" );
	fprintf ( stdout, "    3     Alias for a=f.\n\n" );

	fprintf ( stdout, "    a=    Partition Selection Algorithm.\n" );
	fprintf ( stdout, "%c     s   Minimize splits.\n", ( config.Nodes.Method == 1 ) ? DEFAULT_CHAR : ' ' );
	fprintf ( stdout, "%c     d   Minimize BSP depth.\n", ( config.Nodes.Method == 2 ) ? DEFAULT_CHAR : ' ' );
	fprintf ( stdout, "%c     f   Minimize time.\n", ( config.Nodes.Method == 3 ) ? DEFAULT_CHAR : ' ');
	fprintf ( stdout, "%c     a   Adaptive tree.\n", ( config.Nodes.Method == 4 ) ? DEFAULT_CHAR : ' ');
	fprintf ( stdout, "%c     m   Multi-tree. (Slow)\n", ( config.Nodes.Method == 5 ) ? DEFAULT_CHAR : ' ');
	fprintf ( stdout, "%c     v   Vertex-tree (N/A)\n\n", ( config.Nodes.Method == 6 ) ? DEFAULT_CHAR : ' ');

	fprintf ( stdout, "    m=    Metric, what kind of node tree do you favor.\n" );
	fprintf ( stdout, "%c     b   Favor 2 splits = 1 subsector.\n", ( config.Nodes.Metric == TREE_METRIC_BALANCED ) ? DEFAULT_CHAR : ' ' );
	fprintf ( stdout, "%c     s   Favor fewer SEG splits.\n", ( config.Nodes.Metric == TREE_METRIC_SEGS ) ? DEFAULT_CHAR : ' ' );
	fprintf ( stdout, "%c     u   Favor fewer subsectors.\n\n", ( config.Nodes.Metric == TREE_METRIC_SUBSECTORS ) ? DEFAULT_CHAR : ' ' );

	fprintf ( stdout, "    p=    Favor certain node selection picks for depth algorithm.\n");
	fprintf ( stdout, "%c     z   No favoring, use old algorithm for a balanced tree.\n", ( config.Nodes.SplitReduction == 0 ) ? DEFAULT_CHAR : ' ' );
	fprintf ( stdout, "%c     s   Favor nodes that do not split segs.\n", ( config.Nodes.SplitReduction == 1 ) ? DEFAULT_CHAR : ' ' );
	fprintf ( stdout, "%c     u   Favor nodes that do not create subsectors.\n", ( config.Nodes.SplitReduction == 2 ) ? DEFAULT_CHAR : ' ' );
	fprintf ( stdout, "%c     b   Favor both of the above equally.\n", (config.Nodes.SplitReduction == 3 ) ? DEFAULT_CHAR : ' ' );
	fprintf ( stdout, "%c     m   Try all of the above.\n\n", (config.Nodes.MultipleSplitMethods == 1 ) ? DEFAULT_CHAR : ' ' );

	fprintf ( stdout, "    t=    Tuning factor, varies from algorithm to algorithm.\n" );
	fprintf ( stdout, "      100 Default for adaptive tree.\n");

	fprintf ( stdout, "    w=    Number of sub trees to generate in multi-tree mode.\n" );
	fprintf ( stdout, "%c     2   Default width and also minimum.\n", ( config.Nodes.Width == 2 ) ? DEFAULT_CHAR : ' ');
	
	// Reject
	fprintf ( stdout, "\nSwitches to control REJECT resource in a map.\n\n");

	fprintf ( stdout, "%c -r      Rebuild REJECT resource.\n\n", config.Reject.Rebuild ? DEFAULT_CHAR : ' ' );
	fprintf ( stdout, "%c   z     Insert empty REJECT resource.\n", config.Reject.Empty  ? DEFAULT_CHAR : ' ' );
	fprintf ( stdout, "%c   f     Rebuild even if REJECT effects are detected.\n", config.Reject.Force ? DEFAULT_CHAR : ' ' );
	fprintf ( stdout, "%c   g     Use graphs to reduce LOS calculations.\n", config.Reject.UseGraphs ? DEFAULT_CHAR : ' ' );
	fprintf ( stdout, "%c   m{b}  Process RMB option file (.rej).\n", config.Reject.UseRMB ? DEFAULT_CHAR : ' ' );

	fprintf ( stdout, "\nSwitches to control other options.\n\n");

	fprintf ( stdout, "%c -c      Enable 16 color output.\n", config.Color ? DEFAULT_CHAR : ' ' );
	fprintf ( stdout, "%c -cc     Enable 24bit color output.\n", config.Color ? DEFAULT_CHAR : ' ' );
	fprintf ( stdout, "%c -t      Don't write output file (test mode).\n\n", ! config.WriteWAD ? DEFAULT_CHAR : ' ' );

	// Statistics
	fprintf ( stdout, "  -s=     Which output stats to show.\n");
	fprintf ( stdout, "%c   e     Sector count.\n", config.Statistics.ShowSectors ? DEFAULT_CHAR : ' ' );
	fprintf ( stdout, "%c   l     Linedef count.\n", config.Statistics.ShowLineDefs ? DEFAULT_CHAR : ' ' );
	fprintf ( stdout, "%c   p     Seg split count.\n", config.Statistics.ShowSplits ? DEFAULT_CHAR : ' ' );
	fprintf ( stdout, "%c   n     Nodes count.\n", config.Statistics.ShowNodes ? DEFAULT_CHAR : ' ' );
	fprintf ( stdout, "%c   s     Subsector count.\n", config.Statistics.ShowSubSectors ? DEFAULT_CHAR : ' ' );
	fprintf ( stdout, "%c   t     Thing count.\n", config.Statistics.ShowThings ? DEFAULT_CHAR : ' ' );
	fprintf ( stdout, "%c   v     Vertices count.\n", config.Statistics.ShowVertices ? DEFAULT_CHAR : ' ' );
	fprintf ( stdout, "    a     Show all of the above.\n" );
	fprintf ( stdout, "%c   m     Show a total for all computed levels.\n", config.Statistics.ShowTotals ? DEFAULT_CHAR : ' ' );
	fprintf ( stdout, "    x     Count percentage by %d limit of source ports.\n", SOURCE_PORT_LIMIT );
	fprintf ( stdout, "\n" );

	// Misc
	fprintf ( stdout, " level - ExMy for DOOM/Heretic or MAPxx for DOOM II/HEXEN.\n\n" );
	fprintf ( stdout, "Example: zokumbsp -b- -r- -sm -na=a file.wad map01+map03\n");
	fprintf ( stdout, "This rebuild nodes only, adaptive tree, give a total summary.\n");
	fprintf ( stdout, "It reads from file.wad, but only builds map01 and map03.\n");
}
*/

bool parseBLOCKMAPArgs ( char *&ptr, bool setting ) {
	FUNCTION_ENTRY ( NULL, "parseBLOCKMAPArgs", true );

	config.BlockMap.Rebuild = setting;
	while ( *ptr ) {
		int option = *ptr++;
		bool setting = true;
		if (( *ptr == '+' ) || ( *ptr == '-' )) {
			setting = ( *ptr++ == '+' ) ? true : false;
		}
		switch ( option ) {
			case 'C' : 
				config.BlockMap.Compress = setting;      
				break;
			case 'R' :
				config.BlockMap.RemoveNonCollidable = setting;
				break;
			case 'S' :
				config.BlockMap.SubBlockOptimization = setting;
				break;
			case 'I':
				config.BlockMap.IdCompatible = setting;
				break;
			case 'B':
				config.BlockMap.blockBig = setting;
				break;
			case 'M':
				config.BlockMap.BlockMerge = setting;
			case 'H':
				config.BlockMap.HTMLOutput = setting;
				break;
			case 'Z' : 
				if (ptr[0] && ptr[1]) {
					if (ptr[1] == '0') {
						config.BlockMap.ZeroHeader = 0;
					} else if (ptr[1] == '1') {
						config.BlockMap.ZeroHeader = 1;
					} else if (ptr[1] == '2') {
						config.BlockMap.ZeroHeader = 2;
					} else {
						printf("error");
						return true;
					}
					ptr += 2;
				}
				break;
			case 'O' : 
				if (ptr[0] && ptr[1] && ptr[2] && (ptr[2] == ',')) {
					// We're given offsets, not using inbuilt
					char *comma = strchr(ptr + 1, ',');

					if (comma) {
						comma[0] = '\0';
						if (strlen(ptr+1) && strlen(comma+1)) {
							config.BlockMap.OffsetCommandLineX = atoi(ptr+1);
							config.BlockMap.OffsetCommandLineY = atoi(comma + 1);
							config.BlockMap.OffsetUser = true;

							ptr += strlen(ptr+1) +  strlen(comma+1);

							return false;
						} else {
							printf("error (bad offsets)");	
							return true;
						}
					} else {
						printf("error (missing comma)");
						return true;
					}
				}
				if (ptr[0] && ptr[1]) {
					config.BlockMap.OffsetThirtySix = false;
					if (ptr[1] == '0') {
						config.BlockMap.OffsetZero = true;
					} else if (ptr[1] == '1') {
						config.BlockMap.OffsetEight = true;
					} else if (ptr[1] == '2') {
						config.BlockMap.OffsetThirtySix = true;
					} else if (ptr[1] == '3') {
						config.BlockMap.OffsetHeuristic = true;
					} else if (ptr[1] == '4') {
						config.BlockMap.OffsetBruteForce = true;			
					} else {
						printf("error");
						return true;
					}
					ptr += 2;
				}
				break;

			default  : return true;
		}
		config.BlockMap.Rebuild = true;
	}
	return false;
}

bool parseNODESArgs ( char *&ptr, bool setting ) {
	FUNCTION_ENTRY ( NULL, "parseNODESArgs", true );

	config.Nodes.Rebuild = setting;
	while ( *ptr ) {
		int option = *ptr++;
		bool setting = true;
		if (( *ptr == '+' ) || ( *ptr == '-' )) {
			setting = ( *ptr++ == '+' ) ? true : false;
		}
		switch ( option ) {
			case '1' : config.Nodes.Method = 1;                 		break;
			case '2' : config.Nodes.Method = 2;                 		break;
			case '3' : config.Nodes.Method = 3;                 		break;
			case 'C' : config.Nodes.SegBAMs = setting;			break;
			case 'Q' : config.Nodes.Quiet = setting;            		break;
			case 'U' : config.Nodes.Unique = setting;           		break;
			case 'I' : config.Nodes.ReduceLineDefs = setting;   		break;
			case 'B' : config.BlockMap.autoDetectBacksideRemoval = setting; break;
			case 'A' :
				   if (ptr[0] && ptr[1]) {
					   if (ptr[1] == 'S') {	// minimize splits
						   config.Nodes.Method = 1;
					   } else if (ptr[1] == 'D') { // minimize depths
						   config.Nodes.Method = 2;
					   } else if (ptr[1] == 'F') {
						   config.Nodes.Method = 3;
					   } else if (ptr[1] == 'A') {
                                                   config.Nodes.Method = 4;
					   } else if (ptr[1] == 'M') {
						   config.Nodes.Method = 5;
					   } else if (ptr[1] == 'V') {
						   config.Nodes.Method = 6;
					   } else {
						   printf("error");
						   return true;
					   }
					   ptr += 2;
				   } else {
					   printf("Bad node algorithm\n");
					   return true;
				   }
				   break;
			case 'M' :
				if (ptr[0] && ptr[1]) {
					if (ptr[1] == 'S') {
						config.Nodes.Metric = TREE_METRIC_SEGS;
					} else if (ptr[1] == 'U') {
						config.Nodes.Metric = TREE_METRIC_SUBSECTORS;
					} else if (ptr[1] == 'B') {
						config.Nodes.Metric = TREE_METRIC_BALANCED;
					} else {
						printf("Unsupported metric\n");
						return true;
					}
					ptr += 2;
				}
				break;
			case 'P' :
				if (ptr[0] && ptr[1]) {
					if (ptr[1] == 'Z') {
						config.Nodes.SplitReduction = 0x00;
					} else if (ptr[1] == 'S') {
						config.Nodes.SplitReduction = 0x01;
					} else if (ptr[1] == 'U') {
						config.Nodes.SplitReduction = 0x02;
					} else if (ptr[1] == 'B') {
						config.Nodes.SplitReduction = 0x03;
					}
					ptr += 2;
				}
				break;
			case 'S' :
				   if (ptr[0] && ptr[1]) {
					   if (ptr[1] == 'Z') {	// do not care if it causes slime trilas
						   config.Nodes.SplitHandling = 0;
					   } else if (ptr[1] == 'A') { // Avoid slime trails based on epsilon value
						   config.Nodes.SplitHandling = 1;
					   } else if (ptr[1] == 'R') { // rotate segs to try to fudge!
						   config.Nodes.SplitHandling = 2;
					   } else {
						   printf("Bad node seg split algorithm\n");
						   return true;
					   }
					   ptr += 2;

				   } else {
					   printf("Bad node seg split algorithm\n");
					   return true;
				   }
				   break;
			case 'T' :
				if (ptr[0] && ptr[1]) {
					config.Nodes.Tuning = atoi(ptr+1);
					if (config.Nodes.Tuning > 9) {
                                                ptr+=3;
                                        } else if (config.Nodes.Tuning > 99) {
                                                ptr+=4;
					} else if (config.Nodes.Tuning > 999) {
						ptr+=5;
                                        } else {
                                                ptr+=2;
                                        }
				}
				break;
			case 'W' :
				if (ptr[0] && ptr[1]) {
					config.Nodes.Width = atoi(ptr+1);
					if (config.Nodes.Width > 9) {
						ptr+=3;
					} else if (config.Nodes.Width > 99) {
						ptr+=4;
					} else {
						ptr+=2;
					}
				}
				break;
				
			default  : return true;
		}
		config.Nodes.Rebuild = true;
	}
	return false;
}

bool parseGEOMETRYArgs (char *&ptr, bool setting ) {
	FUNCTION_ENTRY ( NULL, "parseGeometryArgs", true );

	while ( *ptr ) {
		int option = *ptr++;
		switch ( option ) {
			case 'S' :
				if (ptr[0] && ptr[1]) {

					if (ptr[1] == '0') {
						config.BlockMap.GeometrySimplification = 0;
					} else if (ptr[1] == '1') {
						config.BlockMap.GeometrySimplification = 1;
					} else if (ptr[1] == '2') {
						config.BlockMap.GeometrySimplification = 2;
					} else {
						printf("error");
						return true;
					}
					if (config.BlockMap.GeometrySimplification) {
						config.Statistics.ShowLineDefs = true;
					}
					ptr += 2;
				}
				break;
			default  : return true;
		}
	}
	return false;
}

bool parseSTATISTICSArgs (char *&ptr, bool setting ) {
	FUNCTION_ENTRY ( NULL, "parseSTATISTICSArgs", true );

	while ( *ptr ) {
		int option = *ptr++;
		bool setting = true;
		if (( *ptr == '+' ) || ( *ptr == '-' )) {
			setting = ( *ptr++ == '+' ) ? true : false;
		}
		switch ( option ) {
			case 'A' : config.Statistics.ShowSectors = config.Statistics.ShowLineDefs = config.Statistics.ShowSplits = config.Statistics.ShowNodes = config.Statistics.ShowSubSectors = config.Statistics.ShowThings = config.Statistics.ShowVertices = setting;    break;
			case 'E' : config.Statistics.ShowSectors = setting;     break;
			case 'L' : config.Statistics.ShowLineDefs = setting;	break;
			case 'P' : config.Statistics.ShowSplits = setting;	break;
			case 'M' : config.Statistics.ShowTotals = setting;	break;
			case 'N' : config.Statistics.ShowNodes = setting;	break;
			case 'S' : config.Statistics.ShowSubSectors = setting;  break;
			case 'T' : config.Statistics.ShowThings = setting; 	break;
			case 'V' : config.Statistics.ShowVertices = setting;	break;
			case 'X' : config.Statistics.MaxSize = SOURCE_PORT_LIMIT;	break;
			default  : return true;
		}
	}
	return false;
}

bool parseREJECTArgs ( char *&ptr, bool setting )
{
	FUNCTION_ENTRY ( NULL, "parseREJECTArgs", true );

	config.Reject.Rebuild = setting;
	while ( *ptr ) {
		int option = *ptr++;
		bool setting = true;
		if (( *ptr == '+' ) || ( *ptr == '-' )) {
			setting = ( *ptr++ == '+' ) ? true : false;
		}
		switch ( option ) {
			case 'Z' : config.Reject.Empty = setting;           break;
			case 'F' : config.Reject.Force = setting;           break;
			case 'G' : config.Reject.UseGraphs = setting;       break;
			case 'M' : if (( ptr [-1] == 'M' ) && ( *ptr == 'B' )) {
					   ptr++;
					   if (( *ptr == '+' ) || ( *ptr == '-' )) {
						   setting = ( *ptr++ == '+' ) ? true : false;
					   }
				   }
				   config.Reject.UseRMB = setting;	
				   break;
			default  : return true;
		}
		config.Reject.Rebuild = true;
	}
	return false;
}

int parseArgs ( int index, const char *argv [] )
{
	FUNCTION_ENTRY ( NULL, "parseArgs", true );

	bool errors = false;
	while ( argv [ index ] ) {

		if ( argv [index][0] != '-' ) break;

		char *localCopy = _strdup ( argv [ index ]);
		char *ptr = localCopy + 1;
		// _strupr ( localCopy );
		_strupr_s(localCopy, strlen(localCopy) + 1);

		bool localError = false;
		while ( *ptr && ( localError == false )) {
			int option = *ptr++;
			bool setting = true;
			if (( *ptr == '+' ) || ( *ptr == '-' )) {
				setting = ( *ptr == '-' ) ? false : true;
				ptr++;
			}
			switch ( option ) {
				case 'B' : localError = parseBLOCKMAPArgs ( ptr, setting );     break;
				case 'G' : localError = parseGEOMETRYArgs (ptr, setting );	break;
				case 'N' : localError = parseNODESArgs ( ptr, setting );        break;
				case 'R' : localError = parseREJECTArgs ( ptr, setting );       break;
				case 'S' : localError = parseSTATISTICSArgs (ptr, setting);	break;
				case 'T' : config.WriteWAD = ! setting;                         break;
				case 'C' : 
					   if (config.Color) {
						   if (setting) {
							   config.Color = 2;	
						   } else {
							   config.Color = 0;
						   }
					   } else {
						   config.Color = setting;
					   }

					   break;
				case 'V' : exit(0);
				default  : localError = true;
			}
		}
		if ( localError ) {
			errors = true;
			int offset = ptr - localCopy - 1;
			size_t width = strlen ( ptr ) + 1;
			fprintf ( stderr, "Unrecognized parameter '%*.*s'\n", (int) width, (int)width, argv [index] + offset );
		}
		free ( localCopy );
		index++;
	}

	if (errors) {
		fprintf(stderr, "\n");
	}
	return index;
}

void ReadConfigFile(const char* argv[])
{
	FUNCTION_ENTRY(NULL, "ReadConfigFile", true);

	FILE* configFile = NULL;
	if (fopen_s(&configFile, CONFIG_FILENAME, "rt") != 0) {
		// Nothing for now
	}

	if (configFile == NULL) {
		char fileName[256];
		strcpy_s(fileName, sizeof(fileName), argv[0]);

		char* ptr = &fileName[strlen(fileName)];
		while ((--ptr >= fileName) && (*ptr != SEPERATOR));
		*++ptr = '\0';

		strcat_s(ptr, sizeof(fileName) - (ptr - fileName), CONFIG_FILENAME);

		if (fopen_s(&configFile, fileName, "rt") != 0) {
			// handle error
		}
	}

	if (configFile == NULL) return;

	char ch = (char)fgetc(configFile);
	bool errors = false;

	while (!feof(configFile)) {
		ungetc(ch, configFile);

		char lineBuffer[256];
		if (fgets(lineBuffer, sizeof(lineBuffer), configFile) == NULL) {
			fprintf(stderr, "Input error\n");
		}

		_strupr_s(lineBuffer, sizeof(lineBuffer));

		char* basePtr = lineBuffer;
		while (*basePtr == ' ') basePtr++;

		char* context = NULL;
		basePtr = strtok_s(basePtr, "\n\x1A", &context);

		if (basePtr) {
			char* ptr = basePtr;
			bool localError = false;
			while (*ptr && !localError) {
				int option = *ptr++;
				bool setting = true;
				if ((*ptr == '+') || (*ptr == '-')) {
					setting = (*ptr++ != '-');
				}
				switch (option) {
				case 'B': localError = parseBLOCKMAPArgs(ptr, setting); break;
				case 'N': localError = parseNODESArgs(ptr, setting);    break;
				case 'R': localError = parseREJECTArgs(ptr, setting);   break;
				case 'T': config.WriteWAD = !setting;                   break;
				case 'C': config.Color = setting;                       break;
				case 'V': exit(0);
				default:  localError = true;
				}
			}
			if (localError) {
				errors = true;
				int offset = basePtr - lineBuffer - 1;
				size_t width = strlen(basePtr) + 1;
				fprintf(stderr, "Unrecognized configuration option '%*.*s'\n",
					(int)width, (int)width, lineBuffer + offset);
			}
		}
		ch = (char)fgetc(configFile);
	}

	fclose(configFile);
	if (errors) fprintf(stderr, "\n");
}

int getLevels(int argIndex, const char* argv[], char names[][MAX_LUMP_NAME], wadList* list) {
	FUNCTION_ENTRY(NULL, "getLevels", true);

	int index = 0, errors = 0;

	char buffer[2048] = { 0 };

	if (argv[argIndex]) {
		strcpy_s(buffer, sizeof(buffer), argv[argIndex]);
		_strupr_s(buffer, sizeof(buffer));
	}

	char* context = NULL;
	char* ptr = strtok_s(buffer, "+", &context);

	// See if the user requested specific levels
	if (ptr && WAD::IsMap(ptr)) {
		argIndex++;
		while (ptr) {
			if (WAD::IsMap(ptr)) {
				if (list->FindWAD(ptr)) {
					strcpy_s(names[index++], MAX_LUMP_NAME, ptr);
				}
				else {
					fprintf(stderr, "  Could not find '%s'. Errors: %d\n", ptr, errors++);
				}
			}
			else {
				fprintf(stderr, "  %s is not a valid name for a level. Errors: %d\n", ptr, errors++);
			}
			ptr = strtok_s(NULL, "+", &context);
		}
	}
	else {
		int size = list->DirSize();
		const wadListDirEntry* dir = list->GetDir(0);
		for (int i = 0; i < size; i++) {
			if (dir->wad->IsMap(dir->entry->name)) {
				// Make sure it's really a level
				if (strcmp(dir[1].entry->name, "THINGS") == 0) {
					if (index == MAX_LEVELS) {
						fprintf(stderr, "ERROR: Too many levels in WAD - ignoring %s! Errors: %d\n",
							dir->entry->name, errors++);
					}
					else {
						memcpy_s(names[index++], MAX_LUMP_NAME, dir->entry->name, MAX_LUMP_NAME);
					}
				}
			}
			dir++;
		}
	}

	memset(names[index], 0, MAX_LUMP_NAME);

	if (errors) {
		if (errors) {
			fprintf(stderr, "\n");
		}
	}
	return argIndex;
}

bool ReadOptionsRMB ( const char *wadName, sOptionsRMB *options ) {
	FUNCTION_ENTRY ( NULL, "ReadOptionsRMB", true );

	char fileName [ 256 ];
	strcpy_s ( fileName, wadName );
	char *ptr = &fileName [ strlen ( fileName )];
	while ( *--ptr != '.' );
	*++ptr = '\0';

	size_t remainingSize = sizeof(fileName) - (ptr - fileName);
	strcat_s(ptr, remainingSize, "rej");

	while ((ptr > fileName) && (*ptr != SEPERATOR)) ptr--;
	if ((ptr < fileName) || (*ptr == SEPERATOR)) ptr++;

	FILE* optionFile = NULL;
	fopen_s(&optionFile, ptr, "rt");
	if (optionFile == NULL) {
		fopen_s(&optionFile, fileName, "rt");
		if (optionFile == NULL) return false;
	}

	memset ( options, 0, sizeof ( sOptionsRMB ));

	options->wadName = _strdup ( wadName );

	fprintf ( stdout, "Parsing RMB option file %s", fileName );

	int line = 0, index = 0;
	char buffer [512];

	while ( fgets ( buffer, sizeof ( buffer ) - 1, optionFile ) != NULL ) {
		if ( index >= MAX_OPTIONS ) {
			fprintf ( stderr, " - Too many RMB options\n" );
			break;
		}
		sRejectOptionRMB tempOption;
		if ( ParseOptionRMB ( ++line, buffer, &tempOption ) == true ) {
			options->option [index] = tempOption;
			index++;
		}
	}

	fclose ( optionFile );

	if ( index != 0 ) {
		fprintf ( stdout, " - %d valid RMB options detected\n", index );
		return true;
	}

	return false;
}

void EnsureExtension(std::string& fileName, const std::string& ext) {
	FUNCTION_ENTRY(NULL, "EnsureExtension", true);

	// If the file exists already, don't modify it
	FILE* file = nullptr;
#ifdef _WIN32
	fopen_s(&file, fileName.c_str(), "rb");
#else
	file = fopen(fileName.c_str(), "rb");
#endif
	if (file != nullptr) {
		fclose(file);
		return;
	}

	// Case-insensitive compare for existing extension
#ifdef _WIN32
	auto icompare = [](const std::string& a, const std::string& b) {
		return _stricmp(a.c_str(), b.c_str()) == 0;
		};
#else
	auto icompare = [](const std::string& a, const std::string& b) {
		return strcasecmp(a.c_str(), b.c_str()) == 0;
		};
#endif

	if (fileName.size() < ext.size() ||
		!icompare(fileName.substr(fileName.size() - ext.size()), ext)) {
		fileName += ext;
	}
}

const char *TypeName ( eWadType type )
{
	FUNCTION_ENTRY ( NULL, "TypeName", true );

	const char *name = NULL;
	switch ( type ) {
		case wt_DOOM    : name = "DOOM";        break;
		case wt_DOOM2   : name = "DOOM2";       break;
		case wt_HERETIC : name = "Heretic";     break;
		case wt_HEXEN   : name = "Hexen";       break;
		case wt_STRIFE	: name = "Strife";		break;
		case wt_HACX	: name = "Hacx";        break;	
		case wt_CHEXQUEST	: name = "Chex Quest"; break;
		default         : name = "<Unknown>";   break;
	}
	return name;
}

// Assumes wadList, WAD, ws_* enums, config, etc. are defined elsewhere.
std::unique_ptr<wadList> getInputFiles(const std::string& cmdLine, std::string& outWadList) {
	FUNCTION_ENTRY(nullptr, "getInputFiles", true);

	outWadList.clear();
	auto myList = std::make_unique<wadList>();

	if (cmdLine.empty()) return myList;

	std::stringstream ss(cmdLine);
	std::string token;
	int errors = 0;
	int index = 0;
	bool wadOk = true;

	while (std::getline(ss, token, '+')) {
		if (token.empty()) continue;

		std::string wadName = token;
		EnsureExtension(wadName, ".wad");

		auto wad = std::make_unique<WAD>(wadName.c_str());
		if (wad->Status() != ws_OK) {
			wadOk = false;
			switch (wad->Status()) {
			case ws_INVALID_FILE:
				std::fprintf(stderr, "The file %s does not exist\n", wadName.c_str());
				break;
			case ws_CANT_READ:
				std::fprintf(stderr, "Can't open the file %s for read access\n", wadName.c_str());
				break;
			case ws_INVALID_WAD:
				std::fprintf(stderr, "%s is not a valid WAD file\n", wadName.c_str());
				break;
			default:
				std::fprintf(stderr, "** Unexpected Error opening %s **\n", wadName.c_str());
				break;
			}
			continue;
		}

		if (!myList->IsEmpty()) {
			std::cout << "Merging: " << wadName << " with " << outWadList << "\n";
			outWadList.push_back('+');
		}

		if (!myList->Add(std::move(wad))) {
			errors++;
			if (myList->Type() != wt_UNKNOWN) {
				std::cerr << "ERROR: " << wadName << " is not a " << TypeName(myList->Type()) << " PWAD.\n";
			}
			else {
				std::cerr << "ERROR: " << wadName << " is not the same type.\n";
			}
		}
		else {
			if (config.Reject.UseRMB && index < MAX_WADS) {
				if (ReadOptionsRMB(wadName.c_str(), &rmbOptionTable[index])) {
					config.Reject.rmb = rmbOptionTable[index].option;
					index++;
				}
			}
			else if (index == MAX_WADS) {
				std::cerr << "WARNING: Too many wads specified - RMB options ignored\n";
				index++;
			}

			auto pos = wadName.find_last_of(SEPERATOR);
			std::string baseName = (pos != std::string::npos) ? wadName.substr(pos + 1) : wadName;
			outWadList += baseName;
		}
	}

	if (wadOk && !outWadList.empty() && outWadList.back() == '+') {
		outWadList.pop_back();
	}

	if (myList->wadCount() > 1) std::cout << "\n";
	if (errors) std::cerr << "\n";
	if (index != 0) std::cout << "\n";

	return myList;
}

void ReadSection(std::istream& file, int max, std::vector<bool>& array) {
	array.assign(max, false); // Initialize all entries to false

	std::string line;
	while (std::getline(file, line)) {
		// Trim newline, carriage return, or Ctrl-Z characters
		line.erase(std::remove_if(line.begin(), line.end(),
			[](char c) { return c == '\n' || c == '\r' || c == '\x1A'; }),
			line.end());

		// Skip leading whitespace
		auto start = line.begin();
		while (start != line.end() && std::isspace(static_cast<unsigned char>(*start))) ++start;
		if (start == line.end() || *start == '[') {
			// End of section or empty line
			break;
		}

		bool value = true;
		if (*start == '!') {
			value = false;
			++start;
			while (start != line.end() && std::isspace(static_cast<unsigned char>(*start))) ++start;
		}

		std::string tokenStr(start, line.end());
		std::istringstream tokenStream(tokenStr);
		std::string token;

		while (std::getline(tokenStream, token, ',')) {
			// Trim leading and trailing whitespace
			token.erase(token.begin(),
				std::find_if(token.begin(), token.end(), [](unsigned char c) { return !std::isspace(c); }));
			token.erase(std::find_if(token.rbegin(), token.rend(), [](unsigned char c) { return !std::isspace(c); }).base(),
				token.end());

			if (token.empty()) continue;

			if (token == "all" || token == "ALL") {
				std::fill(array.begin(), array.end(), value);
				continue;
			}

			int low = -1, high = -1;
			size_t dashPos = token.find('-');
			try {
				if (dashPos != std::string::npos) {
					low = std::stoi(token.substr(0, dashPos));
					high = std::stoi(token.substr(dashPos + 1));
				}
				else {
					low = high = std::stoi(token);
				}
			}
			catch (...) {
				// Invalid token, skip
				continue;
			}

			// Bounds check
			if (low < 0) low = 0;
			if (high >= max) high = max - 1;

			for (int i = low; i <= high; ++i) {
				array[i] = value;
			}
		}
	}
}

void ReadCustomFile(DoomLevel* curLevel, wadList* myList, sBSPOptions* options) {
	FUNCTION_ENTRY(nullptr, "ReadCustomFile", true);

	// Get WAD filename
	const wadListDirEntry* dir = myList->FindWAD(curLevel->Name(), nullptr, nullptr);
	if (!dir) { 
		return; 
	}

	std::string fileName = dir->wad->Name();
	auto dotPos = fileName.rfind('.');
	if (dotPos != std::string::npos) {
		fileName.replace(dotPos + 1, fileName.size() - dotPos - 1, "zen");
	}

	auto sepPos = fileName.find_last_of(SEPERATOR);
	std::string optionFileName = (sepPos != std::string::npos) ? fileName.substr(sepPos + 1) : fileName;

	std::ifstream optionFile(optionFileName);
	if (!optionFile.is_open()) {
		optionFile.open(fileName);
		if (!optionFile.is_open()) return;
	}

	std::string line;
	bool foundMap = false;

	while (std::getline(optionFile, line)) {
		// Trim whitespace and special characters
		line.erase(std::remove_if(line.begin(), line.end(),
			[](char c) { return c == '\n' || c == '\r' || c == '\x1A' || c == ']'; }),
			line.end());
		line.erase(line.begin(),
			std::find_if(line.begin(), line.end(), [](unsigned char c) { return !std::isspace(c); }));

		if (WAD::IsMap(line)) {
			if (line == curLevel->Name()) {
				foundMap = true;
			}
			else if (foundMap) {
				break; // done with this map
			}
			continue;
		}

		if (!foundMap) continue;

		int maxIndex = 0;
		bool isSectorSplit = false;
		std::vector<bool>* arrayPtr = nullptr;
		std::vector<bool> tempArray;

		if (_stricmp(line.c_str(), "ignore-linedefs") == 0) {
			maxIndex = curLevel->LineDefCount();
			if (!options->ignoreLineDef) options->ignoreLineDef = new bool[maxIndex]();
			arrayPtr = reinterpret_cast<std::vector<bool>*>(options->ignoreLineDef); // For ReadSection
		}
		else if (_stricmp(line.c_str(), "dont-split-linedefs") == 0) {
			maxIndex = curLevel->LineDefCount();
			if (!options->dontSplit) options->dontSplit = new bool[maxIndex]();
			arrayPtr = reinterpret_cast<std::vector<bool>*>(options->dontSplit);
		}
		else if (_stricmp(line.c_str(), "dont-split-sectors") == 0) {
			isSectorSplit = true;
			maxIndex = curLevel->SectorCount();
			tempArray.assign(maxIndex, false);
			arrayPtr = &tempArray;
		}
		else if (_stricmp(line.c_str(), "unique-sectors") == 0) {
			maxIndex = curLevel->SectorCount();
			if (!options->keepUnique) options->keepUnique = new bool[maxIndex]();
			arrayPtr = reinterpret_cast<std::vector<bool>*>(options->keepUnique);
		}

		if (arrayPtr) {
			ReadSection(optionFile, maxIndex, *arrayPtr);

			if (isSectorSplit) {
				const wLineDefInternal* lineDef = curLevel->GetLineDefs();
				const wSideDef* sideDef = curLevel->GetSideDefs();
				for (int i = 0; i < curLevel->LineDefCount(); i++, lineDef++) {
					for (int s = 0; s < 2; s++) {
						int side = lineDef->sideDef[s];
						if (side != NO_SIDEDEF && tempArray[sideDef[side].sector]) {
							options->dontSplit[i] = true;
						}
					}
				}
			}
		}
	}
}

int CheckREJECT ( DoomLevel *curLevel ) {
	FUNCTION_ENTRY ( NULL, "CheckREJECT", true );

	static bool initialized = false;
	if ( ! initialized ) {
		initialized = true;
		for ( int i = 0; i < 256; i++ ) {
			int val = i, count = 0;
			for ( int j = 0; j < 8; j++ ) {
				if ( val & 1 ) count++;
				val >>= 1;
			}
			HammingTable [i] = ( char ) count;
		}
	}

	int size = curLevel->RejectSize ();
	int noSectors = curLevel->SectorCount ();
	int mask = ( 0xFF00 >> ( size * 8 - noSectors * noSectors )) & 0xFF;
	int count = 0;
	if ( curLevel->GetReject () != 0 ) {
		UINT8 *ptr = ( UINT8 * ) curLevel->GetReject ();
		while ( size-- ) count += HammingTable [ *ptr++ ];
		count -= HammingTable [ ptr [-1] & mask ];
	}

	return ( int ) ( 10000.0 * count / ( noSectors * noSectors ) + 0.5 );
}
void PrintTime ( std::uint32_t time ) {
	FUNCTION_ENTRY ( NULL, "PrintTime", false );

	GotoXY ( 63, startY );
	
	long d = (time / 1000) / (3600 * 24);
	time -= d * (24 * 3600 * 1000);

	long h = (time / 1000) / 3600;
	time -= h * (3600 * 1000);

	long m = (time / 1000) / 60;
	long s = (time / 1000) % 60;
	long ms = time % 1000;

	if (d > 0) {
		printf ( "%3ldd %2ldh %02ldm %02lds\r\n", d, h, m, s);
	} else if (h > 9) {
		printf ( "%7ldh %02ldm %02lds\r\n", h, m, s);
	} else if (h > 0) {
		printf ( "%1ldh %02ldm %02lds %03ldms\r\n", h, m, s, ms);
	} else if (m > 0) {
		printf ( "   %2ldm %02lds %03ldms\r\n", m, s, ms);
	} else if (s > 0) {
		printf ( "       %2lds %03ldms\r\n", s, ms);
	} else {
		printf ( "           %3ldms\r\n", ms);
	}
	
}

void PrintColorOff(void) {
	printf ( "%c[0m", 27);
}

void PrintMapHeader(char *map, bool highlight) {
	GotoXY ( 0, startY );

	char output[2048] = "";
	char add[2048];

	int color, color2, bcolor, bright;

	if (highlight) {
		color = 32;
		color2 = 37;
		bcolor = 44;
		bright = 1;
	} else {
		color = 37;
		color2 = 37;
		bcolor = 44;
		bright = 44;
	}

	if (config.Color == 1)  {
		sprintf_s (add, "%c[%d;%d;%dm", 27, color, bcolor, bright );
		strcat_s(output, add);
	} else if (config.Color == 2) {
		sprintf_s(add, "\033[48;2;%d;%d;%dm", 4,20,64);
		strcat_s(output, add);
	}
	sprintf_s(add, "%-*.*s", MAX_LUMP_NAME, MAX_LUMP_NAME, map );
	strcat_s(output, add);

	if (config.Color == 1) {
		sprintf_s (add, "%c[0;%d;%d;%dm", 27, color2, bcolor, bright);
		strcat_s(output, add);

	} else if (config.Color == 2) {
		sprintf_s (add, "\033[48;2;%d;%d;%dm", 4,20,64);
		strcat_s(output, add);

	}

	sprintf_s(add, " Entry         Old         New     %%%%Change      %%%%Limit     Time Elapsed\r\n");
	strcat_s(output, add);
	
	printf (output);

	if (config.Color) {
		PrintColorOff();
	}

	GetXY ( &startX, &startY );
}

int oldHashes = 0;
double oldProgress[2] = {-1.0, -1.0};

char *oldLump;

std::uint32_t oldTime[2];
int lastRow = 0;

void ProgressBar(char *lump, double progress, int width, int row) {

	char output[2048]; // we write to this and then flush it to screen
	char add[2048];

	char prog[256];

	double counter = 0;
	double divisor = 0;
	double total = 0;

	if (oldProgress[row]  > progress) {
		oldProgress[row] = -1.0;
	}
		// we only update 4 times per second
		std::uint32_t now = CurrentTime ();
	
		if (now < (oldTime[row] + 200)) {
			return;
		}
		oldTime[row] = now;

		if (progress < (oldProgress[row] +  0.0001)) {
			return;
		} 
		oldProgress[row] = progress;

	if (progress > 1.0) {
		progress = 1.0;
	}

	int hashes = lrint(progress * (double) width);

// Ok we will have to write something to screen!

	if (row) {
		for (int i = 0; i != row; i++) {
			printf("\n");
		}
	}


	GotoXY (0, startY );
	
	sprintf_s(output, "         %-s", lump);

	if (progress < 1.0) {
		// printf(" %5.2f%% ", 100.0 * progress);
		sprintf_s(add, "%6.2f%%%% ", 100.0 * progress);
	} else {
		// printf(" %5.1f%% ", 100.0 * progress);
		sprintf_s(add, "%6.1f%%%% ", 100.0 * progress);
	}

	// pile on the output into a write buffer
	strcat_s(output, add);
	
	sprintf_s(prog, "");

	if (config.Color) {
		//printf ( "%c[0;37m", 27);
		sprintf_s(add, "%c[0;37m", 27);
		strcat_s(output, add);
	}
	// printf("[");
	sprintf_s(add, "[");
	strcat_s(output, add);

	if (config.Color) {
		// printf ( "%c[1;30m", 27);
		sprintf_s(add, "%c[1;30m", 27);
		strcat_s(output, add);
	}

	if (config.Color == 2) {

		int r = 56;
		int g = 16;
		int b = 224;

		double stepsize = 1.0 / width ; 

		for (int i = 0; i != hashes; i++) {
			r = 32 + (int) (stepsize * i * 300);
			g = 8 + (int) (stepsize * i * 75);
			b = 270 - (int) (stepsize * i * 240);

			if (i > ((width - 1) / 2)) {
				g += (int) (stepsize * ((i - (width / 2)) * 115));
				r += (int) (stepsize * ((i - (width / 2)) * 45));
			}


			if (r > 255 ) { 
				r = 255;
				g += (int) (stepsize * ((i - (width / 2)) * 20));
			}
			if (g > 255 ) {
				g = 255;
			}
			if (b > 255 ) {
				b = 255;
			}

			if (b < 0) {
				b = 0;
			}
			if (g < 0) {
				g = 0;
			}
			if (r < 0) {
				r = 0;
			}

			// printf ("\033[38;2;%d;%d;%dm#", r, g, b);
			sprintf_s(add, "\033[38;2;%d;%d;%dm#", r, g, b);
			strcat_s(output, add);
		}
		if (hashes != width) {
			// printf("%*s", width - hashes, (char *) " ");
			sprintf_s(add, "%*s", width - hashes, (char *) " ");
			strcat_s(output, add);
		}

	} else {
		sprintf_s(prog, "%*s", width, (char *)" ");

		for (int i = 0; i != hashes; i++) {
		        prog[i] = '#';
		}
		// printf ( "%s", prog);
		strcat_s(output, prog);
	}

	if (config.Color) {
		// printf ( "%c[0;37m", 27);
		sprintf_s (add, "%c[0;37m", 27);
		strcat_s(output, add);
	}

	// printf("]");
	sprintf_s(add, "]");
	strcat_s(output, add);

	printf(output);

	if (config.Color) {
		PrintColorOff();
	}

	if (row) {
		MoveUp ( row );
		GetXY ( &startX, &startY );
	}

}

void PrintMapLump(char *lump, int old, int nu, int limit, double oldD, double nuD) {
	GotoXY ( 8, startY );

	oldTime[0] = 0;
	oldTime[1] = 0;

	if (nu != -1) {
		printf ( " %-8s  %7d  => %7d", lump, old, nu );
	} else {
		printf ( " %-8s  %6.2f%%  => %6.2f%%", lump, oldD, nuD );
	}

	double dNew = nu;
	double dOld = old;
	double dLimit = limit;

	double change, efficiency;

	if (nu && limit) {
		efficiency = dNew / dLimit;
	} else {
		efficiency = 1.0;
	}

	char ansicolor[8];

	if ((oldD > 0) && (nuD > 0)) {
		change = oldD / nuD;
		nu = 1;
		old = 1;
	} else if (old && nu) {
		change = (double) nu / (double) old;
	} else {
		change = 1.0;
	}

	if(config.Color) {
		if(change >= 1.25) {
			sprintf_s(ansicolor, "1;31");
		} else if (change >= 1.1) {
			sprintf_s(ansicolor, "0;33");
		} else if (change > 1.0) {
			sprintf_s(ansicolor, "1;33");
		} else if ((nuD == dOld) || (old == nu)) {
			sprintf_s(ansicolor, "0;37");
		} else if (change >= 0.9) {
			sprintf_s(ansicolor, "0;32");
		} else {
			sprintf_s(ansicolor, "1;32");
		}
		printf ("%c[%sm", 27, ansicolor);

	}
	if (old || (oldD > 0.0)) {
		printf("     %6.2f%%", change * 100.0);
	} else {
		printf("           -");
	}

	if(config.Color) {
		if (limit == 0) {
			sprintf_s(ansicolor, "0;37");
		} else if (	(efficiency) > 1.0) {
			sprintf_s(ansicolor, "1;37;41");
		} else if (efficiency >= 0.95) {
			sprintf_s(ansicolor, "0;31");
		} else if ( efficiency >= 0.90) {
			sprintf_s(ansicolor, "1;31");
		} else if ( efficiency >= 0.85) {
			sprintf_s(ansicolor, "0;33");
		} else if ( efficiency >= 0.80) {
			sprintf_s(ansicolor, "1;33");
		} else {
			sprintf_s(ansicolor, "0;37");
		}
		printf ("%c[%sm", 27, ansicolor);
	}

	if (limit) {
		printf("     %6.2f%%", efficiency * 100.0);	
	} else {
		printf("           -");
	}

	if (config.Color) {
		PrintColorOff();
	}
}

bool ProcessLevel ( char *name, wadList *myList, std::uint32_t *elapsed ) {
	FUNCTION_ENTRY ( NULL, "ProcessLevel", true );

	std::uint32_t dummyX = 0;

	*elapsed = 0;
	GetXY ( &startX, &startY );
	PrintMapHeader(name, false );

	const wadListDirEntry *dir = myList->FindWAD ( name );
	DoomLevel *curLevel = new DoomLevel ( name, dir->wad );
	if ( curLevel->IsValid ( false ) == false ) {
		printf ( "This level is not valid... " );
		printf ( "\r\n" );
		delete curLevel;
		return false;
	}

	int rows = 1;

	startX = 4;

	if ( config.BlockMap.Rebuild || config.Nodes.Rebuild || config.Reject.Rebuild ) {
		std::uint32_t preTime = CurrentTime ();

		int oldSectorCount = curLevel->SectorCount ();
		int oldLineCount = curLevel->LineDefCount();
		bool showTime = false;

		MapExtraData(curLevel, &config);
		*elapsed += preTime = CurrentTime () - preTime;

		if (config.Statistics.ShowLineDefs) {
			PrintMapLump((char * )"Linedefs", oldLineCount, curLevel->LineDefCount (), config.Statistics.MaxSize - 1, 0, 0);

			PrintTime (preTime);
			showTime = true;
			// printf ( "\r\n" );
			rows++;
		}
		if (config.Statistics.ShowSectors) {
			PrintMapLump((char * )"Sectors", oldSectorCount, curLevel->SectorCount (), config.Statistics.MaxSize - 1, 0, 0);		

			if (showTime == false) {
				PrintTime (preTime);
			}
			printf ( "\r\n" );
			rows++;
		}
		GetXY ( &dummyX, &startY );
	}

	if ( config.BlockMap.Rebuild ) {

		int oldSize = curLevel->BlockMapSize ();
		std::uint32_t blockTime = CurrentTime ();
		int saved = CreateBLOCKMAP ( curLevel, config.BlockMap );
		*elapsed += blockTime = CurrentTime () - blockTime;
		int newSize = curLevel->BlockMapSize ();

		Status ( (char *) "" );
		GotoXY(0, startY);

		if ( saved >= 0 ) {
			PrintMapLump((char * )"Blockmap", oldSize, newSize, 65536, 0, 0);
		} else {
			printf ( "       Blockmap   Failed to create a valid lump\n" );
		}

		PrintTime ( blockTime );

		GetXY ( &dummyX, &startY );
		rows++;
	}

	if ( config.Nodes.Rebuild ) {
		int oldNodeCount = curLevel->NodeCount ();
		int oldSegCount  = curLevel->SegCount ();
		int oldSubSectorCount = curLevel->SubSectorCount ();
		int oldSplits = 0;
		int segCount = curLevel->SegCount();
		int oldVertexCount = curLevel->VertexCount();
	
		const wSegs *segs = curLevel->GetSegs ();

 		for (int i = 0; i != segCount; i++) {
			if (segs[i].offset) {
				oldSplits++;
			}
		}

		bool *keep = new bool [ curLevel->SectorCount ()];
		memset ( keep, config.Nodes.Unique, sizeof ( bool ) * curLevel->SectorCount ());

		sBSPOptions options;
		options.algorithm      = config.Nodes.Method;
		options.thoroughness   = config.Nodes.Thoroughness;
		options.showProgress   = ! config.Nodes.Quiet;
		options.segBAMs 	= config.Nodes.SegBAMs;
		options.reduceLineDefs = config.Nodes.ReduceLineDefs;
		options.ignoreLineDef  = NULL;
		options.dontSplit      = NULL;
		options.keepUnique     = keep;
		options.SplitHandling = config.Nodes.SplitHandling;
		options.SplitReduction = config.Nodes.SplitReduction;
		options.MultipleSplitMethods = config.Nodes.MultipleSplitMethods;
		options.Cache =		config.Nodes.Cache;
		options.Metric = 	config.Nodes.Metric;
		options.Width =		config.Nodes.Width;
		options.Tuning = 	config.Nodes.Tuning;
		options.vertexPartition = config.Nodes.vertexPartition;

		ReadCustomFile ( curLevel, myList, &options );

		std::uint32_t nodeTime = CurrentTime ();
		CreateNodesStructure ( curLevel, &options );
		*elapsed += nodeTime = CurrentTime () - nodeTime;

		if ( options.ignoreLineDef ) delete [] options.ignoreLineDef;
		if ( options.dontSplit ) delete [] options.dontSplit;
		if ( options.keepUnique ) delete [] options.keepUnique;

		Status ( (char *) "" );
		GotoXY ( startX, startY );

		totalNodes +=  curLevel->NodeCount ();
		oldTotalNodes += oldNodeCount;
		totalSegs += curLevel->SegCount ();
		oldTotalSegs += oldSegCount;

		segCount = curLevel->SegCount();

		PrintMapLump( (char *)  "Segs", oldSegCount, segCount, config.Statistics.MaxSize, 0, 0);
		printf ( "\r\n" );
		rows++;

		if (config.Statistics.ShowSplits) {
			int splits = 0;
			segs = curLevel->GetSegs ();

			for (int i = 0; i != segCount; i++) {
				if (segs[i].offset) {
					splits++;
				}
			}

			PrintMapLump( (char *)  "Splits",  oldSplits, splits, 0, 0, 0);
			printf ( "\r\n" );
			rows++;
		}
		if (config.Statistics.ShowVertices) {
			PrintMapLump((char * )"Vertices", oldVertexCount, curLevel->VertexCount (), config.Statistics.MaxSize - 1, 0, 0);
			printf ( "\r\n" );
			rows++;
		}
		PrintMapLump( (char *)  "Subsecs", oldSubSectorCount, curLevel->SubSectorCount (), config.Statistics.MaxSize, 0, 0);
		GetXY ( &dummyX, &startY );
		PrintTime ( nodeTime );

		rows++;
	}

	if ( config.Reject.Rebuild ) {

		int oldEfficiency = CheckREJECT ( curLevel );

		std::uint32_t rejectTime = CurrentTime ();
		bool special = CreateREJECT ( curLevel, config.Reject/*, config.BlockMap*/);
		*elapsed += rejectTime = CurrentTime () - rejectTime;

		int newEfficiency = CheckREJECT ( curLevel );

		if ( special == false ) {
			Status ( (char *) "" );
			GotoXY ( startX, startY );
			PrintMapLump( (char *)  "Reject", 0, 0, 0, oldEfficiency / 100.0, newEfficiency / 100.0);
			PrintTime ( rejectTime );
		} else {
			printf ( "         Reject special effects detected, use -rf to force an update." );
			printf ( "\r\n" );
		}
		rows++;
	}

	bool changed = false;
	if ( rows != 0 ) {
		Status ( (char *) "Updating Level ... " );
		changed = curLevel->UpdateWAD ();
		Status ( (char *) "" );
		if ( changed ) {
			GetXY ( &dummyX, &startY );
			MoveUp ( rows );

			if (config.Color == 1) {
				PrintMapHeader(name, true);
				
			} else if (config.Color == 2) {
				printf ("\033[48;2;%d;%d;%dm", 4,20,64);
			}
			if (config.Color == 0) {
				GotoXY(7, startY);
				printf("*");
			}
			MoveDown ( rows );
			if (config.Color) {
				PrintColorOff();
			}
		}
	} else {
		printf ( "Nothing to do here ... " );
		printf ( "\r\n" );
	}

	printf ( "\r\n" );

	int noSectors = curLevel->SectorCount ();
	int rejectSize = (( noSectors * noSectors ) + 7 ) / 8;
	if (( curLevel->RejectSize () != rejectSize ) && ( config.Reject.Rebuild == false )) {
		fprintf ( stderr, "WARNING: The REJECT structure for %s is the wrong size - try using -r\n", name );
	}
	delete curLevel;

	return changed;
}

void PrintStats ( int totalLevels, std::uint32_t totalTime, int totalUpdates ) {
	FUNCTION_ENTRY ( NULL, "PrintStats", true );

	if ( totalLevels != 0 ) {

		printf ( "%d Level%s processed in ", totalLevels, totalLevels > 1 ? "s" : "" );
		if ( totalTime > 60000 ) {
			std::uint32_t minutes = totalTime / 60000;
			std::uint32_t tempTime = totalTime - minutes * 60000;
			printf ( "%ld minute%s %ld.%03ld second%s - ", minutes, minutes > 1 ? "s" : "", tempTime / 1000, tempTime % 1000, ( tempTime == 1000 ) ? "" : "s" );
		} else {
			printf ( "%ld.%03ld second%s - ", totalTime / 1000, totalTime % 1000, ( totalTime == 1000 ) ? "" : "s"  );
		}

		if ( totalUpdates ) {
			printf ( "%d Level%s need%s updating.\r\n", totalUpdates, totalUpdates > 1 ? "s" : "", config.WriteWAD ? "ed" : "" );
		} else {
			printf ( "No Levels need%s updating.\r\n", config.WriteWAD ? "ed" : "" );
		}

		if ( totalTime == 0 ) {
			printf ( "WOW! Whole bunches of levels/sec!\r\n" );
		} else if ( totalTime < 1000 ) {
			printf ( "%f levels/sec\r\n", 1000.0 * totalLevels / totalTime );
		} else if ( totalLevels > 1 ) {
			printf ( "%f secs/level\r\n", totalTime / ( totalLevels * 1000.0 ));
		}
	}
}

void printTotals(void) {
	PrintMapHeader((char *) "Total:", false);

	GotoXY ( startX, startY );

	PrintMapLump( (char *)  "Segs", oldTotalSegs, totalSegs, 0, 0, 0);
	printf("\r\n");
	oldTotalNodes++;
	totalNodes++;
	PrintMapLump( (char *)  "Subsecs", oldTotalNodes, totalNodes, 0, 0, 0);
	printf("\r\n");
}

#ifdef _WIN32
#define strtok_safe(str, delim, ctx) strtok_s(str, delim, ctx)
#define strcpy_safe(dst, src, len) strcpy_s(dst, len, src)
#else
#define strtok_safe(str, delim, ctx) strtok(str, delim)
#define strcpy_safe(dst, src, len) strcpy(dst, src)
#endif

int getOutputFile(int index, const char* argv[], std::string& wadFileName) {
	FUNCTION_ENTRY(nullptr, "getOutputFile", true);

	// Remove anything after the first '+'
	auto plusPos = wadFileName.find('+');
	if (plusPos != std::string::npos) {
		wadFileName.erase(plusPos);
	}

	const char* arg = argv[index];
	if (arg && *arg == '-') {
		std::string option(arg + 1); // skip '-'
		if (!option.empty()) {
			char ch = static_cast<char>(std::toupper(option[0]));
			if (ch == 'O' || ch == 'X') {
				index++;

				// Handle inline value like "-O:filename" or "-X:filename"
				if (option.size() > 1) {
					if (option[1] != ':') {
						std::cerr << "\nUnrecognized argument '" << arg << "'\n";
						config.Extract = false;
						return index + 1;
					}
					wadFileName = option.substr(2); // skip "O:" or "X:"
				}
				else {
					// Filename should be in next argument
					if (argv[index]) {
						wadFileName = argv[index++];
					}
				}

				if (ch == 'X') {
					config.Extract = true;
				}
				else {
					config.OutputWad = true;
				}
			}
		}
	}

	// Ensure .wad extension
	EnsureExtension(wadFileName, ".wad");

	return index;
}

std::string ConvertNumber(uint32_t value) {
	if (value == 0) return "0";

	std::string result;
	int count = 0;

	while (value > 0) {
		if (count && count % 3 == 0) {
			result.push_back(','); // insert comma
		}
		result.push_back('0' + (value % 10));
		value /= 10;
		count++;
	}

	std::reverse(result.begin(), result.end());
	return result;
}

int main(int argc, const char* argv[]) {
	FUNCTION_ENTRY(NULL, "main", true);

	SaveConsoleSettings();
	HideCursor();

	printf("%s\r\n", ZOKBANNER);
	printf("%s\r\n\r\n", BANNER);

	if (!_isatty(_fileno(stdout))) fprintf(stdout, "%s\n\n", BANNER);
	if (!_isatty(_fileno(stderr))) fprintf(stderr, "%s\n\n", BANNER);

	// Initialize config defaults
	config.BlockMap = {
		true,  // Rebuild
		true,  // Compress
		true,  // RemoveNonCollidable
		false, // OffsetEight
		false, // OffsetZero
		true,  // OffsetThirtySix
		false, // OffsetBruteForce
		0,     // OffsetCommandLineX
		0,     // OffsetCommandLineY
		false, // OffsetUser
		false, // OffsetHeuristic
		1,     // ZeroHeader
		false, // BlockMerge
		false, // SubBlockOptimization
		false, // IdCompatible
		false, // HTMLOutput
		false, // blockBig
		false  // autoDetectBacksideRemoval
	};

	config.Geometry.Simplification = 0;

	config.Nodes = {
		true,                                // Rebuild
		2,                                   // Method
		1,                                   // Thoroughness
		!_isatty(_fileno(stdout)),           // Quiet
		true,                                // Unique
		false,                               // ReduceLineDefs
		true,                                // SegBAMs
		1,                                   // SplitHandling
		3,                                   // SplitReduction
		0,                                   // MultipleSplitMethods
		1,                                   // Cache
		TREE_METRIC_BALANCED,                // Metric
		1,                                   // Width
		100,                                 // Tuning
		false                                // vertexPartition
	};

	config.Reject = {
		true,  // Rebuild
		false, // Empty
		false, // Force
		true,  // UseGraphs
		false  // UseRMB
	};

	config.Statistics = {
		VANILLA_LIMIT, // MaxSize
		false, false, false, false, false, false // Show flags
	};

	config.WriteWAD = true;
	config.Color = false;
	config.OutputWad = false;

	if (argc == 1) {
		PrintHelp(&config);
		return -1;
	}

	ReadConfigFile(argv);

	int argIndex = 1;
	int totalLevels = 0, totalTime = 0, totalUpdates = 0;

	while (KeyPressed()) GetKey();

	do {
		config.Extract = false;
		argIndex = parseArgs(argIndex, argv);
		if (argIndex >= argc) break;

		std::string wadFileName;
		auto myList = getInputFiles(argv[argIndex++], wadFileName);

		if (!myList->IsEmpty()) {
			printf("Working on: %s\r\n\n", wadFileName.c_str());
			TRACE("Processing " << wadFileName);

			char levelNames[MAX_LEVELS + 1][MAX_LUMP_NAME];
			argIndex = getLevels(argIndex, argv, levelNames, myList.get());

			if (levelNames[0][0] == '\0') {
				fprintf(stderr, "Unable to find any valid levels in %s\n", wadFileName.c_str());
				myList.reset();
				break;
			}

			int noLevels = 0;
			int updateCount = myList->wadCount() - 1;

			do {
				std::uint32_t elapsedTime;
				if (ProcessLevel(levelNames[noLevels++], myList.get(), &elapsedTime))
					updateCount++;
				totalTime += elapsedTime;

				if (KeyPressed() && (GetKey() == 0x1B)) break;

			} while (levelNames[noLevels][0]);

			if (config.Statistics.ShowTotals) {
				printTotals();
			}

			config.Extract = false;
			argIndex = getOutputFile(argIndex, argv, wadFileName); // now takes std::string&

			WAD* end = MakeENDOOMLump(myList.get(), wadFileName.c_str());
			updateCount++;

			if (updateCount || config.Extract) {
				if (config.WriteWAD) {
					printf("\r\n%s to %s...", config.Extract ? "Extracting" : "Saving", wadFileName.c_str());
					bool ok = config.Extract
						? myList->Extract(levelNames, wadFileName.c_str())
						: myList->Save(wadFileName.c_str());

					if (!ok) fprintf(stderr, " Error writing to file!\n");
					printf("\r\n");
				}
				else {
					std::cout << "\nChanges would have been written to "
						<< wadFileName
						<< " (" << ConvertNumber(myList->FileSize()) << " bytes)\n";

				}
			}
			else {
				printf(" no updates........... ! \n");
			}
			printf("\r\n");

			if (end) {
				delete end;
			}

			updateCount -= myList->wadCount() - 1;
			totalLevels += noLevels;
			totalUpdates += updateCount;
		}
	} while (argIndex < argc && argv[argIndex]);

	PrintStats(totalLevels, totalTime, totalUpdates - 1); // Why -1?
	RestoreConsoleSettings();

	return 0;
}