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

DBG_REGISTER(__FILE__);

#define ZENVERSION              "1.2.1"
#define ZOKVERSION		"1.1.2-alpha1"
#define ZOKVERSIONSHORT		"1.1.2"



bool parseBLOCKMAPArgs(char*& ptr, bool setting) {
	FUNCTION_ENTRY(NULL, "parseBLOCKMAPArgs", true);

	config.BlockMap.Rebuild = setting;
	while (*ptr) {
		int option = *ptr++;
		bool setting = true;
		if ((*ptr == '+') || (*ptr == '-')) {
			setting = (*ptr++ == '+') ? true : false;
		}
		switch (option) {
		case 'C':
			config.BlockMap.Compress = setting;
			break;
		case 'R':
			config.BlockMap.RemoveNonCollidable = setting;
			break;
		case 'S':
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
		case 'Z':
			if (ptr[0] && ptr[1]) {
				if (ptr[1] == '0') {
					config.BlockMap.ZeroHeader = 0;
				}
				else if (ptr[1] == '1') {
					config.BlockMap.ZeroHeader = 1;
				}
				else if (ptr[1] == '2') {
					config.BlockMap.ZeroHeader = 2;
				}
				else {
					printf("error");
					return true;
				}
				ptr += 2;
			}
			break;
		case 'O':
			if (ptr[0] && ptr[1] && ptr[2] && (ptr[2] == ',')) {
				// We're given offsets, not using inbuilt
				char* comma = strchr(ptr + 1, ',');

				if (comma) {
					comma[0] = '\0';
					if (strlen(ptr + 1) && strlen(comma + 1)) {
						config.BlockMap.OffsetCommandLineX = atoi(ptr + 1);
						config.BlockMap.OffsetCommandLineY = atoi(comma + 1);
						config.BlockMap.OffsetUser = true;

						ptr += strlen(ptr + 1) + strlen(comma + 1);

						return false;
					}
					else {
						printf("error (bad offsets)");
						return true;
					}
				}
				else {
					printf("error (missing comma)");
					return true;
				}
			}
			if (ptr[0] && ptr[1]) {
				config.BlockMap.OffsetThirtySix = false;
				if (ptr[1] == '0') {
					config.BlockMap.OffsetZero = true;
				}
				else if (ptr[1] == '1') {
					config.BlockMap.OffsetEight = true;
				}
				else if (ptr[1] == '2') {
					config.BlockMap.OffsetThirtySix = true;
				}
				else if (ptr[1] == '3') {
					config.BlockMap.OffsetHeuristic = true;
				}
				else if (ptr[1] == '4') {
					config.BlockMap.OffsetBruteForce = true;
				}
				else {
					printf("error");
					return true;
				}
				ptr += 2;
			}
			break;

		default: return true;
		}
		config.BlockMap.Rebuild = true;
	}
	return false;
}