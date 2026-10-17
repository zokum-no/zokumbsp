unsigned int ComputeAngle(double dx, double dy);
BAM HandleAngle(const wLineDefInternal* lineDef, long dx, long dy);
const wSideDef* HandleSegRender(const wSideDef* sideDef);
bool CoLinear(const SEG* seg,int x,int y,int dx,int dy,unsigned angle);
int _WhichSide(SEG* seg, sBSPOptions* options, DoomLevel* level);
int WhichSide(SEG* seg, sBSPOptions* options, DoomLevel* level);
bool IsConvex(SEG* segs, int noSegs, nodeBuilderData* nbd);
void ComputeStaticVariables(SEG* list, int offset);