enum
{
	HUFFMAN_EOF_SYMBOL = 256,

	HUFFMAN_MAX_SYMBOLS=HUFFMAN_EOF_SYMBOL+1,
	HUFFMAN_MAX_NODES=HUFFMAN_MAX_SYMBOLS*2-1,

	HUFFMAN_LUTBITS = 10,
	HUFFMAN_LUTSIZE = (1<<HUFFMAN_LUTBITS),
	HUFFMAN_LUTMASK = (HUFFMAN_LUTSIZE-1)
};

typedef struct
{
	/* symbol */
	unsigned m_Bits;
	unsigned m_NumBits;

	/* don't use pointers for this. shorts are smaller so we can fit more data into the cache */
	unsigned short m_aLeafs[2];

	/* what the symbol represents */
	unsigned char m_Symbol;
} HuffmanNode;

typedef struct {
	HuffmanNode m_aNodes[HUFFMAN_MAX_NODES];
	HuffmanNode *m_apDecodeLut[HUFFMAN_LUTSIZE];
	HuffmanNode *m_pStartNode;
	int m_NumNodes;
} Huffman;

typedef struct {
	unsigned short m_NodeId;
 	int m_Frequency;
} HuffmanConstructNode;

void HuffmanSetbits_r(Huffman *hf, HuffmanNode *pNode, int Bits, unsigned Depth);
void HuffmanConstructTree(Huffman *hf);
void HuffmanInit(Huffman *hf);
int HuffmanDecompress(Huffman *hf, const void *pInput, int InputSize, void *pOutput, int OutputSize);


static const unsigned HuffmanFreqTable[256+1] = {
	1<<30,4545,2657,431,1950,919,444,482,2244,617,838,542,715,1814,304,240,754,212,647,186,
	283,131,146,166,543,164,167,136,179,859,363,113,157,154,204,108,137,180,202,176,
	872,404,168,134,151,111,113,109,120,126,129,100,41,20,16,22,18,18,17,19,
	16,37,13,21,362,166,99,78,95,88,81,70,83,284,91,187,77,68,52,68,
	59,66,61,638,71,157,50,46,69,43,11,24,13,19,10,12,12,20,14,9,
	20,20,10,10,15,15,12,12,7,19,15,14,13,18,35,19,17,14,8,5,
	15,17,9,15,14,18,8,10,2173,134,157,68,188,60,170,60,194,62,175,71,
	148,67,167,78,211,67,156,69,1674,90,174,53,147,89,181,51,174,63,163,80,
	167,94,128,122,223,153,218,77,200,110,190,73,174,69,145,66,277,143,141,60,
	136,53,180,57,142,57,158,61,166,112,152,92,26,22,21,28,20,26,30,21,
	32,27,20,17,23,21,30,22,22,21,27,25,17,27,23,18,39,26,15,21,
	12,18,18,27,20,18,15,19,11,17,33,12,18,15,19,18,16,26,17,18,
	9,10,25,22,22,17,20,16,6,16,15,20,14,18,24,335,1517};

