#ifndef _COLOR_CONVERTOR_H_
#define _COLOR_CONVERTOR_H_

struct ENCODER_PARAM;
class CColorConvertor
{
public:
	CColorConvertor  (void);
	~CColorConvertor (void);

	bool Start     (ENCODER_PARAM * pEncoderParam);
	void Stop      ();
	bool IsRunning ();
	bool Convert   (BYTE * pSrcData, int nSrcSize, BYTE ** pDstData, int * pDstSize);

private:
	void YUYV_IYUV  (int nWidth, int nHeight, BYTE * pSrc, BYTE * pDst);
	void YVYU_IYUV  (int nWidth, int nHeight, BYTE * pSrc, BYTE * pDst);
	void UYVY_IYUV  (int nWidth, int nHeight, BYTE * pSrc, BYTE * pDst);
	void RGB24_IYUV (int nWidth, int nHeight, BYTE * pSrc, BYTE * pDst, BYTE * pTmp);
	void RGB32_IYUV (int nWidth, int nHeight, BYTE * pSrc, BYTE * pDst, BYTE * pTmp);

private:
	bool            m_bRunning;
	ENCODER_PARAM * m_pEncoderParam;

	bool            m_bNeedConvert;
	int             m_nOutSize;
	BYTE          * m_pOutBuff;
	int             m_nTmpSize;
	BYTE          * m_pTmpBuff;
};

#endif // _COLOR_CONVERTOR_H_
