#include "stdafx.h"
#include "VideoGrabber.h"
#include "DownshaEncoder.h"
#include "Utils.h"
#include "Log.h"

CVideoGrabber::CVideoGrabber()
{
	m_bRunning      = false;
	m_pEncoderParam = NULL;
	m_ulRef         = 0;

	m_nFrameTotal   = 0;
	m_nFrameDrop    = 0;

	m_pDumpFile     = NULL;
	m_pPushProc     = NULL;
	m_pCaller       = NULL;
}

CVideoGrabber::~CVideoGrabber()
{
	Stop();
}

bool CVideoGrabber::Start(ENCODER_PARAM * pEncoderParam, void * pPushProc, void * pCaller)
{
	if (m_bRunning)
	{
		LOG(LOG_WARNING, _T("video grabber already started"));
		return false;
	}
	m_pEncoderParam = pEncoderParam;
	m_pPushProc     = pPushProc;
	m_pCaller       = pCaller;

	m_bRunning = true;
	LOG(LOG_DEBUG, _T("video grabber started. dump raw: %s, dump avc: %s, drop raw: %s, ratio: %d/%d"),
		m_pEncoderParam->m_bVideoRawFile ? _T("yes") : _T("no"), 
		m_pEncoderParam->m_bVideoAVCFile ? _T("yes") : _T("no"),
		m_pEncoderParam->m_bVideoDropRaw ? _T("yes") : _T("no"),
		m_pEncoderParam->m_nVideoDropNum, m_pEncoderParam->m_nVideoDropDen);
	return true;
}

void CVideoGrabber::Stop()
{
	if (m_bRunning)
	{
		m_bRunning = false;
		LOG(LOG_DEBUG, _T("video grabber stopped"));
	}
	if (m_pDumpFile)
	{
		fclose(m_pDumpFile);
		m_pDumpFile = NULL;
	}
}

bool CVideoGrabber::IsRunning()
{
	return m_bRunning;
}

STDMETHODIMP CVideoGrabber::BufferCB(double dblSampleTime, BYTE * pBuffer, long lBufferSize)
{
	// The data processing thread blocks until the callback method returns.
	// If the callback does not return quickly, it can interfere with playback.
	int nSampleTime = 0;

	if (!m_bRunning || (pBuffer == NULL) || (lBufferSize <= 0))
	{
		LOG(LOG_TRACE, _T("video grabber state: %s, buffer data: %p, size: %d, IGNORED"), 
			(m_bRunning ? _T("on") : _T("off")), pBuffer, lBufferSize);
		return S_OK;
	}

#if 0
	if (lBufferSize < m_pEncoderParam->m_nVideoRawFrameSize)
	{
		LOG(LOG_TRACE, _T("video sample time: %f, buffer size: %d, expect size: %d"), 
			dblSampleTime, lBufferSize, m_pEncoderParam->m_nVideoRawFrameSize);
	}
#endif

	if (m_pEncoderParam->m_bVideoDropRaw)
	{
		m_nFrameTotal ++;
		if (m_nFrameDrop * m_pEncoderParam->m_nVideoDropDen < m_nFrameTotal * m_pEncoderParam->m_nVideoDropNum)
		{
			m_nFrameDrop ++;
			return S_OK;
		}
	}

	if (m_pEncoderParam->m_bVideoRawFile)
	{
		if (!m_pDumpFile)
		{
			char szFileName[64] = {0};
			sprintf(szFileName, "%dx%dp_%dfps.%s", 
				m_pEncoderParam->m_nVideoWidth, m_pEncoderParam->m_nVideoHeight, m_pEncoderParam->m_nVideoFPS, 
				(m_pEncoderParam->m_nVideoCSP >= ENCODER_CSP_RGB1) ? "rgb" : "yuv");
			m_pDumpFile = fopen(szFileName, "wb");
		}
		if (m_pDumpFile)
			fwrite(pBuffer, 1, lBufferSize, m_pDumpFile);
	}
	if (m_pPushProc)
	{
		if (dblSampleTime > 0.0)
		{
			// The given sample time is a double-precision number of seconds relative to start time of graph.
			nSampleTime = (int)(dblSampleTime * 1000);
		}
		else
		{
			nSampleTime = (int)(Utils::GetSystemMilliSeconds() - m_pEncoderParam->m_ullStartTime);
			if (nSampleTime < 0) nSampleTime = 0;
		}
		((ENCODER_RAW_PUSH_PROC)m_pPushProc)(m_pCaller, pBuffer, lBufferSize, nSampleTime);
	}

	return S_OK;
}

STDMETHODIMP CVideoGrabber::SampleCB(double SampleTime, IMediaSample * pSample)
{
	return S_OK;
}

STDMETHODIMP_(ULONG) CVideoGrabber::AddRef()
{
	return ++m_ulRef;
}

STDMETHODIMP_(ULONG) CVideoGrabber::Release() 
{
	ULONG ulRef = --m_ulRef;
	if (ulRef == 0) delete this;
	return ulRef;
}

STDMETHODIMP CVideoGrabber::QueryInterface(REFIID riid, void ** ppv)
{
	*ppv = NULL;
	if (riid == IID_IUnknown) *ppv = (void *)static_cast<ISampleGrabberCB*>(this);
	else if (riid == IID_ISampleGrabberCB) *ppv = (void *)static_cast<ISampleGrabberCB*>(this);

	if (*ppv != NULL) AddRef();
	return (*ppv == NULL) ? E_NOINTERFACE : S_OK;
}
