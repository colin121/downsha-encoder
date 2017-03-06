#ifndef _AUDIO_ENCODER_H_
#define _AUDIO_ENCODER_H_

// faac header files
#include "faac.h"
#include "Thread.h"

struct ENCODER_PARAM;
class CAudioEncoder : OS::CThread
{
public:
	CAudioEncoder();
	virtual ~CAudioEncoder();

	bool Start       (ENCODER_PARAM * pEncoderParam, void * pPullProc, void * pPostProc, void * pCaller);
	void Stop        ();
	bool IsRunning   ();

private:
	bool FaacOpen    ();
	void FaacClose   ();
	int  EncodeFrame (BYTE * pFrameData, int nFrameSize, int nFrameTime);
	virtual DWORD ThreadProc();

private:
	bool            m_bRunning;
	ENCODER_PARAM * m_pEncoderParam;

	faacEncHandle   m_hFaacEncoder;
	unsigned long   m_ulInputSamples;
	unsigned long   m_ulMaxOutputBytes;
	BYTE          * m_pOutputBuffer;

	FILE          * m_pDumpFile;
	void          * m_pPullProc;
	void          * m_pPostProc;
	void          * m_pCaller;
};

#endif // _AUDIO_ENCODER_H_
