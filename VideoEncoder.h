#ifndef _VIDEO_ENCODER_H_
#define _VIDEO_ENCODER_H_

// x264 header files
#include "stdint.h"
#include "inttypes.h"
#include "x264.h"
#include "Thread.h"

struct ENCODER_PARAM;
class CVideoEncoder : OS::CThread
{
public:
	CVideoEncoder();
	virtual ~CVideoEncoder();

	bool Start       (ENCODER_PARAM * pEncoderParam, void * pPullProc, void * pPostProc, void * pCaller);
	void Stop        ();
	bool IsRunning   ();

private:
	int  X264Open    ();
	void X264Close   ();
	int  EncodeFrame (BYTE * pFrameData, int nFrameSize, int nFrameTime);
	virtual DWORD ThreadProc();

private:
	bool             m_bRunning;
	ENCODER_PARAM  * m_pEncoderParam;
	x264_t         * m_hX264Encoder;  // X264 Encoder
	x264_picture_t * m_pX264Picture;

	FILE           * m_pDumpFile;
	void           * m_pPullProc;
	void           * m_pPostProc;
	void           * m_pCaller;
};

#endif // _VIDEO_ENCODER_H_
