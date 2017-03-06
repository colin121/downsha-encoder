#include "stdafx.h"
#include "ColorConvertor.h"
#include "DownshaEncoder.h"
#include "Log.h"

// RGB -> YUV Conversion Factor
__declspec(align(16)) int CONVERT_Y_R[4];
__declspec(align(16)) int CONVERT_Y_G[4];
__declspec(align(16)) int CONVERT_Y_B[4];
__declspec(align(16)) int CONVERT_CB_R[4];
__declspec(align(16)) int CONVERT_CB_G[4];
__declspec(align(16)) int CONVERT_CB_B[4];
__declspec(align(16)) int CONVERT_CR_R[4];
__declspec(align(16)) int CONVERT_CR_G[4];
__declspec(align(16)) int CONVERT_CR_B[4];
__declspec(align(16)) int CONVERT_SHIFT[4];
unsigned short CONVERT_TWO[8];

CColorConvertor::CColorConvertor()
{
	m_bRunning      = false;
	m_pEncoderParam = NULL;

	m_bNeedConvert  = false;
	m_nOutSize      = 0;
	m_pOutBuff      = NULL;
	m_nTmpSize      = 0;
	m_pTmpBuff      = NULL;
}

CColorConvertor::~CColorConvertor()
{
	Stop();
}

bool CColorConvertor::Start(ENCODER_PARAM * pEncoderParam)
{
	if (m_bRunning)
	{
		LOG(LOG_WARNING, _T("color convertor already started"));
		return false;
	}
	m_pEncoderParam = pEncoderParam;

	switch (m_pEncoderParam->m_nVideoCSP)
	{
	case ENCODER_CSP_IYUV:
		{
			// VERY LUCKY! IYUV (also known as I420) is really what encoders need.
			m_bNeedConvert = false;
		}
		break;
	case ENCODER_CSP_YUYV:
	case ENCODER_CSP_YVYU:
	case ENCODER_CSP_UYVY:
		{
			// YUV Packed 4:2:2 --> YUV Planar 4:2:0
			m_nOutSize = m_pEncoderParam->m_nVideoWidth * m_pEncoderParam->m_nVideoHeight * 3 / 2;
			m_bNeedConvert = true;
		}
		break;
	case ENCODER_CSP_RGB24:
	case ENCODER_CSP_RGB32:
	case ENCODER_CSP_ARGB32:
		{
			// RGB --> YUV Planar 4:2:0
			int i = 0;
			for (i = 0; i < 4; i++)
			{
				// Y = Round(0.299 * R + 0.587 * G + 0.114 * B);
				CONVERT_Y_R[i] = (int)(0.299 * 0x10000 + 0.5);
				CONVERT_Y_G[i] = (int)(0.587 * 0x10000 + 0.5);
				CONVERT_Y_B[i] = (int)(0.114 * 0x10000 + 0.5);
				// Cb = Round(-0.1687 * R - 0.3313 * G + 0.5 * B + 128);
				CONVERT_CR_R[i] = (int)(0.5 * 0x10000 + 0.5);
				CONVERT_CR_G[i] = (int)(0.4187 * 0x10000 + 0.5);
				CONVERT_CR_B[i] = (int)(0.0813 * 0x10000 + 0.5);
				// Cr = Round(0.5 * R - 0.4187 * G - 0.0813 * B + 128);
				CONVERT_CB_R[i] = (int)(0.1687 * 0x10000 + 0.5);
				CONVERT_CB_G[i] = (int)(0.3313 * 0x10000 + 0.5);
				CONVERT_CB_B[i] = (int)(0.5 * 0x10000 + 0.5);
				CONVERT_SHIFT[i] = 128 << 16;
				CONVERT_TWO[i * 2 + 1] = CONVERT_TWO[ i * 2] = 2;
			}
			m_nOutSize = m_pEncoderParam->m_nVideoWidth * m_pEncoderParam->m_nVideoHeight * 3 / 2;
			m_nTmpSize = m_pEncoderParam->m_nVideoWidth * m_pEncoderParam->m_nVideoHeight * 2;
			m_bNeedConvert = true;
		}
		break;
	default:
		{
			LOG(LOG_ERROR, _T("unsupported video csp: %d"), m_pEncoderParam->m_nVideoCSP);
			return false;
		}
	}
	
	if (!m_pOutBuff && m_nOutSize > 0)
		m_pOutBuff = new BYTE[m_nOutSize];
	if (!m_pTmpBuff && m_nTmpSize > 0)
		m_pTmpBuff = new BYTE[m_nTmpSize];

	m_bRunning = true;
	LOG(LOG_DEBUG, _T("color convertor started. need convert: %s, type: %s"), 
		m_bNeedConvert ? _T("yes") : _T("no"), 
		(m_pEncoderParam->m_nVideoCSP == ENCODER_CSP_YUYV) ? _T("YUYV -> IYUV") : 
		(m_pEncoderParam->m_nVideoCSP == ENCODER_CSP_YVYU) ? _T("YVYU -> IYUV") : 
		(m_pEncoderParam->m_nVideoCSP == ENCODER_CSP_UYVY) ? _T("UYVY -> IYUV") : 
		(m_pEncoderParam->m_nVideoCSP == ENCODER_CSP_RGB24) ? _T("RGB24 -> IYUV") : 
		(m_pEncoderParam->m_nVideoCSP == ENCODER_CSP_RGB32) ? _T("RGB32 -> IYUV") : 
		(m_pEncoderParam->m_nVideoCSP == ENCODER_CSP_ARGB32) ? _T("ARGB32 -> IYUV") : _T("NULL"));

	return true;
}

