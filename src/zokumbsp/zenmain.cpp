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

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _MSC_VER
#include <strings.h>
#endif

#if defined ( __OS2__ )
    #include <conio.h>
    #include <dos.h>
    #include <io.h>
    #define INCL_DOS
    #define INCL_SUB
    #include <os2.h>
#elif defined ( __WIN32__ )
    #include <conio.h>
    #include <io.h>
#elif defined ( __LINUX__ )
    #include <unistd.h>
#else
    #error This platform is not supported
#endif

#if defined ( __BORLANDC__ )
    #include <dir.h>
#endif

#include "common.hpp"
#include "logger.hpp"
#include "wad.hpp"
#include "level.hpp"
#include "console.hpp"
#include "zennode.hpp"
#include "blockmap.hpp"
#include "preprocess.hpp"
#include "endoom.hpp"

DBG_REGISTER ( __FILE__ );

#define ZENVERSION              "1.2.1"
#define ZOKVERSION		"1.0.10-beta1"
#define ZOKVERSIONSHORT		"1.0.9"

const char ZOKBANNER []         = "ZokumBSP Version: " ZOKVERSION " (c) 2016-2017 Kim Roar Foldøy Hauge";
const char BANNER []            = "Based on: ZenNode Version " ZENVERSION " (c) 1994-2004 Marc Rousseau";
const char CONFIG_FILENAME []   = "ZenNode.cfg";
const int  MAX_LEVELS           = 99;
const int  MAX_OPTIONS          = 256;
const int  MAX_WADS             = 32;

char HammingTable [ 256 ];
/*
struct sOptions {
    sBlockMapOptions BlockMap;
    sNodeOptions     Nodes;
    sRejectOptions   Reject;
    bool             WriteWAD;
    bool             Extract;
} config;
*/

sOptions config;

