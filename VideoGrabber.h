#ifndef _VIDEO_GRABBER_H_
#define _VIDEO_GRABBER_H_

struct ENCODER_PARAM;
class CVideoGrabber : public ISampleGrabberCB
{
public:
	CVideoGrabber  (void);
	~CVideoGrabber (void);

	bool Start     (ENCODER_PARAM * pEncoderParam, void * pPushProc, void * pCaller);
	void Stop      ();
	bool IsRunning ();

	virtual STDMETHODIMP BufferCB(double dblSampleTime, BYTE * pBuffer, long lBufferSize);
	virtual STDMETHODIMP SampleCB(double SampleTime, IMediaSample * pSample);

	virtual STDMETHODIMP_(ULONG) AddRef();
	virtual STDMETHODIMP_(ULONG) Release();
	virtual STDMETHODIMP QueryInterface(REFIID riid, void ** ppv);

private:
	bool            m_bRunning;
	ENCODER_PARAM * m_pEncoderParam;
	ULONG           m_ulRef;

	int             m_nFrameTotal;
	int             m_nFrameDrop;

	FILE          * m_pDumpFile;
	void          * m_pPushProc;
	void          * m_pCaller;
};

#endif // _VIDEO_GRABBER_H_
