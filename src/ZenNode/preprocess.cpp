#include <math.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include "common.hpp"
#include "level.hpp"
#include "ZenNode.hpp"
#include "blockmap.hpp"
#include "console.hpp"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "endoom.hpp"

#include <algorithm>

//void BlockMapExtraData( DoomLevel *level, sMapExtraData *extraData, // const sBlockMapOptions &options) {
void MapExtraData( DoomLevel *level, const sOptions *config) {
	const sBlockMapOptions options = config->BlockMap; //  sOptions
	
	level->extraData = new sMapExtraData;
	sMapExtraData *extraData = level->extraData;

	const wSideDef *sideDef = level->GetSideDefs ();
	const wSector *sectors = level->GetSectors();

	const wLineDef *lineDef = level->GetLineDefs();
	const wVertex *vertex   = level->GetVertices();

	int numberOfLineDefs = level->LineDefCount();

	extraData->multiSectorSpecial = false;

	extraData->rightMostVertex = extraData->leftMostVertex = vertex [0].x;
	extraData->bottomVertex = extraData->topVertex = vertex [0].y;


	// For simplicity we set them all to true, then to false if needed :)
	extraData->lineDefsUsed= new bool [numberOfLineDefs];

	for ( int i = 0; i < level->LineDefCount(); i++ ) {
		extraData->lineDefsUsed[i] = true;
	}

	// Check all linedefs and flag them as false if we don't
	// need to check for collisions.

	bool removenoncollidable = !options.IdCompatible && options.RemoveNonCollidable;

	if (removenoncollidable) {
		// first we look for raising stairs and donuts
		for ( int i = 0; i < level->LineDefCount(); i++ ) {
			switch (lineDef [i].type) {
				case 9:
				case 8:
				case 127:
				case 100:
				case 7:
				// BOOM types
				// stairs
				case 258:
				case 256:
				case 259:
				case 257:
				// donuts
				case 146:
				case 155:
				case 191:

					extraData->multiSectorSpecial = true;
			}
			// Generalized check
			if ((lineDef [i].type >= 0x3000) && (lineDef [i].type <= 0x33FF)) {
				extraData->multiSectorSpecial = true;
			} 
		}

		for ( int i = 0; i < level->LineDefCount(); i++ ) {
			if ((lineDef [i].tag) == 999) {
				extraData->lineDefsUsed[i] = false;
			} else if ((lineDef [i].flags & LDF_TWO_SIDED) 
					&& !(lineDef [i].flags & LDF_IMPASSABLE)  
					&& !(lineDef [i].flags & LDF_BLOCK_MONSTERS)
					&& (lineDef [i].type == 0)
				  ) {

				int sectorL = sideDef [lineDef [i].sideDef[LEFT_SIDEDEF]].sector;
				int sectorR = sideDef [lineDef [i].sideDef[RIGHT_SIDEDEF]].sector;

				// map01 n doom2.wad has a linedef that partions a sector into two parts, but serves NO purpose
				if (sectorR == sectorL) {
					// printf("Linedef %d\n", i);
					extraData->lineDefsUsed[i] = false;
				} else if (extraData->multiSectorSpecial == false) {

					// two sided linedef, without any action, same floor and ceiling
					if ((sectors[sectorL].floorh == sectors[sectorR].floorh) 
							&& (sectors[sectorL].ceilh == sectors[sectorR].ceilh)
							&& (sectors[sectorL].trigger == 0)
							&& (sectors[sectorR].trigger == 0)
							&& (sectors[sectorR].special != 10) // door like effects
							&& (sectors[sectorR].special != 14)
							&& (sectors[sectorL].special != 10)
							&& (sectors[sectorL].special != 14)
					   ) 

					{
						// printf("linedef optimization\n");
						extraData->lineDefsUsed[i] = false;
					} 
				} 
			} 
		}

		// Remove boundary walls in sectors that have 0 height, not tagged, not a door.
		for (int i = 0; i < level->SectorCount(); i++) {

			// 0 height and no tag, only need to check for d? doors
			if ((sectors[i].floorh == sectors[i].ceilh) && (sectors[i].trigger == 0)) {
				bool direct = false;
				//printf("found 0-heigh sector %d\n", i);

				// Find all linedefs used by that sector
				for ( int j = 0; j < level->LineDefCount(); j++ ) {

					// RIGHT = FRONT
					// Find linedefs conncted to that sector
					if (lineDef [j].flags & LDF_TWO_SIDED) {

						if (lineDef [j].sideDef[LEFT_SIDEDEF] == 65535) {
							// printf("Error: 1-sided linedef %d flagged as 2-sided\n", j);
							continue;
						}

						if ((sideDef [lineDef [j].sideDef[LEFT_SIDEDEF]].sector) == i) {
							int s = lineDef[j].type;
							//printf(" checking for type %d on %d\n",s, j );
							if ((s == 1) || (s == 26) || (s == 27)  || (s == 28)
									|| (s == 31) || (s == 32) || (s == 33) || (s == 34) 
									|| (s == 117) || (s == 118)) {
								direct = true;
								// printf("found conv. door\n");
								break;
							}
						}

					}
				}
				if (!direct) {
					//printf("  found sky-sector %d\n", i);
					// remove all 1-sided linedefs belonging to that sector
					for ( int j = 0; j < level->LineDefCount(); j++ ) {
						if ((sideDef [lineDef [j].sideDef[RIGHT_SIDEDEF]].sector == i) 
								&& !(lineDef[j].flags & LDF_TWO_SIDED))  {
							extraData->lineDefsUsed[j] = false;
							// printf("   found sky-sector linedef %d\n",j);
						}
					}
				}
			}
		}
	}
	// Loop through all linedefs, look at the starting vertex
	for ( int i = 0; i < level->LineDefCount(); i++ ) {
		if ( (extraData->lineDefsUsed[i] == false)) {
			// we use all blocking linedefs to make the block grid, not all vertexes.
			continue;
		}

		if ( vertex [ lineDef[i].start].x < extraData->leftMostVertex ) {
			extraData->leftMostVertex = vertex [ lineDef[i].start].x;
		}
		if ( vertex [ lineDef[i].start].x > extraData->rightMostVertex ) {
			extraData->rightMostVertex = vertex [lineDef[i].start].x;
		}
		if ( vertex [ lineDef[i].start].y < extraData->bottomVertex ) {
			extraData->bottomVertex = vertex [lineDef[i].start].y;
		}
		if ( vertex [ lineDef[i].start].y > extraData->topVertex ) {
			extraData->topVertex = vertex [ lineDef[i].start].y;
		}
	}
}

