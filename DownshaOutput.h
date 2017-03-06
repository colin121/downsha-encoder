#ifndef _DOWNSHA_OUTPUT_H_
#define _DOWNSHA_OUTPUT_H_

struct ENCODER_PARAM;
class CDownshaOutput
{
public:
	CDownshaOutput();
	virtual ~CDownshaOutput();

	bool Start     (ENCODER_PARAM * pEncoderParam);
	void Stop      ();
	bool IsRunning ();
	int  PushNalu  (unsigned char nMediaType, unsigned char * pData, int nSize, int nStamp, int nKeyFrame, int nCTOff);
	void * GetStat ();

private:
	bool            m_bRunning;
	ENCODER_PARAM * m_pEncoderParam;

	void          * m_pProbeCore;
	void          * m_pLiveMgmt;
	void          * m_pLiveSess;
	void          * m_pLiveStat;
};

#endif // _DOWNSHA_OUTPUT_H_