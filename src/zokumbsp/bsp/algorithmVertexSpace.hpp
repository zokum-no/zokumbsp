int AlgorithmVertex(SEG* segs, int noSegs, nodeBuilderData* nbd);
void FindConcaveCorners(SEG* segs, int noSegs, vertexSpace* vs, int vertices, nodeBuilderData* nbd);
void ComputeQuality(int* quality, int noSegs, nodeBuilderData* nbd);
void FindConcaveCorners(SEG* segs, int noSegs, vertexSpace* vs, int vertices, nodeBuilderData* nbd);
int GenerateVertexSpace(vertexSpace* vs, SEG* segs, int noSegs);
int PartitionLineGoodSeg(SEG* segs, int noSegs, nodeBuilderData* nbd, int maxSplits, int maxSubSectors);
int PartitionLineLastResort(SEG* segs, int noSegs, vertexSpace* vs, int vertices, nodeBuilderData* nbd, int convexMode);