void CColorConvertor::Stop()
{
	if (m_bRunning)
	{
		m_bRunning = false;
		LOG(LOG_DEBUG, _T("color convertor stopped"));
	}
	if (m_pOutBuff)
	{
		delete m_pOutBuff;
		m_pOutBuff = NULL;
	}
	m_nOutSize = 0;
	if (m_pTmpBuff)
	{
		delete m_pTmpBuff;
		m_pTmpBuff = NULL;
	}
	m_nTmpSize = 0;
}

bool CColorConvertor::IsRunning()
{
	return m_bRunning;
}

bool CColorConvertor::Convert(BYTE * pSrcData, int nSrcSize, BYTE ** pDstData, int * pDstSize)
{
	if (!m_bRunning || (pSrcData == NULL) || (nSrcSize <= 0))
	{
		LOG(LOG_TRACE, _T("color convertor state: %s, buffer data: %p, size: %d, IGNORED"), 
			(m_bRunning ? _T("on") : _T("off")), pSrcData, nSrcSize);
		return false;
	}

	if (!pDstData || !pDstSize)
		return false;

	if (m_bNeedConvert)
	{
		if (m_pEncoderParam->m_nVideoCSP == ENCODER_CSP_YUYV)
			YUYV_IYUV(m_pEncoderParam->m_nVideoWidth, m_pEncoderParam->m_nVideoHeight, pSrcData, m_pOutBuff);
		else if (m_pEncoderParam->m_nVideoCSP == ENCODER_CSP_YVYU)
			YVYU_IYUV(m_pEncoderParam->m_nVideoWidth, m_pEncoderParam->m_nVideoHeight, pSrcData, m_pOutBuff);
		else if (m_pEncoderParam->m_nVideoCSP == ENCODER_CSP_UYVY)
			UYVY_IYUV(m_pEncoderParam->m_nVideoWidth, m_pEncoderParam->m_nVideoHeight, pSrcData, m_pOutBuff);
		else if (m_pEncoderParam->m_nVideoCSP == ENCODER_CSP_RGB24)
			RGB24_IYUV(m_pEncoderParam->m_nVideoWidth, m_pEncoderParam->m_nVideoHeight, pSrcData, m_pOutBuff, m_pTmpBuff);
		else if (m_pEncoderParam->m_nVideoCSP == ENCODER_CSP_RGB32 || m_pEncoderParam->m_nVideoCSP == ENCODER_CSP_ARGB32)
			RGB32_IYUV(m_pEncoderParam->m_nVideoWidth, m_pEncoderParam->m_nVideoHeight, pSrcData, m_pOutBuff, m_pTmpBuff);
		*pDstData = m_pOutBuff;
		*pDstSize = m_nOutSize;
	}
	else
	{
		*pDstData = pSrcData;
		*pDstSize = nSrcSize;
	}

	return true;
}