struct sOptionsRMB {
    const char         *wadName;
    sRejectOptionRMB   *option [MAX_OPTIONS];
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



void printHelp () {
    FUNCTION_ENTRY ( NULL, "printHelp", true );

    fprintf ( stdout, "Usage: zokumbsp {-options} filename[.wad] [level{+level}] {-o|x output[.wad]}\n" );
    fprintf ( stdout, "\n" );
    fprintf ( stdout, " -x+ turn on option   -x- turn off option  %c = default\n", DEFAULT_CHAR );
    fprintf ( stdout, "\n" );
    fprintf ( stdout, " -b[chio=0,1,2,3rsz=0,1,2g=0,1,2] %c - Rebuild BLOCKMAP\n", config.BlockMap.Rebuild ? DEFAULT_CHAR : ' ' );
    fprintf ( stdout, "    c              %c   - Compress BLOCKMAP.\n", config.BlockMap.Compress ? DEFAULT_CHAR : ' ' );
    fprintf ( stdout, "    h              %c   - Output BLOCKMAP data as HTML.\n", config.BlockMap.HTMLOutput ?  DEFAULT_CHAR : ' ' );
    fprintf ( stdout, "    i              %c   - Id compatible BLOCKMAP. Sets 'o=1n=2g=0' and 's-r-'\n", config.BlockMap.IdCompatible ? DEFAULT_CHAR : ' ' );
    fprintf ( stdout, "    o                  - Offset configuration.\n");
    fprintf ( stdout, "                   %c     0 = ZenNode 0,0 offset BLOCKMAP.\n", config.BlockMap.OffsetZero ? DEFAULT_CHAR : ' ' );
    fprintf ( stdout, "                   %c     1 = DooMBSP / BSP 8,8 offset BLOCKMAP.\n", config.BlockMap.OffsetEight ? DEFAULT_CHAR : ' ' );
    fprintf ( stdout, "                   %c     2 = Best of 36 offset combinations.\n", config.BlockMap.OffsetThirtySix ? DEFAULT_CHAR : ' ' );
    fprintf ( stdout, "                   %c     3 = Heuristic method to reduce from 65536 offsets.\n", config.BlockMap.OffsetHeuristic ? DEFAULT_CHAR : ' ' );
    fprintf ( stdout, "                   %c     4 = Best of all 65536 offset combinations.\n", config.BlockMap.OffsetBruteForce ? DEFAULT_CHAR : ' ' );
    fprintf ( stdout, "                   %c     x,y = Specify specific offsets.\n", config.BlockMap.OffsetUser ? DEFAULT_CHAR : ' ' );
    // fprintf ( stdout, "    m               %c   - Merge block compression BLOCKMAP. Not done!\n", config.BlockMap.BlockMerge ? DEFAULT_CHAR : ' ' );
    fprintf ( stdout, "    r              %c   - Remove non-collidable lines from BLOCKMAP.\n", config.BlockMap.RemoveNonCollidable ? DEFAULT_CHAR : ' ' );
    fprintf ( stdout, "    s              %c   - Subset compress BLOCKMAP.\n", config.BlockMap.SubBlockOptimization ? DEFAULT_CHAR : ' ' );
    fprintf ( stdout, "    z                  - Zero header configuration.\n");
    fprintf ( stdout, "                   %c     0 = No zero header.\n", (config.BlockMap.ZeroHeader == 0) ? DEFAULT_CHAR : ' ' );
    fprintf ( stdout, "                   %c     1 = Conventional zero header.\n", (config.BlockMap.ZeroHeader == 1) ? DEFAULT_CHAR : ' ' );
    fprintf ( stdout, "                   %c     2 = Zero footer.\n", (config.BlockMap.ZeroHeader == 2) ? DEFAULT_CHAR : ' ' );
    fprintf ( stdout, "    g                  - Geometry simplification.\n");
    fprintf ( stdout, "                   %c     0 = No simplification.\n", (config.BlockMap.GeometrySimplification == 0) ? DEFAULT_CHAR : ' ' );
    fprintf ( stdout, "                   %c     1 = Only if same sector.\n", (config.BlockMap.GeometrySimplification == 1) ? DEFAULT_CHAR : ' ' );
    fprintf ( stdout, "                   %c     2 = Also 1-sided lines in different sectors.\n", (config.BlockMap.GeometrySimplification == 2) ? DEFAULT_CHAR : ' ' );
    fprintf ( stdout, "    b              %c   - Build big 32bit BLOCKMAP.\n", config.BlockMap.blockBig ? DEFAULT_CHAR : ' ' );
    fprintf ( stdout, "\n" );
    fprintf ( stdout, " -n[a=s,d,f|q|u|i] %c - Rebuild NODES\n", config.Nodes.Rebuild ? DEFAULT_CHAR : ' ' );
    fprintf ( stdout, "    a                  - Partition Selection Algorithm\n" );
    fprintf ( stdout, "                   %c     s = Minimize splits.\n", ( config.Nodes.Method == 1 ) ? DEFAULT_CHAR : ' ' );
    fprintf ( stdout, "                   %c     d = Minimize BSP depth.\n", ( config.Nodes.Method == 2 ) ? DEFAULT_CHAR : ' ' );
    fprintf ( stdout, "                   %c     f = Minimize time.\n", ( config.Nodes.Method == 3 ) ? DEFAULT_CHAR : ' ');
    fprintf ( stdout, "                   %c     a = Adaptive, depth then splits.\n", ( config.Nodes.Method == 4 ) ? DEFAULT_CHAR : ' ');
    fprintf ( stdout, "    t                  - Thoroughness, tweak how many variations to test\n" );
    fprintf ( stdout, "                   %c     0 = Try 1 variation.\n", ( config.Nodes.Thoroughness == 0 ) ? DEFAULT_CHAR : ' ' );
    fprintf ( stdout, "                   %c     1 = Try some variations.\n", ( config.Nodes.Thoroughness == 1 ) ? DEFAULT_CHAR : ' ' );
    fprintf ( stdout, "                   %c     2-7 = Try more variations.\n", ( config.Nodes.Thoroughness == 2 ) ? DEFAULT_CHAR : ' ' );
    fprintf ( stdout, "                   %c     x = Try all variations.\n", ( config.Nodes.Thoroughness == 999 ) ? DEFAULT_CHAR : ' ' );
    fprintf ( stdout, "    q              %c   - Don't display progress bar\n", config.Nodes.Quiet ? DEFAULT_CHAR : ' ' );
    fprintf ( stdout, "    u              %c   - Ensure all sub-sectors contain only 1 sector\n", config.Nodes.Unique ? DEFAULT_CHAR : ' ' );
    fprintf ( stdout, "    i              %c   - Ignore non-visible lineDefs.\n", config.Nodes.ReduceLineDefs ? DEFAULT_CHAR : ' ' );
    fprintf ( stdout, "    1                  - Alias for a=s.\n" );
    fprintf ( stdout, "    2                  - Alias for a=d.\n" );
    fprintf ( stdout, "    3                  - Alias for a=f.\n" );
    fprintf ( stdout, "\n");
    fprintf ( stdout, " -r[zfgm]          %c - Rebuild REJECT resource.\n", config.Reject.Rebuild ? DEFAULT_CHAR : ' ' );
    fprintf ( stdout, "    z              %c   - Insert empty REJECT resource.\n", config.Reject.Empty  ? DEFAULT_CHAR : ' ' );
    fprintf ( stdout, "    f              %c   - Rebuild even if REJECT effects are detected.\n", config.Reject.Force ? DEFAULT_CHAR : ' ' );
    fprintf ( stdout, "    g              %c   - Use graphs to reduce LOS calculations.\n", config.Reject.UseGraphs ? DEFAULT_CHAR : ' ' );
    fprintf ( stdout, "    m{b}           %c   - Process RMB option file (.rej).\n", config.Reject.UseRMB ? DEFAULT_CHAR : ' ' );
    fprintf ( stdout, "\n" );
    fprintf ( stdout, " -t                %c - Don't write output file (test mode).\n", ! config.WriteWAD ? DEFAULT_CHAR : ' ' );
    fprintf ( stdout, "\n" );
    fprintf ( stdout, " level - ExMy for DOOM/Heretic or MAPxx for DOOM II/HEXEN.\n" );
}

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
/*	    case 'G' :
	    	config.BlockMap.GeometrySimplification = setting;
		break;*/
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
	    case 'G' :
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
			case '1' : config.Nodes.Method = 1;                 break;
			case '2' : config.Nodes.Method = 2;                 break;
			case '3' : config.Nodes.Method = 3;                 break;
			case 'Q' : config.Nodes.Quiet = setting;            break;
			case 'U' : config.Nodes.Unique = setting;           break;
			case 'I' : config.Nodes.ReduceLineDefs = setting;   break;
			case 'A' :
				if (ptr[0] && ptr[1]) {
					if (ptr[1] == 'S') {
						config.Nodes.Method = 1;
					} else if (ptr[1] == 'D') {
						config.Nodes.Method = 2;
					} else if (ptr[1] == 'F') {
						config.Nodes.Method = 3;
					} else if (ptr[1] == 'A') {
						config.Nodes.Method = 4;
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
			case 'T' : 
				if (ptr[0] && ptr[1]) {
					if (ptr[1] == '0') {
						config.Nodes.Thoroughness = 0;
					} else if (ptr[1] == '1') {
						config.Nodes.Thoroughness = 1;
					} else if (ptr[1] == '2') {
						config.Nodes.Thoroughness = 2;
					} else if (ptr[1] == '3') {
						config.Nodes.Thoroughness = 3;
					} else if (ptr[1] == '4') {
						config.Nodes.Thoroughness = 4;
					} else if (ptr[1] == '5') {
						config.Nodes.Thoroughness = 5;
					} else if (ptr[1] == '6') {
						config.Nodes.Thoroughness = 6;
					} else if (ptr[1] == '7') {
						config.Nodes.Thoroughness = 7;
					} else if (ptr[1] == 'x') {
						config.Nodes.Thoroughness = 999;
					}
					ptr += 2;
				}
				break;
			default  : return true;
		}

		config.Nodes.Rebuild = true;
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

		char *localCopy = strdup ( argv [ index ]);
		char *ptr = localCopy + 1;
		strupr ( localCopy );

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
				case 'N' : localError = parseNODESArgs ( ptr, setting );        break;
				case 'R' : localError = parseREJECTArgs ( ptr, setting );       break;
				case 'T' : config.WriteWAD = ! setting;                         break;
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

	if ( errors ) fprintf ( stderr, "\n" );

	return index;
}

void ReadConfigFile ( const char *argv [] )
{
	FUNCTION_ENTRY ( NULL, "ReadConfigFile", true );

	FILE *configFile = fopen ( CONFIG_FILENAME, "rt" );
	if ( configFile == NULL ) {
		char fileName [ 256 ];
		strcpy ( fileName, argv [0] );
		char *ptr = &fileName [ strlen ( fileName )];
		while (( --ptr >= fileName ) && ( *ptr != SEPERATOR ));
		*++ptr = '\0';
		strcat ( ptr, CONFIG_FILENAME );
		configFile = fopen ( fileName, "rt" );
	}
	if ( configFile == NULL ) return;

	char ch = ( char ) fgetc ( configFile );
	bool errors = false;
	while ( ! feof ( configFile )) {
		ungetc ( ch, configFile );

		char lineBuffer [ 256 ];
		if (fgets ( lineBuffer, sizeof ( lineBuffer ), configFile ) == NULL) {
			fprintf(stderr, "Input error\n");
		}
		char *basePtr = strupr ( lineBuffer );
		while ( *basePtr == ' ' ) basePtr++;
		basePtr = strtok ( basePtr, "\n\x1A" );

		if ( basePtr ) {
			char *ptr = basePtr;
			bool localError = false;
			while ( *ptr && ( localError == false )) {
				int option = *ptr++;
				bool setting = true;
				if (( *ptr == '+' ) || ( *ptr == '-' )) {
					setting = ( *ptr++ == '-' ) ? false : true;
				}
				switch ( option ) {
					case 'B' : localError = parseBLOCKMAPArgs ( ptr, setting );     break;
					case 'N' : localError = parseNODESArgs ( ptr, setting );        break;
					case 'R' : localError = parseREJECTArgs ( ptr, setting );       break;
					case 'T' : config.WriteWAD = ! setting;                         break;
					default  : localError = true;
				}
			}
			if ( localError ) {
				errors = true;
				int offset = basePtr - lineBuffer - 1;
				size_t width = strlen ( basePtr ) + 1;
				fprintf ( stderr, "Unrecognized configuration option '%*.*s'\n", (int) width, (int) width, lineBuffer + offset );
			}
		}
		ch = ( char ) fgetc ( configFile );
	}
	fclose ( configFile );
	if ( errors ) fprintf ( stderr, "\n" );
}

int getLevels ( int argIndex, const char *argv [], char names [][MAX_LUMP_NAME], wadList *list ) {
	FUNCTION_ENTRY ( NULL, "getLevels", true );

	int index = 0, errors = 0;

	char buffer [2048];
	buffer [0] = '\0';
	if ( argv [argIndex] ) {
		strcpy ( buffer, argv [argIndex] );
		strupr ( buffer );
	}
	char *ptr = strtok ( buffer, "+" );

	// See if the user requested specific levels
	if ( WAD::IsMap ( ptr )) {
		argIndex++;
		while ( ptr ) {
			if ( WAD::IsMap ( ptr )) {
				if ( list->FindWAD ( ptr )) {
					strcpy ( names [index++], ptr );
				} else {
					fprintf ( stderr, "  Could not find %s\n", ptr, errors++ );
				}
			} else {
				fprintf ( stderr, "  %s is not a valid name for a level\n", ptr, errors++ );
			}
			ptr = strtok ( NULL, "+" );
		}
	} else {
		int size = list->DirSize ();
		const wadListDirEntry *dir = list->GetDir ( 0 );
		for ( int i = 0; i < size; i++ ) {
			if ( dir->wad->IsMap ( dir->entry->name )) {
				// Make sure it's really a level
				if ( strcmp ( dir[1].entry->name, "THINGS" ) == 0 ) {
					if ( index == MAX_LEVELS ) {
						fprintf ( stderr, "ERROR: Too many levels in WAD - ignoring %s!\n", dir->entry->name, errors++ );
					} else {
						memcpy ( names [index++], dir->entry->name, MAX_LUMP_NAME );
					}
				}
			}
			dir++;
		}
	}
	memset ( names [index], 0, MAX_LUMP_NAME );

	if ( errors ) fprintf ( stderr, "\n" );

	return argIndex;
}

bool ReadOptionsRMB ( const char *wadName, sOptionsRMB *options ) {
	FUNCTION_ENTRY ( NULL, "ReadOptionsRMB", true );

	char fileName [ 256 ];
	strcpy ( fileName, wadName );
	char *ptr = &fileName [ strlen ( fileName )];
	while ( *--ptr != '.' );
	*++ptr = '\0';
	strcat ( ptr, "rej" );
	while (( ptr > fileName ) && ( *ptr != SEPERATOR )) ptr--;
	if (( ptr < fileName ) || ( *ptr == SEPERATOR )) ptr++;
	FILE *optionFile = fopen ( ptr, "rt" );
	if ( optionFile == NULL ) {
		optionFile = fopen ( fileName, "rt" );
		if ( optionFile == NULL ) return false;
	}

	memset ( options, 0, sizeof ( sOptionsRMB ));

	options->wadName = strdup ( wadName );

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
			options->option [index]  = new sRejectOptionRMB;
			*options->option [index] = tempOption;
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

void EnsureExtension ( char *fileName, const char *ext ) {
	FUNCTION_ENTRY ( NULL, "EnsureExtension", true );

	// See if the file exists first
	FILE *file = fopen ( fileName, "rb" );
	if ( file != NULL ) {
		fclose ( file );
		return;
	}

	size_t length = strlen ( fileName );
	if ( stricmp ( &fileName [length-4], ext ) != 0 ) {
		strcat ( fileName, ext );
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
		default         : name = "<Unknown>";   break;
	}
	return name;
}

wadList *getInputFiles ( const char *cmdLine, char *wadFileName )
{
	FUNCTION_ENTRY ( NULL, "getInputFiles", true );

	char *listNames = wadFileName;
	wadList *myList = new wadList;

	if ( cmdLine == NULL ) return myList;

	char temp [ 256 ];
	strcpy ( temp, cmdLine );
	char *ptr = strtok ( temp, "+" );

	int errors = 0;
	int index  = 0;

	bool wadOk = true;

	while ( ptr && *ptr ) {
		char wadName [ 256 ];
		strcpy ( wadName, ptr );
		EnsureExtension ( wadName, ".wad" );

		WAD *wad = new WAD ( wadName );
		if ( wad->Status () != ws_OK ) {
			const char *msg;
			wadOk = false;
			switch ( wad->Status ()) {
				case ws_INVALID_FILE : 
					msg = "The file %s does not exist\n";             
					break;
				case ws_CANT_READ    : 
					msg = "Can't open the file %s for read access\n"; 
					break;
				case ws_INVALID_WAD  : 
					msg = "%s is not a valid WAD file\n";             
					break;
				default              : 
					msg = "** Unexpected Error opening %s **\n";      
					break;
			}
			fprintf ( stderr, msg, wadName );
			delete wad;
		} else {
			if ( ! myList->IsEmpty ()) {
				cprintf ( "Merging: %s with %s\r\n", wadName, listNames );
				*wadFileName++ = '+';
			}
			if ( myList->Add ( wad ) == false ) {
				errors++;
				if ( myList->Type () != wt_UNKNOWN ) {
					fprintf ( stderr, "ERROR: %s is not a %s PWAD.\n", wadName, TypeName ( myList->Type ()));
				} else {
					fprintf ( stderr, "ERROR: %s is not the same type.\n", wadName );
				}
				delete wad;
			} else {
				if (( config.Reject.UseRMB == true ) && ( index < MAX_WADS )) {
					if ( ReadOptionsRMB ( wadName, &rmbOptionTable [index] ) == true ) index++;
				} else if ( index == MAX_WADS ) {
					fprintf ( stderr, "WARNING: Too many wads specified - RMB options ignored" );
					index++;
				}
				char *end = wadName + strlen ( wadName ) - 1;
				while (( end > wadName ) && ( *end != SEPERATOR )) end--;
				if ( *end == SEPERATOR ) end++;
				wadFileName += sprintf ( wadFileName, "%s", end );
			}
		}
		ptr = strtok ( NULL, "+" );
	}

	if ( wadOk && (wadFileName [-1] == '+') ) wadFileName [-1] = '\0';
	if ( myList->wadCount () > 1 ) cprintf ( "\r\n" );
	if ( errors ) fprintf ( stderr, "\n" );
	if ( index != 0 ) fprintf ( stdout, "\n" );

	return myList;
}

void ReadSection ( FILE *file, int max, bool *array ) {
	FUNCTION_ENTRY ( NULL, "ReadSection", true );

	char ch = ( char ) fgetc ( file );
	while (( ch != '[' ) && ! feof ( file )) {
		ungetc ( ch, file );
		char lineBuffer [ 256 ];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result"

		fgets ( lineBuffer, sizeof ( lineBuffer ), file );

#pragma GCC diagnostic pop

		strtok ( lineBuffer, "\n\x1A" );
		char *ptr = lineBuffer;
		while ( *ptr == ' ' ) ptr++;

		bool value = true;
		if ( *ptr == '!' ) {
			value = false;
			ptr++;
		}
		ptr = strtok ( ptr, "," );
		while ( ptr ) {
			int low = -1, high = 0, count = 0;
			if ( stricmp ( ptr, "all" ) == 0 ) {
				memset ( array, value, sizeof ( bool ) * max );
			} else {
				count = sscanf ( ptr, "%d-%d", &low, &high );
			}
			ptr = strtok ( NULL, "," );
			if (( low < 0 ) || ( low >= max )) continue;
			switch ( count ) {
				case 1 : array [low] = value;
					 break;
				case 2 : if ( high >= max ) high = max - 1;
						 for ( int i = low; i <= high; i++ ) {
							 array [i] = value;
						 }
					 break;
			}
			if ( count == 0 ) break;
		}
		ch = ( char ) fgetc ( file );
	}
	ungetc ( ch, file );
}

void ReadCustomFile ( DoomLevel *curLevel, wadList *myList, sBSPOptions *options )
{
	FUNCTION_ENTRY ( NULL, "ReadCustomFile", true );

	char fileName [ 256 ];
	const wadListDirEntry *dir = myList->FindWAD ( curLevel->Name (), NULL, NULL );
	strcpy ( fileName, dir->wad->Name ());
	char *ptr = &fileName [ strlen ( fileName )];
	while ( *--ptr != '.' );
	*++ptr = '\0';
	strcat ( ptr, "zen" );
	while (( ptr > fileName ) && ( *ptr != SEPERATOR )) ptr--;
	if (( ptr < fileName ) || ( *ptr == SEPERATOR )) ptr++;
	FILE *optionFile = fopen ( ptr, "rt" );
	if ( optionFile == NULL ) {
		optionFile = fopen ( fileName, "rt" );
		if ( optionFile == NULL ) return;
	}

	char ch = ( char ) fgetc ( optionFile );
	bool foundMap = false;
	do {
		while ( ! feof ( optionFile ) && ( ch != '[' )) ch = ( char ) fgetc ( optionFile );
		char lineBuffer [ 256 ];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result"

		fgets ( lineBuffer, sizeof ( lineBuffer ), optionFile );

#pragma GCC diagnostic pop

		strtok ( lineBuffer, "\n\x1A]" );
		if ( WAD::IsMap ( lineBuffer )) {
			if ( strcmp ( lineBuffer, curLevel->Name ()) == 0 ) {
				foundMap = true;
			} else if ( foundMap ) {
				break;
			}
		}
		if ( ! foundMap ) {
			ch = ( char ) fgetc ( optionFile );
			continue;
		}

		int maxIndex = 0;
		bool isSectorSplit = false;
		bool *array = NULL;
		if ( stricmp ( lineBuffer, "ignore-linedefs" ) == 0 ) {
			maxIndex = curLevel->LineDefCount ();
			if ( options->ignoreLineDef == NULL ) {
				options->ignoreLineDef = new bool [ maxIndex ];
				memset ( options->ignoreLineDef, false, sizeof ( bool ) * maxIndex );
			}
			array = options->ignoreLineDef;
		} else if ( stricmp ( lineBuffer, "dont-split-linedefs" ) == 0 ) {
			maxIndex = curLevel->LineDefCount ();
			if ( options->dontSplit == NULL ) {
				options->dontSplit = new bool [ maxIndex ];
				memset ( options->dontSplit, false, sizeof ( bool ) * maxIndex );
			}
			array = options->dontSplit;
		} else if ( stricmp ( lineBuffer, "dont-split-sectors" ) == 0 ) {
			isSectorSplit = true;
			maxIndex = curLevel->LineDefCount ();
			if ( options->dontSplit == NULL ) {
				options->dontSplit = new bool [ maxIndex ];
				memset ( options->dontSplit, false, sizeof ( bool ) * maxIndex );
			}
			maxIndex = curLevel->SectorCount ();
			array = new bool [ maxIndex ];
			memset ( array, false, sizeof ( bool ) * maxIndex );
		} else if ( stricmp ( lineBuffer, "unique-sectors" ) == 0 ) {
			maxIndex = curLevel->SectorCount ();
			if ( options->keepUnique == NULL ) {
				options->keepUnique = new bool [ maxIndex ];
				memset ( options->keepUnique, false, sizeof ( bool ) * maxIndex );
			}
			array = options->keepUnique;
		}
		if ( array != NULL ) {
			ReadSection ( optionFile, maxIndex, array );
			if ( isSectorSplit == true ) {
				const wLineDefInternal *lineDef = curLevel->GetLineDefs ();
				const wSideDef *sideDef = curLevel->GetSideDefs ();
				for ( int side, i = 0; i < curLevel->LineDefCount (); i++, lineDef++ ) {
					side = lineDef->sideDef [0];
					if (( side != NO_SIDEDEF ) && ( array [ sideDef [ side ].sector ])) {
						options->dontSplit [i] = true;
					}
					side = lineDef->sideDef [1];
					if (( side != NO_SIDEDEF ) && ( array [ sideDef [ side ].sector ])) {
						options->dontSplit [i] = true;
					}
				}
				delete [] array;
			}
		}

		ch = ( char ) fgetc ( optionFile );

	} while ( ! feof ( optionFile ));

	fclose ( optionFile );
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
void PrintTime ( UINT32 time ) {
	FUNCTION_ENTRY ( NULL, "PrintTime", false );

	GotoXY ( 53, startY );
	cprintf ( "%4ld.%03ld sec%s", time / 1000, time % 1000, ( time == 1000 ) ? "" : "s" );
}

bool ProcessLevel ( char *name, wadList *myList, UINT32 *elapsed ) {
	FUNCTION_ENTRY ( NULL, "ProcessLevel", true );

	UINT32 dummyX = 0;

	*elapsed = 0;

	// cprintf ( "\r%-*.*s", MAX_LUMP_NAME, MAX_LUMP_NAME, name );
	cprintf ( "\r%-*.*s        Old       New    %%Change    %%Limit   Time Elapsed\n\r", MAX_LUMP_NAME, MAX_LUMP_NAME, name );
	GetXY ( &startX, &startY );
	// startX = 4;

	const wadListDirEntry *dir = myList->FindWAD ( name );
	DoomLevel *curLevel = new DoomLevel ( name, dir->wad );
	if ( curLevel->IsValid ( ! config.Nodes.Rebuild ) == false ) {
		cprintf ( "This level is not valid... " );
		cprintf ( "\r\n" );
		delete curLevel;
		return false;
	}

	int rows = 0;

	// sMapExtraData *extraData = new sMapExtraData;

	startX = 4;

	if ( config.BlockMap.Rebuild || config.Nodes.Rebuild || config.Reject.Rebuild ) {
		UINT32 preTime = CurrentTime ();

		int oldSectorCount = curLevel->SectorCount ();
		int oldLineCount = curLevel->LineDefCount();

		MapExtraData(curLevel, &config);
		*elapsed += preTime = CurrentTime () - preTime;
		// Status ( (char *) "" );
		// GotoXY ( startX, startY );
		//cprintf ( "PREPROCESS - ");

		Status ( (char *) "" );
		GotoXY ( startX, startY );

		cprintf ( "Linedefs: %6d => %6d ", (int) oldLineCount,  curLevel->LineDefCount ());

		if ( oldLineCount ) {
			cprintf ( "   %6.2f%%", ( float ) ( 100.0 * curLevel->LineDefCount () / (float) oldLineCount ));
		} else {
			cprintf ( " - " );
		}

		double pct = ((double) curLevel->LineDefCount () / 32768.0) * 100.0;
		cprintf("   %6.2f%%", pct);

		PrintTime (preTime);
		cprintf ( "\r\n" );
		GotoXY ( startX, startY );

		cprintf ( "Sectors:  %6d => %6d ", (int) oldSectorCount,  curLevel->SectorCount ());

		if ( oldSectorCount ) {
			cprintf ( "   %6.2f%%", ( float ) ( 100.0 * curLevel->SectorCount () / (float) oldSectorCount ));
		} else {
			cprintf ( " - " );
		}

		pct = ((double) curLevel->SectorCount () / 32768.0) * 100.0;
		cprintf("   %6.2f%%", pct);
		cprintf ( "\r\n" );

		GetXY ( &dummyX, &startY );

	}

	if ( config.BlockMap.Rebuild ) {

		rows++;

		int oldSize = curLevel->BlockMapSize ();
		UINT32 blockTime = CurrentTime ();
		int saved = CreateBLOCKMAP ( curLevel, config.BlockMap );
		*elapsed += blockTime = CurrentTime () - blockTime;
		int newSize = curLevel->BlockMapSize ();

		Status ( (char *) "" );
		GotoXY ( startX, startY );

		if ( saved >= 0 ) {
			cprintf ( "Blockmap: %6d => %6d ", oldSize, newSize );
			if ( oldSize ) cprintf ( "   %6.2f%%", ( float ) ( 100.0 * newSize / oldSize ));
			else cprintf ( " - " );
			/*cprintf ( "   Compressed: " );
			  if ( newSize + saved ) cprintf ( "%3.2f%%", ( float ) ( 100.0 * newSize / ( newSize + saved ) ));
			  else cprintf ( "(****)" );
			  */
			double pct = ((double) newSize / 65535.0) * 100.0; 
			cprintf("   %6.2f%%", pct);
		} else {
			cprintf ( "Blockmap: * Level too big to create valid BLOCKMAP *" );
		}

		PrintTime ( blockTime );

		cprintf ( "\r\n" );
		GetXY ( &dummyX, &startY );
	}

	if ( config.Nodes.Rebuild ) {

		rows++;

		int oldNodeCount = curLevel->NodeCount ();
		int oldSegCount  = curLevel->SegCount ();

		bool *keep = new bool [ curLevel->SectorCount ()];
		memset ( keep, config.Nodes.Unique, sizeof ( bool ) * curLevel->SectorCount ());

		sBSPOptions options;
		options.algorithm      = config.Nodes.Method;
		options.thoroughness   = config.Nodes.Thoroughness;
		options.showProgress   = ! config.Nodes.Quiet;
		options.reduceLineDefs = config.Nodes.ReduceLineDefs;
		options.ignoreLineDef  = NULL;
		options.dontSplit      = NULL;
		options.keepUnique     = keep;

		ReadCustomFile ( curLevel, myList, &options );

		UINT32 nodeTime = CurrentTime ();
		CreateNODES ( curLevel, &options );
		*elapsed += nodeTime = CurrentTime () - nodeTime;

		if ( options.ignoreLineDef ) delete [] options.ignoreLineDef;
		if ( options.dontSplit ) delete [] options.dontSplit;
		if ( options.keepUnique ) delete [] options.keepUnique;

		Status ( (char *) "" );
		GotoXY ( startX, startY );

		double pct;

		totalNodes +=  curLevel->NodeCount ();
		oldTotalNodes += oldNodeCount;
		totalSegs += curLevel->SegCount ();
		oldTotalSegs += oldSegCount;

		cprintf ( "Nodes: %9d => %6d    ", oldNodeCount,  curLevel->NodeCount ());
		if ( oldNodeCount ) {
			double pct = ( 100.0 * (curLevel->NodeCount () / (double) oldNodeCount) );
			cprintf ( "%6.2f%%", pct);
			//cprintf ( "%3.1f%%", ( 100.0 * (curLevel->NodeCount () / oldNodeCount) ));
		}
		else {
			cprintf ( "     -" );
		}
		pct = ((double) curLevel->NodeCount() / 32878.0) * 100.0;
		cprintf("   %6.2f%%", pct);

		PrintTime ( nodeTime );

		//cprintf("\r\n");
		cprintf("\r\n");
		GotoXY ( startX, startY );

		cprintf ( "Segs: %10d => %6d ",  oldSegCount, curLevel->SegCount ());

		if ( oldSegCount ) {
			pct = ( 100.0 * ( (double) curLevel->SegCount () / (double) oldSegCount) );
			cprintf ( "   %6.2f%%", pct);
		} else {
			cprintf ( "     -" );
		}

		pct = ((double) curLevel->SegCount() / 32768.0) * 100.0;
		cprintf("   %6.2f%%", pct);
		/*
		   cprintf ( "\r\n   Nodes: %9d => %6d   ", oldNodeCount,  curLevel->NodeCount ());
		   if ( oldNodeCount ) {
		   pct = ( 100.0 * (curLevel->NodeCount () / (double) oldNodeCount) );
		   cprintf ( " %6.2f%%", pct);
		//cprintf ( "%3.1f%%", ( 100.0 * (curLevel->NodeCount () / oldNodeCount) ));
		}
		else {
		cprintf ( "     -" );
		}
		pct = ((double) curLevel->NodeCount() / 32878.0) * 100.0;
		cprintf("   %6.2f%%", pct);
		*/

		// PrintTime ( nodeTime );
		cprintf ( "\r\n" );
		GetXY ( &dummyX, &startY );
	}

	if ( config.Reject.Rebuild ) {

		rows++;

		int oldEfficiency = CheckREJECT ( curLevel );

		UINT32 rejectTime = CurrentTime ();
		bool special = CreateREJECT ( curLevel, config.Reject, config.BlockMap );
		*elapsed += rejectTime = CurrentTime () - rejectTime;

		int newEfficiency = CheckREJECT ( curLevel );

		if ( special == false ) {
			Status ( (char *) "" );
			GotoXY ( startX, startY );
			//cprintf ( "Reject:  %3ld.%1ld%% =>%3ld.%1ld%%  Sectors: %5d", newEfficiency / 100, newEfficiency % 100,
			//		oldEfficiency / 100, oldEfficiency % 100, curLevel->SectorCount ());
			cprintf ( "Reject:  %3ld.%1ld%% =>%3ld.%1ld%%", newEfficiency / 100, newEfficiency % 100, oldEfficiency / 100, oldEfficiency % 100);

			cprintf ( "    %6.2f%%         -", ((double) newEfficiency / (double) oldEfficiency) * 100.0);


			PrintTime ( rejectTime );
		} else {
			cprintf ( "Reject: Special effects detected, use -rf to force an update." );
		}

		cprintf ( "\r\n" );
		GetXY ( &dummyX, &startY );
	}

	bool changed = false;
	if ( rows != 0 ) {
		Status ( (char *) "Updating Level ... " );
		changed = curLevel->UpdateWAD ();
		Status ( (char *) "" );
		if ( changed ) {
			MoveUp ( rows );
			cprintf ( "\r *" );
			MoveDown ( rows );
		}
	} else {
		cprintf ( "Nothing to do here ... " );
		cprintf ( "\r\n" );
	}

	int noSectors = curLevel->SectorCount ();
	int rejectSize = (( noSectors * noSectors ) + 7 ) / 8;
	if (( curLevel->RejectSize () != rejectSize ) && ( config.Reject.Rebuild == false )) {
		fprintf ( stderr, "WARNING: The REJECT structure for %s is the wrong size - try using -r\n", name );
	}
	// delete [] curLevel->extraData->lineDefsUsed;
	// delete curLevel->extraData;
	delete curLevel;

	return changed;
}

void PrintStats ( int totalLevels, UINT32 totalTime, int totalUpdates ) {
	FUNCTION_ENTRY ( NULL, "PrintStats", true );

	if ( totalLevels != 0 ) {

		cprintf ( "%d Level%s processed in ", totalLevels, totalLevels > 1 ? "s" : "" );
		if ( totalTime > 60000 ) {
			UINT32 minutes = totalTime / 60000;
			UINT32 tempTime = totalTime - minutes * 60000;
			cprintf ( "%ld minute%s %ld.%03ld second%s - ", minutes, minutes > 1 ? "s" : "", tempTime / 1000, tempTime % 1000, ( tempTime == 1000 ) ? "" : "s" );
		} else {
			cprintf ( "%ld.%03ld second%s - ", totalTime / 1000, totalTime % 1000, ( totalTime == 1000 ) ? "" : "s"  );
		}

		if ( totalUpdates ) {
			cprintf ( "%d Level%s need%s updating.\r\n", totalUpdates, totalUpdates > 1 ? "s" : "", config.WriteWAD ? "ed" : "" );
		} else {
			cprintf ( "No Levels need%s updating.\r\n", config.WriteWAD ? "ed" : "" );
		}

		if ( totalTime == 0 ) {
			cprintf ( "WOW! Whole bunches of levels/sec!\r\n" );
		} else if ( totalTime < 1000 ) {
			cprintf ( "%f levels/sec\r\n", 1000.0 * totalLevels / totalTime );
		} else if ( totalLevels > 1 ) {
			cprintf ( "%f secs/level\r\n", totalTime / ( totalLevels * 1000.0 ));
		}
	}
}

void printTotals(void) {
	double pct;

	// summary..
	GotoXY ( 0, startY );
	cprintf ( "=================================================================");

	cprintf("\r\n");
	GotoXY ( startX, startY );

	cprintf ( "Nodes: %9d => %6d    ", oldTotalNodes, totalNodes );
	if ( oldTotalNodes ) {
		pct = ( 100.0 * (totalNodes / (double) oldTotalNodes) );
		cprintf ( "%6.2f%%", pct);
	}

	cprintf("\r\n");
	GotoXY ( startX, startY );

	cprintf ( "Segs: %10d => %6d ",  oldTotalSegs, totalSegs );

	if ( oldTotalSegs ) {
		pct = ( 100.0 * ( (double) totalSegs / (double) oldTotalSegs) );
		cprintf ( "   %6.2f%%", pct);
	} else {
		cprintf ( "     -" );
	}
	cprintf("\r\n");

}

int getOutputFile ( int index, const char *argv [], char *wadFileName )
{
	FUNCTION_ENTRY ( NULL, "getOutputFile", true );

	strtok ( wadFileName, "+" );

	const char *ptr = argv [ index ];
	if ( ptr && ( *ptr == '-' )) {
		char ch = ( char ) toupper ( *++ptr );
		if (( ch == 'O' ) || ( ch == 'X' )) {
			index++;
			if ( *++ptr ) {
				if ( *ptr++ != ':' ) {
					fprintf ( stderr, "\nUnrecognized argument '%s'\n", argv [ index ] );
					config.Extract = false;
					return index + 1;
				}
			} else {
				ptr = argv [ index++ ];
			}
			if ( ptr ) strcpy ( wadFileName, ptr );
			if ( ch == 'X' ) {
				config.Extract = true;
			} else {
				config.OutputWad = true;
			}
		}
	}

	EnsureExtension ( wadFileName, ".wad" );

	return index;
}

char *ConvertNumber ( UINT32 value ) {
	FUNCTION_ENTRY ( NULL, "ConvertNumber", true );

	static char buffer [ 25 ];
	char *ptr = &buffer [ 20 ];

	while ( value ) {
		if ( value < 1000 ) sprintf ( ptr, "%4d", value );
		else sprintf ( ptr, ",%03d", value % 1000 );
		if ( ptr < &buffer [ 20 ] ) ptr [4] = ',';
		value /= 1000;
		if ( value ) ptr -= 4;
	}
	while ( *ptr == ' ' ) ptr++;
	return ptr;
}

/*
   long long totalNodes = 0;
   long long oldTotalNodes = 0;
   long long totalSegs = 0;
   long long oldTotalSegs = 0;
   long long totalBlockmap = 0;
   long long oldTotalBlockmap = 0;
   */


int main ( int argc, const char *argv [] ) {
	FUNCTION_ENTRY ( NULL, "main", true );

	SaveConsoleSettings ();
	HideCursor ();


	cprintf ( "%s\r\n", ZOKBANNER );
	cprintf ( "%s\r\n\r\n", BANNER );

	if ( ! isatty ( fileno ( stdout ))) fprintf ( stdout, "%s\n\n", BANNER );
	if ( ! isatty ( fileno ( stderr ))) fprintf ( stderr, "%s\n\n", BANNER );

	config.BlockMap.Rebuild     = true;
	config.BlockMap.Compress    = true;
	config.BlockMap.RemoveNonCollidable = true;
	config.BlockMap.OffsetEight = false;
	config.BlockMap.OffsetZero  = false;
	config.BlockMap.OffsetThirtySix = true;
	config.BlockMap.OffsetBruteForce  = false;
	config.BlockMap.OffsetCommandLineX = 0;
	config.BlockMap.OffsetCommandLineY = 0;
	config.BlockMap.OffsetUser = false;
	config.BlockMap.OffsetHeuristic = false;
	config.BlockMap.ZeroHeader = 1;
	config.BlockMap.BlockMerge  = false;
	config.BlockMap.SubBlockOptimization = false;
	config.BlockMap.GeometrySimplification = 0;
	config.BlockMap.IdCompatible = false;
	config.BlockMap.HTMLOutput = false;
	config.BlockMap.blockBig = false;

	config.Nodes.Rebuild        = true;
	config.Nodes.Method         = 1;
	config.Nodes.Thoroughness   = 1;
	config.Nodes.Quiet          = isatty ( fileno ( stdout )) ? false : true;
	config.Nodes.Unique         = false;
	config.Nodes.ReduceLineDefs = false;

	config.Reject.Rebuild       = true;
	config.Reject.Empty         = false;
	config.Reject.Force         = false;
	config.Reject.UseGraphs     = true;
	config.Reject.UseRMB        = false;

	config.WriteWAD             = true;
	config.OutputWad            = false;

	if ( argc == 1 ) {
		printHelp ();
		return -1;
	}

	ReadConfigFile ( argv );

	int argIndex = 1;
	int totalLevels = 0, totalTime = 0, totalUpdates = 0;

	while ( KeyPressed ()) GetKey ();

	do {

		config.Extract = false;
		argIndex = parseArgs ( argIndex, argv );
		if ( argIndex >= argc ) break;

		char wadFileName [ 256 ];
		wadList *myList = getInputFiles ( argv [argIndex++], wadFileName );
		if ( myList->IsEmpty () == false ) {

			cprintf ( "Working on: %s\r\n\n", wadFileName );

			TRACE ( "Processing " << wadFileName );

			char levelNames [MAX_LEVELS+1][MAX_LUMP_NAME];
			argIndex = getLevels ( argIndex, argv, levelNames, myList );

			if ( levelNames [0][0] == '\0' ) {
				fprintf ( stderr, "Unable to find any valid levels in %s\n", wadFileName );
				break;
			}

			int noLevels = 0;
			// Trick the code into writing an output file if two or more wads are being merged
			int updateCount = myList->wadCount () - 1;

			do {

				UINT32 elapsedTime;
				if ( ProcessLevel ( levelNames [noLevels++], myList, &elapsedTime )) updateCount++;
				totalTime += elapsedTime;
				if ( KeyPressed () && ( GetKey () == 0x1B )) break;

			} while ( levelNames [noLevels][0] );

			printTotals();

			config.Extract = false;
			argIndex = getOutputFile ( argIndex, argv, wadFileName );

			// generate new ENDOOM goes here
			// WAD *end = MakeENDOOMLump(myList, "fuxxor");
			WAD *end = MakeENDOOMLump(myList, wadFileName);
			//totalUpdates += 1;
			updateCount++;

			if ( updateCount || config.Extract ) {
				if ( config.WriteWAD ) {
					cprintf ( "\r\n%s to %s...", config.Extract ? "Extracting" : "Saving", wadFileName );
					if ( config.Extract ) {
						if ( myList->Extract ( levelNames, wadFileName ) == false ) {
							fprintf ( stderr," Error writing to file!\n" );
						}
					} else {
						if ( myList->Save ( wadFileName ) == false ) {
							fprintf ( stderr," Error writing to file!\n" );
						}
					}
					cprintf ( "\r\n" );
				} else {
					cprintf ( "\r\nChanges would have been written to %s ( %s bytes )\n", wadFileName, ConvertNumber ( myList->FileSize ()));
				}
			} else {
				printf(" no updates........... ! \n");
			}
			cprintf ( "\r\n" );

			if (end) {
				delete end;
			}

			// Undo the bogus update level count
			updateCount -= myList->wadCount () - 1;

			totalLevels += noLevels;
			totalUpdates += updateCount;
		}

		delete myList;

	} while ( argv [argIndex] );

	PrintStats ( totalLevels, totalTime, totalUpdates );
	RestoreConsoleSettings ();

	return 0;
}
