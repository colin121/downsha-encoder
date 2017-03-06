#ifndef _AUDIO_GRABBER_H_
#define _AUDIO_GRABBER_H_

struct ENCODER_PARAM;
class CAudioGrabber : public ISampleGrabberCB
{
public:
	CAudioGrabber(void);
	~CAudioGrabber(void);

	bool Start     (ENCODER_PARAM * pEncoderParam, void * pPushProc, void * pCaller);
	void Stop      ();
	bool IsRunning ();

	virtual STDMETHODIMP BufferCB(double dblSampleTime, BYTE * pBuffer, long lBufferSize);
	virtual STDMETHODIMP SampleCB(double SampleTime, IMediaSample * pSample);

	virtual STDMETHODIMP_(ULONG) AddRef();
	virtual STDMETHODIMP_(ULONG) Release();
	virtual STDMETHODIMP QueryInterface(REFIID riid, void ** ppv);

public:
	bool            m_bRunning;
	ENCODER_PARAM * m_pEncoderParam;
	ULONG           m_ulRef;

	FILE          * m_pDumpFile;
	void          * m_pPushProc;
	void          * m_pCaller;
};

#endif // _AUDIO_GRABBER_H_