void CColorConvertor::YUYV_IYUV(int nWidth, int nHeight, BYTE * pSrc, BYTE * pDst)
{
	int i = 0, j = 0;
	BYTE * pSrcLine1 = NULL;
	BYTE * pSrcLine2 = NULL;
	BYTE * pDstYLine1 = NULL;
	BYTE * pDstYLine2 = NULL;
	BYTE * pDstU = pDst + (nWidth * nHeight);
	BYTE * pDstV = pDstU + (nWidth * nHeight) / 4;

	// packed 4:2:2 --> planar 4:2:0
	for (i = 0; i < nHeight / 2; i++)
	{
		pSrcLine1 = pSrc + nWidth * 2 * 2 * i;
		pSrcLine2 = pSrcLine1 + nWidth * 2;

		pDstYLine1 = pDst + nWidth * 2 * i;
		pDstYLine2 = pDstYLine1 + nWidth;

		for (j = 0; j < nWidth / 2; j++)
		{
			// two pixels in one DWORD
			*pDstYLine1++ = pSrcLine1[0];
			*pDstU++      = pSrcLine1[1];
			*pDstYLine1++ = pSrcLine1[2];
			*pDstV++      = pSrcLine1[3];
			// ignore chroma of the second line
			*pDstYLine2++ = pSrcLine2[0];
			*pDstYLine2++ = pSrcLine2[2];
			pSrcLine1    += 4;
			pSrcLine2    += 4;
		}
	}
}

void CColorConvertor::YVYU_IYUV(int nWidth, int nHeight, BYTE * pSrc, BYTE * pDst)
{
	int i = 0, j = 0;
	BYTE * pSrcLine1 = NULL;
	BYTE * pSrcLine2 = NULL;
	BYTE * pDstYLine1 = NULL;
	BYTE * pDstYLine2 = NULL;
	BYTE * pDstU = pDst + (nWidth * nHeight);
	BYTE * pDstV = pDstU + (nWidth * nHeight) / 4;

	// packed 4:2:2 --> planar 4:2:0
	for (i = 0; i < nHeight / 2; i++)
	{
		pSrcLine1 = pSrc + nWidth * 2 * 2 * i;
		pSrcLine2 = pSrcLine1 + nWidth * 2;

		pDstYLine1 = pDst + nWidth * 2 * i;
		pDstYLine2 = pDstYLine1 + nWidth;

		for (j = 0; j < nWidth / 2; j++)
		{
			// two pixels in one DWORD
			*pDstYLine1++ = pSrcLine1[0];
			*pDstV++      = pSrcLine1[1];
			*pDstYLine1++ = pSrcLine1[2];
			*pDstU++      = pSrcLine1[3];
			// ignore chroma of the second line
			*pDstYLine2++ = pSrcLine2[0];
			*pDstYLine2++ = pSrcLine2[2];
			pSrcLine1    += 4;
			pSrcLine2    += 4;
		}
	}
}

void CColorConvertor::UYVY_IYUV(int nWidth, int nHeight, BYTE * pSrc, BYTE * pDst)
{
	int i = 0, j = 0;
	BYTE * pSrcLine1 = NULL;
	BYTE * pSrcLine2 = NULL;
	BYTE * pDstYLine1 = NULL;
	BYTE * pDstYLine2 = NULL;
	BYTE * pDstU = pDst + (nWidth * nHeight);
	BYTE * pDstV = pDstU + (nWidth * nHeight) / 4;

	// packed 4:2:2 --> planar 4:2:0
	for (i = 0; i < nHeight / 2; i++)
	{
		pSrcLine1 = pSrc + nWidth * 2 * 2 * i;
		pSrcLine2 = pSrcLine1 + nWidth * 2;

		pDstYLine1 = pDst + nWidth * 2 * i;
		pDstYLine2 = pDstYLine1 + nWidth;

		for (j = 0; j < nWidth / 2; j++)
		{
			// two pixels in one DWORD
			*pDstU++      = pSrcLine1[0];
			*pDstYLine1++ = pSrcLine1[1];
			*pDstV++      = pSrcLine1[2];
			*pDstYLine1++ = pSrcLine1[3];
			// ignore chroma of the second line
			*pDstYLine2++ = pSrcLine2[1];
			*pDstYLine2++ = pSrcLine2[3];
			pSrcLine1    += 4;
			pSrcLine2    += 4;
		}
	}
}

