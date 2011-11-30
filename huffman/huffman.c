#include <string.h>
#include "huffman.h"

static void HuffmanBubbleSort(HuffmanConstructNode **ppList, int Size);

void HuffmanSetbits_r(Huffman *hf, HuffmanNode *pNode, int Bits, unsigned Depth)
{
	if(pNode->m_aLeafs[1] != 0xffff)
		HuffmanSetbits_r(hf, &hf->m_aNodes[pNode->m_aLeafs[1]], Bits|(1<<Depth), Depth+1);
	if(pNode->m_aLeafs[0] != 0xffff)
		HuffmanSetbits_r(hf, &hf->m_aNodes[pNode->m_aLeafs[0]], Bits, Depth+1);

	if(pNode->m_NumBits)
	{
		pNode->m_Bits = Bits;
		pNode->m_NumBits = Depth;
	}
}

static void HuffmanBubbleSort(HuffmanConstructNode **ppList, int Size)
{
	int Changed = 1;
	int i;
	HuffmanConstructNode *pTemp;

	while(Changed)
	{
		Changed = 0;
		for(i = 0; i < Size-1; i++)
		{
			if(ppList[i]->m_Frequency < ppList[i+1]->m_Frequency)
			{
				pTemp = ppList[i];
				ppList[i] = ppList[i+1];
				ppList[i+1] = pTemp;
				Changed = 1;
			}
		}
		Size--;
	}
}

void HuffmanConstructTree(Huffman *hf)
{
	HuffmanConstructNode aNodesLeftStorage[HUFFMAN_MAX_SYMBOLS];
	HuffmanConstructNode *apNodesLeft[HUFFMAN_MAX_SYMBOLS];
	int NumNodesLeft = HUFFMAN_MAX_SYMBOLS;
	int i;

	/* add the symbols */
	for(i = 0; i < HUFFMAN_MAX_SYMBOLS; i++)
	{
		hf->m_aNodes[i].m_NumBits = 0xFFFFFFFF;
		hf->m_aNodes[i].m_Symbol = i;
		hf->m_aNodes[i].m_aLeafs[0] = 0xffff;
		hf->m_aNodes[i].m_aLeafs[1] = 0xffff;

		if(i == HUFFMAN_EOF_SYMBOL)
			aNodesLeftStorage[i].m_Frequency = 1;
		else
			aNodesLeftStorage[i].m_Frequency = HuffmanFreqTable[i];
		aNodesLeftStorage[i].m_NodeId = i;
		apNodesLeft[i] = &aNodesLeftStorage[i];

	}

	hf->m_NumNodes = HUFFMAN_MAX_SYMBOLS;

	/* construct the table */
	while(NumNodesLeft > 1)
	{
		/* we can't rely on stdlib's qsort for this, it can generate different results on different implementations */
		HuffmanBubbleSort(apNodesLeft, NumNodesLeft);

		hf->m_aNodes[hf->m_NumNodes].m_NumBits = 0;
		hf->m_aNodes[hf->m_NumNodes].m_aLeafs[0] = apNodesLeft[NumNodesLeft-1]->m_NodeId;
		hf->m_aNodes[hf->m_NumNodes].m_aLeafs[1] = apNodesLeft[NumNodesLeft-2]->m_NodeId;
		apNodesLeft[NumNodesLeft-2]->m_NodeId = hf->m_NumNodes;
		apNodesLeft[NumNodesLeft-2]->m_Frequency = apNodesLeft[NumNodesLeft-1]->m_Frequency + apNodesLeft[NumNodesLeft-2]->m_Frequency;

		hf->m_NumNodes++;
		NumNodesLeft--;
	}

	/* set start node */
	hf->m_pStartNode = &hf->m_aNodes[hf->m_NumNodes-1];

	/* build symbol bits */
	HuffmanSetbits_r(hf, hf->m_pStartNode, 0, 0);
}

void HuffmanInit(Huffman *hf)
{
	int i;

	/* make sure to cleanout every thing */
	memset(hf, 0, sizeof(*hf));

	/* construct the tree */
	HuffmanConstructTree(hf);

	/* build decode LUT */
	for(i = 0; i < HUFFMAN_LUTSIZE; i++)
	{
		unsigned Bits = i;
		int k;
		HuffmanNode *pNode = hf->m_pStartNode;
		for(k = 0; k < HUFFMAN_LUTBITS; k++)
		{
			pNode = &hf->m_aNodes[pNode->m_aLeafs[Bits&1]];
			Bits >>= 1;

			if(!pNode)
				break;

			if(pNode->m_NumBits)
			{
				hf->m_apDecodeLut[i] = pNode;
				break;
			}
		}

		if(k == HUFFMAN_LUTBITS)
			hf->m_apDecodeLut[i] = pNode;
	}
}

int HuffmanDecompress(Huffman *hf, const void *pInput, int InputSize, void *pOutput, int OutputSize)
{
	/* setup buffer pointers */
	unsigned char *pDst = (unsigned char *)pOutput;
	unsigned char *pSrc = (unsigned char *)pInput;
	unsigned char *pDstEnd = pDst + OutputSize;
	unsigned char *pSrcEnd = pSrc + InputSize;

	unsigned Bits = 0;
	unsigned Bitcount = 0;

	HuffmanNode *pEof = &hf->m_aNodes[HUFFMAN_EOF_SYMBOL];
	HuffmanNode *pNode = 0;

	while(1)
	{
		/* {A} try to load a node now, this will reduce dependency at location {D} */
		pNode = 0;
		if(Bitcount >= HUFFMAN_LUTBITS)
			pNode = hf->m_apDecodeLut[Bits&HUFFMAN_LUTMASK];

		/* {B} fill with new bits */
		while(Bitcount < 24 && pSrc != pSrcEnd)
		{
			Bits |= (*pSrc++) << Bitcount;
			Bitcount += 8;
		}

		/* {C} load symbol now if we didn't that earlier at location {A} */
		if(!pNode)
			pNode = hf->m_apDecodeLut[Bits&HUFFMAN_LUTMASK];

		if(!pNode)
			return -1;

		/* {D} check if we hit a symbol already */
		if(pNode->m_NumBits)
		{
			/* remove the bits for that symbol */
			Bits >>= pNode->m_NumBits;
			Bitcount -= pNode->m_NumBits;
		}
		else
		{
			/* remove the bits that the lut checked up for us */
			Bits >>= HUFFMAN_LUTBITS;
			Bitcount -= HUFFMAN_LUTBITS;

			/* walk the tree bit by bit */
			while(1)
			{
				/* traverse tree */
				pNode = &hf->m_aNodes[pNode->m_aLeafs[Bits&1]];

				/* remove bit */
				Bitcount--;
				Bits >>= 1;

				/* check if we hit a symbol */
				if(pNode->m_NumBits)
					break;

				/* no more bits, decoding error */
				if(Bitcount == 0)
					return -1;
			}
		}

		/* check for eof */
		if(pNode == pEof)
			break;

		/* output character */
		if(pDst == pDstEnd)
			return -1;
		*pDst++ = pNode->m_Symbol;
	}

	/* return the size of the decompressed buffer */
	return (int)(pDst - (const unsigned char *)pOutput);
}
