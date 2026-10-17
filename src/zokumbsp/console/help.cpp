#include <stdio.h>
#include <string>

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

#if defined ( __LINUX__ )
#include <unistd.h>
#else
// #error This platform is not supported
#endif

#include "common.hpp"
#include "logger.hpp"
#include "../bsp/zennode.hpp"

int SOURCE_PORT_LIMIT = 65536;

void PrintHelp(sOptions *config) {
	FUNCTION_ENTRY(NULL, "printHelp", true);

	fprintf(stdout, "Usage: zokumbsp {-options} filename[.wad] [level{+level}] {-o|x output[.wad]}\n");
	fprintf(stdout, "\n");
	fprintf(stdout, " -x+ turn on option   -x- turn off option  %c = default\n", DEFAULT_CHAR);
	fprintf(stdout, "\n");
	// Geometry	
	fprintf(stdout, "Switches to change level geometry and optimize geometry.\n\n");

	fprintf(stdout, "%c -g      Geometry pass, 1 suboption.\n\n", config->Geometry.Perform ? DEFAULT_CHAR : ' ');

	fprintf(stdout, "    s=    Simplify collision geometry.\n");
	fprintf(stdout, "%c     0   No simplification.\n", (config->Geometry.Simplification == 0) ? DEFAULT_CHAR : ' ');
	fprintf(stdout, "%c     1   Only if same sector.\n", (config->Geometry.Simplification == 1) ? DEFAULT_CHAR : ' ');
	fprintf(stdout, "%c     2   Also 1-sided lines in different sectors.\n", (config->Geometry.Simplification == 2) ? DEFAULT_CHAR : ' ');
	// Blockmap	
	fprintf(stdout, "\nSwitches to control BLOCKMAP strucure in a map.\n\n");

	fprintf(stdout, "%c -b      Rebuild BLOCKMAP, 8 suboptions.\n", config->BlockMap.Rebuild ? DEFAULT_CHAR : ' ');
	fprintf(stdout, "%c   b     Build big 32bit BLOCKMAP, N/A.\n", config->BlockMap.blockBig ? DEFAULT_CHAR : ' ');
	fprintf(stdout, "%c   c     Compress BLOCKMAP.\n", config->BlockMap.Compress ? DEFAULT_CHAR : ' ');
	fprintf(stdout, "%c   h     Output BLOCKMAP data as HTML.\n", config->BlockMap.HTMLOutput ? DEFAULT_CHAR : ' ');
	fprintf(stdout, "%c   i     Id-compatible BLOCKMAP. Sets 'o=1n=2g=0' and 's-r-'.\n\n", config->BlockMap.IdCompatible ? DEFAULT_CHAR : ' ');
	fprintf(stdout, "    o=    Offset configuration.\n");
	fprintf(stdout, "%c     0   ZenNode 0,0 offset BLOCKMAP.\n", config->BlockMap.OffsetZero ? DEFAULT_CHAR : ' ');
	fprintf(stdout, "%c     1   DooMBSP / BSP 8,8 offset BLOCKMAP.\n", config->BlockMap.OffsetEight ? DEFAULT_CHAR : ' ');
	fprintf(stdout, "%c     2   Best of 36 offset combinations.\n", config->BlockMap.OffsetThirtySix ? DEFAULT_CHAR : ' ');
	fprintf(stdout, "%c     3   Heuristic method to reduce from 65536 offsets.\n", config->BlockMap.OffsetHeuristic ? DEFAULT_CHAR : ' ');
	fprintf(stdout, "%c     4   Best of all 65536 offset combinations.\n", config->BlockMap.OffsetBruteForce ? DEFAULT_CHAR : ' ');
	fprintf(stdout, "%c     x,y Specify specific offsets.\n\n", config->BlockMap.OffsetUser ? DEFAULT_CHAR : ' ');

	fprintf(stdout, "%c   r     Remove non-collidable lines from BLOCKMAP.\n\n", config->BlockMap.RemoveNonCollidable ? DEFAULT_CHAR : ' ');
	fprintf(stdout, "%c   s     Subset compress BLOCKMAP.\n\n", config->BlockMap.SubBlockOptimization ? DEFAULT_CHAR : ' ');
	fprintf(stdout, "    z=    Zero header configuration.\n");
	fprintf(stdout, "%c     0   No zero header.\n", (config->BlockMap.ZeroHeader == 0) ? DEFAULT_CHAR : ' ');
	fprintf(stdout, "%c     1   Conventional zero header.\n", (config->BlockMap.ZeroHeader == 1) ? DEFAULT_CHAR : ' ');
	fprintf(stdout, "%c     2   Zero footer.\n", (config->BlockMap.ZeroHeader == 2) ? DEFAULT_CHAR : ' ');
	// Nodes
	fprintf(stdout, "\nSwitches to control BSP tree and related structures in a map.\n\n");

	fprintf(stdout, "%c -n      Rebuild NODES.\n\n", config->Nodes.Rebuild ? DEFAULT_CHAR : ' ');

	fprintf(stdout, "%c   b     Remove backside seg on on some linedefs.\n", config->BlockMap.autoDetectBacksideRemoval ? DEFAULT_CHAR : ' ');
	fprintf(stdout, "%c   c     Calculate BAM from SEGs instead of lineDefs.\n", (config->Nodes.SegBAMs == 1) ? DEFAULT_CHAR : ' ');
	fprintf(stdout, "%c   i     Ignore non-visible lineDefs.\n", config->Nodes.ReduceLineDefs ? DEFAULT_CHAR : ' ');
	fprintf(stdout, "%c   q     Don't display progress bar.\n", config->Nodes.Quiet ? DEFAULT_CHAR : ' ');
	fprintf(stdout, "%c   u     Ensure all sub-sectors contain only 1 sector.\n", config->Nodes.Unique ? DEFAULT_CHAR : ' ');
	fprintf(stdout, "    1     Alias for a=s.\n");
	fprintf(stdout, "    2     Alias for a=d.\n");
	fprintf(stdout, "    3     Alias for a=f.\n\n");

	fprintf(stdout, "    a=    Partition Selection Algorithm.\n");
	fprintf(stdout, "%c     s   Minimize splits.\n", (config->Nodes.Method == 1) ? DEFAULT_CHAR : ' ');
	fprintf(stdout, "%c     d   Minimize BSP depth.\n", (config->Nodes.Method == 2) ? DEFAULT_CHAR : ' ');
	fprintf(stdout, "%c     f   Minimize time.\n", (config->Nodes.Method == 3) ? DEFAULT_CHAR : ' ');
	fprintf(stdout, "%c     a   Adaptive tree.\n", (config->Nodes.Method == 4) ? DEFAULT_CHAR : ' ');
	fprintf(stdout, "%c     m   Multi-tree. (Slow)\n", (config->Nodes.Method == 5) ? DEFAULT_CHAR : ' ');
	fprintf(stdout, "%c     v   Vertex-tree (N/A)\n\n", (config->Nodes.Method == 6) ? DEFAULT_CHAR : ' ');

	fprintf(stdout, "    m=    Metric, what kind of node tree do you favor.\n");
	fprintf(stdout, "%c     b   Favor 2 splits = 1 subsector.\n", (config->Nodes.Metric == TREE_METRIC_BALANCED) ? DEFAULT_CHAR : ' ');
	fprintf(stdout, "%c     s   Favor fewer SEG splits.\n", (config->Nodes.Metric == TREE_METRIC_SEGS) ? DEFAULT_CHAR : ' ');
	fprintf(stdout, "%c     u   Favor fewer subsectors.\n\n", (config->Nodes.Metric == TREE_METRIC_SUBSECTORS) ? DEFAULT_CHAR : ' ');

	fprintf(stdout, "    p=    Favor certain node selection picks for depth algorithm.\n");
	fprintf(stdout, "%c     z   No favoring, use old algorithm for a balanced tree.\n", (config->Nodes.SplitReduction == 0) ? DEFAULT_CHAR : ' ');
	fprintf(stdout, "%c     s   Favor nodes that do not split segs.\n", (config->Nodes.SplitReduction == 1) ? DEFAULT_CHAR : ' ');
	fprintf(stdout, "%c     u   Favor nodes that do not create subsectors.\n", (config->Nodes.SplitReduction == 2) ? DEFAULT_CHAR : ' ');
	fprintf(stdout, "%c     b   Favor both of the above equally.\n", (config->Nodes.SplitReduction == 3) ? DEFAULT_CHAR : ' ');
	fprintf(stdout, "%c     m   Try all of the above.\n\n", (config->Nodes.MultipleSplitMethods == 1) ? DEFAULT_CHAR : ' ');

	fprintf(stdout, "    t=    Tuning factor, varies from algorithm to algorithm.\n");
	fprintf(stdout, "      100 Default for adaptive tree.\n");

	fprintf(stdout, "    w=    Number of sub trees to generate in multi-tree mode.\n");
	fprintf(stdout, "%c     2   Default width and also minimum.\n", (config->Nodes.Width == 2) ? DEFAULT_CHAR : ' ');

	// Reject
	fprintf(stdout, "\nSwitches to control REJECT resource in a map.\n\n");

	fprintf(stdout, "%c -r      Rebuild REJECT resource.\n\n", config->Reject.Rebuild ? DEFAULT_CHAR : ' ');
	fprintf(stdout, "%c   z     Insert empty REJECT resource.\n", config->Reject.Empty ? DEFAULT_CHAR : ' ');
	fprintf(stdout, "%c   f     Rebuild even if REJECT effects are detected.\n", config->Reject.Force ? DEFAULT_CHAR : ' ');
	fprintf(stdout, "%c   g     Use graphs to reduce LOS calculations.\n", config->Reject.UseGraphs ? DEFAULT_CHAR : ' ');
	fprintf(stdout, "%c   m{b}  Process RMB option file (.rej).\n", config->Reject.UseRMB ? DEFAULT_CHAR : ' ');

	fprintf(stdout, "\nSwitches to control other options.\n\n");

	fprintf(stdout, "%c -c      Enable 16 color output.\n", config->Color ? DEFAULT_CHAR : ' ');
	fprintf(stdout, "%c -cc     Enable 24bit color output.\n", config->Color ? DEFAULT_CHAR : ' ');
	fprintf(stdout, "%c -t      Don't write output file (test mode).\n\n", !config->WriteWAD ? DEFAULT_CHAR : ' ');

	// Statistics
	fprintf(stdout, "  -s=     Which output stats to show.\n");
	fprintf(stdout, "%c   e     Sector count.\n", config->Statistics.ShowSectors ? DEFAULT_CHAR : ' ');
	fprintf(stdout, "%c   l     Linedef count.\n", config->Statistics.ShowLineDefs ? DEFAULT_CHAR : ' ');
	fprintf(stdout, "%c   p     Seg split count.\n", config->Statistics.ShowSplits ? DEFAULT_CHAR : ' ');
	fprintf(stdout, "%c   n     Nodes count.\n", config->Statistics.ShowNodes ? DEFAULT_CHAR : ' ');
	fprintf(stdout, "%c   s     Subsector count.\n", config->Statistics.ShowSubSectors ? DEFAULT_CHAR : ' ');
	fprintf(stdout, "%c   t     Thing count.\n", config->Statistics.ShowThings ? DEFAULT_CHAR : ' ');
	fprintf(stdout, "%c   v     Vertices count.\n", config->Statistics.ShowVertices ? DEFAULT_CHAR : ' ');
	fprintf(stdout, "    a     Show all of the above.\n");
	fprintf(stdout, "%c   m     Show a total for all computed levels.\n", config->Statistics.ShowTotals ? DEFAULT_CHAR : ' ');
	fprintf(stdout, "    x     Count percentage by %d limit of source ports.\n", SOURCE_PORT_LIMIT);
	fprintf(stdout, "\n");

	// Misc
	fprintf(stdout, " level - ExMy for DOOM/Heretic or MAPxx for DOOM II/HEXEN.\n\n");
	fprintf(stdout, "Example: zokumbsp -b- -r- -sm -na=a file.wad map01+map03\n");
	fprintf(stdout, "This rebuild nodes only, adaptive tree, give a total summary.\n");
	fprintf(stdout, "It reads from file.wad, but only builds map01 and map03.\n");
}