/*****************************************************************************
 * CColorConvertor::RGB24_IYUV(): transform RGB24 to YUV 4:2:0 planar.
 * NOTE: SSE4 Instructions is used for fast conversion.
 * MAKE SURE your CPU has SSE4, otherwise this function will crash.
 * For RGB 24, every pixel is an RGBTRIPLE. Each color is one byte, with a value from 0 to 255, inclusive.
 * The memory layout is:
 * Byte   0     1       2
 * Value  Blue  Green   Red
 *****************************************************************************/
void CColorConvertor::RGB24_IYUV(int nWidth, int nHeight, BYTE * pSrc, BYTE * pDst, BYTE * pTmp)
{
	BYTE * pRGBCursor = NULL;
	BYTE * pYCursor   = pDst; // Save Luma(Y) directly
	BYTE * pUCursor   = pYCursor + (nWidth * nHeight);
	BYTE * pVCursor   = pUCursor + ((nWidth * nHeight) >> 2);

	BYTE * pChroma    = pTmp;    // Save Chroma(Cb and Cr) temporarily
	BYTE * pCbCursor  = pChroma;
	BYTE * pCrCursor  = pCbCursor + nWidth * nHeight;
	int i = 0, j = 0;

	// RGB --> YCbCr Conversion in Bottom-Up mode to flip raw image.
	for (i = nHeight - 1; i >= 0; i--)
	{
		pRGBCursor = pSrc + (nWidth * i * 3);
		// 4 pixels (12 bytes) each time. source byte order: BGR BGR BGR BGR
		for (j = 0; j < nWidth * 3; j += 12)
		{
			__asm
			{
				mov eax, pRGBCursor;
				pxor xmm0, xmm0;
				pxor xmm1, xmm1;
				pxor xmm2, xmm2;
				// xmm0 : R
				PinsrB xmm0, [eax][2 + 3 * 0], 0;
				PinsrB xmm0, [eax][2 + 3 * 1], 4;
				PinsrB xmm0, [eax][2 + 3 * 2], 8;
				PinsrB xmm0, [eax][2 + 3 * 3], 12;
				// xmm1: G
				PinsrB xmm1, [eax][1 + 3 * 0], 0;
				PinsrB xmm1, [eax][1 + 3 * 1], 4;
				PinsrB xmm1, [eax][1 + 3 * 2], 8;
				PinsrB xmm1, [eax][1 + 3 * 3], 12;
				// xmm2: B
				PinsrB xmm2, [eax][0 + 3 * 0], 0;
				PinsrB xmm2, [eax][0 + 3 * 1], 4;
				PinsrB xmm2, [eax][0 + 3 * 2], 8;
				PinsrB xmm2, [eax][0 + 3 * 3], 12;
				// Y = Round(0.299 * R + 0.587 * G + 0.114 * B);
				movdqa xmm3, xmm0;
				PMulld xmm3, CONVERT_Y_R;
				movdqa xmm4, xmm1;
				PMulld xmm4, CONVERT_Y_G;
				PAddd xmm3, xmm4;
				movdqa xmm4, xmm2;
				PMulld xmm4, CONVERT_Y_B;
				PAddd xmm3, xmm4;
				mov eax, pYCursor;
				PSrld xmm3, 16;
				Packuswb xmm3, xmm3;
				Packuswb xmm3, xmm3;
				movd [eax], xmm3;
				// Cb = Round(-0.1687 * R - 0.3313 * G + 0.5 * B + 128);
				movdqa xmm3, xmm2;
				PMulld xmm3, CONVERT_CB_B;
				PAddd xmm3, CONVERT_SHIFT;
				movdqa xmm4, xmm0;
				PMulld xmm4, CONVERT_CB_R;
				PSubd xmm3, xmm4;
				movdqa xmm4, xmm1;
				PMulld xmm4, CONVERT_CB_G;
				PSubd xmm3, xmm4;
				mov eax, pCbCursor;			
				PSrad xmm3, 16;
				Packuswb xmm3, xmm3;
				Packuswb xmm3, xmm3;
				movd [eax], xmm3;
				// Cr = Round(0.5 * R - 0.4187 * G - 0.0813 * B + 128);
				movdqa xmm3, xmm0;
				PMulld xmm3, CONVERT_CR_R;
				PAddd xmm3, CONVERT_SHIFT;
				movdqa xmm4, xmm1;
				PMulld xmm4, CONVERT_CR_G;
				PSubd xmm3, xmm4;
				movdqa xmm4, xmm2;
				PMulld xmm4, CONVERT_CR_B;
				PSubd xmm3, xmm4;
				mov eax, pCrCursor;
				PSrld xmm3, 16;
				Packuswb xmm3, xmm3;
				Packuswb xmm3, xmm3;
				movd [eax], xmm3;
			}
			pRGBCursor += 12;
			pYCursor   += 4;
			pCbCursor  += 4;
			pCrCursor  += 4;
		}
	}

	// Chroma 2x2 sub-sample
	__asm{ movq mm7, CONVERT_TWO};
	for (i = 0; i < nHeight; i += 2)
	{
		// Source: Cb and Cr plane
		pCbCursor = pChroma + nWidth * i;
		pCrCursor = pCbCursor + nWidth * nHeight;

		for (j = 0; j < nWidth; j += 8)
		{
			__asm
			{
				// 16 bytes Cb --> 4 bytes U
				mov eax, pCbCursor;
				PMovzxBW xmm0, [eax];
				add eax, nWidth;
				PMovzxBW xmm1, [eax];
				PAddw xmm0, xmm1;
				PHAddw xmm0, xmm0;
				MOVDQ2Q mm0, xmm0;
				PAddw mm0, mm7;
				PSrlw mm0, 2;
				Packuswb mm0, mm0;
				mov eax, pUCursor;
				movd [eax], mm0;
				// 16 bytes Cr --> 4 bytes V
				mov eax, pCrCursor;
				PMovzxBW xmm0, [eax];
				add eax, nWidth;
				PMovzxBW xmm1, [eax];
				PAddw xmm0, xmm1;
				PHAddw xmm0, xmm0;
				MOVDQ2Q mm0, xmm0;
				PAddw mm0, mm7;
				PSrlw mm0, 2;
				Packuswb mm0, mm0;
				mov eax, pVCursor;
				movd [eax], mm0;
			}
			pCbCursor += 8;
			pCrCursor += 8;
			pUCursor  += 4;
			pVCursor  += 4;
		}
	}
	__asm{ emms };
}

