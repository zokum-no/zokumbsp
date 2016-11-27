# ZokumBSP Version: 1.0.5-rc1

## Author
(c) 2016 Kim Roar Foldøy Hauge

## Description

ZokumBSP is an advanced node/blockmap builder for Doom / Doom 2. 
Based on ZenNode 1.2.0 and available from GitHub.

This is a work in progress and is a very experimental node builder.

## Proeject goals

A node / blockmap / reject builder that allows for larger maps that
are compatible with the classic Doom dos executables and with the 
Chocolate Doom project.

## Compability

It can produce compressed blockmaps that are compatible with the 
original Id Software blockmaps that should ensure 100% demo playback
compability. It can also produce blockmaps that are compatible with
ZenNode 1.2.0 and BSP blockmaps, but with a possibly smaller size.

The Blockmap in the original 'vanilla' Doom dos executables have an
upper limit of about 65 kilobyte. Bigger blockmaps would lead to the
game crashing, putting an upper limit on the complexity and size of
the maps one can create.

By using better algorithms, some tricks and the added option of 
brute force testing up to 65k different blockmap offsets one can
generate blockmaps that are significantly smaller than what was
possible with ZenNode, BSP or Id's original node builder.

Use the -bi+ switch to build blockmaps that can replace the 
original blockmaps. The quality of compability has been tested
mostly with multiple compet-n demos of multilevel recordings. 
Primarily 30nm2039.lmp, 30ns6155.lmp and 30famax2.lmp

## Project web page

For more information about the webpage, check out the project
page coming soon! This will explain the code. math and algorithms.

## Usage

ZokumBSP Version: 1.0.5-rc1 (c) 2016 Kim Roar Foldøy Hauge
Based on: ZenNode Version 1.2.1 (c) 1994-2004 Marc Rousseau

Usage: zokumbsp {-options} filename[.wad] [level{+level}] {-o|x output[.wad]}

 -x+ turn on option   -x- turn off option  * = default

 -b[chio=0,1,2,3rsz=0,1,2]       * - Rebuild BLOCKMAP
    c              *   - Compress BLOCKMAP
    h                  - Output BLOCKMAP data as HTML.
    i                  - Id compatible BLOCKMAP. Sets 'o=1n=2' and 'c-s-r-'
    o                  - Offset configuration.
                         0 = ZenNode 0,0 offset BLOCKMAP.
                         1 = IdBSP / BSP 8,8 offset BLOCKMAP.
                   *     2 = Best of 36 offset combinations.
                         3 = Heuristic method to reduce from 65536 offsets.
                         4 = Best of all 65536 offset combinations.
    r              *   - Remove non-collidable lines from BLOCKMAP.
    s                  - Subset compress BLOCKMAP.
    z                   - Zero header configuration.
                   *     0 = No zero header.
                         1 = Conventional zero header.
                         2 = Zero footer.

 -n[a=1,2,3|q|u|i] * - Rebuild NODES
    a                   - Partition Selection Algorithm
                   *     1 = Minimize splits
                         2 = Minimize BSP depth
                         3 = Minimize time
    q                  - Don't display progress bar
    u                  - Ensure all sub-sectors contain only 1 sector
    i                  - Ignore non-visible lineDefs

 -r[zfgm]          * - Rebuild REJECT resource
    z                  - Insert empty REJECT resource
    f                  - Rebuild even if REJECT effects are detected
    g              *   - Use graphs to reduce LOS calculations
    m{b}               - Process RMB option file (.rej)

 -t                  - Don't write output file (test mode)

 level - ExMy for DOOM/Heretic or MAPxx for DOOM II/HEXEN
