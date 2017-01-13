//----------------------------------------------------------------------------
//
// File:        endoom.cpp
// Date:        05.12.2016
// Programmer:  Kim Roar Foldøy Hauge.
//
// Description: This module contains the logic for the ENDOOM generator.
//
// Copyright (c) 2016 Kim Roar Foldøy Hauge. All Rights Reserved.
// Based on Marc Rousseau's ZenNode 1.2.0 blockmap.cpp
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
//----------------------------------------------------------------------------


#include <string.h>

void MakeENDOOMLump(void) {

	char lump[4000];

	char logo[9][150];

	strcpy(logo [0], "      ÜßßßßßßßßßßßÜßßÜÜ  °°   °°±±±°±±± ° þÜßßßßßßßÜÜÜÜÜßßßÜßßÜßßßßÜ þ          ");
	strcpy(logo [1], "      ²ÞÛ²ÛÛÛÛÛÛßß ÛÛÜ Û  ÜÜßÜÜÜÜÜÜÜÜ  þ Ü±²ÞÛÛÛÛÛÛÜ  ÜÜÛÛÛÜÞÛÜÛÛ²ÛÜßÜ          ");
	strcpy(logo [2], "      ± ß  ÜÛÛß  ß ÞÛÛ ²Üß Üß Ü  Ü  ÜßÜÜßÜßÞ ÛÛ ÜÜßÛÝÛÛÛßþßÛ ÛÛßÜ ßÛÝ² ZokumBSP ");
	strcpy(logo [3], "      ²Üþ Û²Û  ÜÛÛÜ ÛÛ ² ÜÛÝ ÛÛ ÛÝ ÜÛÜ  ²ÛÛÞ ÛÛÜß ÜÛÝÞÛ²Ü ß  ÛÛÜßß ÛÝ± 1.0.6rc-1");
	strcpy(logo [4], "      ÞÝ ±²Û  Ûß ßÛÝ²ÛÝÜÛÛß  ÛÛ ÞÛ ²ÛÛÝ²ÛßÛÝ ²Û²ÛÛ²Ü Üß±²±Üß ²Û²Û±²ß ²          ");
	strcpy(logo [5], "      ²  ±± þÞ  þ  Ý ±°Û±Ý þÞ²Ý  ± ±ß ±Ýþ °Û ±² ÜÜß±ÝÝÜ ß°±Û ±²Ý þ Üß  WAD data ");
	strcpy(logo [6], "     ÞÝ ±  ÜÛÜß Ü °   Ý  ²      °±  Ý  ±      ±Üß Ü°ÝÞ°°ÜÜ±ÛÝ ±Ýþßß²ßþ 36 maps  ");
	strcpy(logo [7], "     ²Þ°°±±±²±Üß²²ÝÞ±±²  ²²Þ²²±²²ÝÞ±± ±±ÝÞ²²Þ°²²±²ßßÜÜß²±°ß Þ°²±Ü þ± ± 18991Kb  ");
	strcpy(logo [8], "    °²ÜÜÜÜÜÜÜÜÜßÜÜÜßÜÜÜßßÜÜÜÜÜÜÜÜßÜÜÜßÜÜÜßÜÜßÜÜÜÜÜßß  ßÜÜÜßßÜÜÜÜÜÜ² ±°          ");

	char stats[81];

	for (int line = 0; line != 25; line++) {
		for (int character = 0; character != 80; character += 2) {
			if ((line > 1) && (line < 10)) {
				lump[line * 80 + character] = logo[line -2][character];
			} else {
				lump[line * 80 + character] = 32; // space
			}
			lump[line * 80 + character + 1] = 7; // grey on black
		}
	}
}