/*****************************************************************************
 * CColorConvertor::RGB32_IYUV(): transform RGB32 to YUV 4:2:0 planar.
 * NOTE: SSE4 Instructions is used for fast conversion.
 * MAKE SURE your CPU has SSE4, otherwise this function will crash.
 * For RGB 32, every pixel is an RGBQUAD. Each color is one byte, with a value from 0 to 255, inclusive.
 * The memory layout is:
 * Byte   0     1       2      3
 * Value  Blue  Green   Red    Alpha or Don't Care
 *****************************************************************************/
void CColorConvertor::RGB32_IYUV(int nWidth, int nHeight, BYTE * pSrc, BYTE * pDst, BYTE * pTmp)
{
	BYTE * pRGBCursor = NULL;
	BYTE * pYCursor   = pDst; // Save Luma(Y) directly
	BYTE * pUCursor   = pYCursor + (nWidth * nHeight);
	BYTE * pVCursor   = pUCursor + ((nWidth * nHeight) >> 2);

	BYTE * pChroma    = pTmp;    // Save Chroma(Cb and Cr) temporarily
	BYTE * pCbCursor  = pChroma;
	BYTE * pCrCursor  = pCbCursor + nWidth * nHeight;
	int i = 0, j = 0;

	// RGB --> YCbCr Conversion in Bottom-Up mode to flip raw image.
	for (i = nHeight - 1; i >= 0; i--)
	{
		pRGBCursor = pSrc + (nWidth * i * 4);
		// 4 pixels (16 bytes) each time. source byte order: BGRA BGRA BGRA BGRA
		for (j = 0; j < nWidth * 4; j += 16)
		{
			__asm
			{
				mov eax, pRGBCursor;
				pxor xmm0, xmm0;
				pxor xmm1, xmm1;
				pxor xmm2, xmm2;
				// xmm0 : R
				PinsrB xmm0, [eax][2 + 4 * 0], 0;
				PinsrB xmm0, [eax][2 + 4 * 1], 4;
				PinsrB xmm0, [eax][2 + 4 * 2], 8;
				PinsrB xmm0, [eax][2 + 4 * 3], 12;
				// xmm1: G
				PinsrB xmm1, [eax][1 + 4 * 0], 0;
				PinsrB xmm1, [eax][1 + 4 * 1], 4;
				PinsrB xmm1, [eax][1 + 4 * 2], 8;
				PinsrB xmm1, [eax][1 + 4 * 3], 12;
				// xmm2: B
				PinsrB xmm2, [eax][0 + 4 * 0], 0;
				PinsrB xmm2, [eax][0 + 4 * 1], 4;
				PinsrB xmm2, [eax][0 + 4 * 2], 8;
				PinsrB xmm2, [eax][0 + 4 * 3], 12;
				// Y = Round(0.299 * R + 0.587 * G + 0.114 * B);
				movdqa xmm3, xmm0;
				PMulld xmm3, CONVERT_Y_R;
				movdqa xmm4, xmm1;
				PMulld xmm4, CONVERT_Y_G;
				PAddd xmm3, xmm4;
				movdqa xmm4, xmm2;
				PMulld xmm4, CONVERT_Y_B;
				PAddd xmm3, xmm4;
				mov eax, pYCursor;
				PSrld xmm3, 16;
				Packuswb xmm3, xmm3;
				Packuswb xmm3, xmm3;
				movd [eax], xmm3;
				// Cb = Round(-0.1687 * R - 0.3313 * G + 0.5 * B + 128);
				movdqa xmm3, xmm2;
				PMulld xmm3, CONVERT_CB_B;
				PAddd xmm3, CONVERT_SHIFT;
				movdqa xmm4, xmm0;
				PMulld xmm4, CONVERT_CB_R;
				PSubd xmm3, xmm4;
				movdqa xmm4, xmm1;
				PMulld xmm4, CONVERT_CB_G;
				PSubd xmm3, xmm4;
				mov eax, pCbCursor;			
				PSrad xmm3, 16;
				Packuswb xmm3, xmm3;
				Packuswb xmm3, xmm3;
				movd [eax], xmm3;
				// Cr = Round(0.5 * R - 0.4187 * G - 0.0813 * B + 128);
				movdqa xmm3, xmm0;
				PMulld xmm3, CONVERT_CR_R;
				PAddd xmm3, CONVERT_SHIFT;
				movdqa xmm4, xmm1;
				PMulld xmm4, CONVERT_CR_G;
				PSubd xmm3, xmm4;
				movdqa xmm4, xmm2;
				PMulld xmm4, CONVERT_CR_B;
				PSubd xmm3, xmm4;
				mov eax, pCrCursor;
				PSrld xmm3, 16;
				Packuswb xmm3, xmm3;
				Packuswb xmm3, xmm3;
				movd [eax], xmm3;
			}
			pRGBCursor += 16;
			pYCursor   += 4;
			pCbCursor  += 4;
			pCrCursor  += 4;
		}
	}

	// Chroma 2x2 sub-sample
	__asm{ movq mm7, CONVERT_TWO};
	for (i = 0; i < nHeight; i += 2)
	{
		// Source: Cb and Cr plane
		pCbCursor = pChroma + nWidth * i;
		pCrCursor = pCbCursor + nWidth * nHeight;

		for (j = 0; j < nWidth; j += 8)
		{
			__asm
			{
				// 16 bytes Cb --> 4 bytes U
				mov eax, pCbCursor;
				PMovzxBW xmm0, [eax];
				add eax, nWidth;
				PMovzxBW xmm1, [eax];
				PAddw xmm0, xmm1;
				PHAddw xmm0, xmm0;
				MOVDQ2Q mm0, xmm0;
				PAddw mm0, mm7;
				PSrlw mm0, 2;
				Packuswb mm0, mm0;
				mov eax, pUCursor;
				movd [eax], mm0;
				// 16 bytes Cr --> 4 bytes V
				mov eax, pCrCursor;
				PMovzxBW xmm0, [eax];
				add eax, nWidth;
				PMovzxBW xmm1, [eax];
				PAddw xmm0, xmm1;
				PHAddw xmm0, xmm0;
				MOVDQ2Q mm0, xmm0;
				PAddw mm0, mm7;
				PSrlw mm0, 2;
				Packuswb mm0, mm0;
				mov eax, pVCursor;
				movd [eax], mm0;
			}
			pCbCursor += 8;
			pCrCursor += 8;
			pUCursor  += 4;
			pVCursor  += 4;
		}
	}
	__asm{ emms };
}
