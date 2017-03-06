#include "stdafx.h"
#include "AudioGrabber.h"
#include "DownshaEncoder.h"
#include "Utils.h"
#include "Log.h"

CAudioGrabber::CAudioGrabber()
{
	m_bRunning      = false;
	m_pEncoderParam = NULL;
	m_ulRef         = 0;

	m_pDumpFile     = NULL;
	m_pPushProc     = NULL;
	m_pCaller       = NULL;
}

CAudioGrabber::~CAudioGrabber()
{
	Stop();
}

bool CAudioGrabber::Start(ENCODER_PARAM * pEncoderParam, void * pPushProc, void * pCaller)
{
	if (m_bRunning)
	{
		LOG(LOG_WARNING, _T("audio grabber already started"));
		return false;
	}
	m_pEncoderParam = pEncoderParam;
	m_pPushProc     = pPushProc;
	m_pCaller       = pCaller;

	m_bRunning = true;
	LOG(LOG_DEBUG, _T("audio grabber started. dump raw: %s, dump aac: %s"),
		m_pEncoderParam->m_bAudioRawFile ? _T("yes") : _T("no"), 
		m_pEncoderParam->m_bAudioAACFile ? _T("yes") : _T("no"));
	return true;
}

void CAudioGrabber::Stop()
{
	if (m_bRunning)
	{
		m_bRunning = false;
		LOG(LOG_DEBUG, _T("audio grabber stopped"));
	}
	if (m_pDumpFile)
	{
		fclose(m_pDumpFile);
		m_pDumpFile = NULL;
	}
}

bool CAudioGrabber::IsRunning()
{
	return m_bRunning;
}

STDMETHODIMP CAudioGrabber::BufferCB(double dblSampleTime, BYTE * pBuffer, long lBufferSize)
{
	int nSampleTime = 0;

	if (!m_bRunning || (pBuffer == NULL) || (lBufferSize <= 0))
	{
		LOG(LOG_TRACE, _T("audio grabber state: %s, buffer data: %p, size: %d, IGNORED"), 
			(m_bRunning ? _T("on") : _T("off")), pBuffer, lBufferSize);
		return S_OK;
	}

#if 0
	if (lBufferSize < m_pEncoderParam->m_nAudioRawFrameSize)
	{
		LOG(LOG_TRACE, _T("audio sample time: %f, buffer size: %d, expect size: %d"), 
			dblSampleTime, lBufferSize, m_pEncoderParam->m_nAudioRawFrameSize);
	}
#endif

	if (m_pEncoderParam->m_bAudioRawFile)
	{
		if (!m_pDumpFile)
		{
			char szFileName[64] = {0};
			sprintf(szFileName, "%d_%s.pcm", m_pEncoderParam->m_nAudioSamplesPerSec, (m_pEncoderParam->m_nAudioChannels > 1 ? "stereo" : "mono"));
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

STDMETHODIMP CAudioGrabber::SampleCB(double SampleTime, IMediaSample * pSample)
{
	return S_OK;
}

STDMETHODIMP_(ULONG) CAudioGrabber::AddRef()
{
	return ++m_ulRef;
}

STDMETHODIMP_(ULONG) CAudioGrabber::Release() 
{
	ULONG ulRef = --m_ulRef;
	if (ulRef == 0) delete this;
	return ulRef;
}

STDMETHODIMP CAudioGrabber::QueryInterface(REFIID riid, void ** ppv)
{
	*ppv = NULL;
	if (riid == IID_IUnknown) *ppv = (void *)static_cast<ISampleGrabberCB*>(this);
	else if (riid == IID_ISampleGrabberCB) *ppv = (void *)static_cast<ISampleGrabberCB*>(this);

	if (*ppv != NULL) AddRef();
	return (*ppv == NULL) ? E_NOINTERFACE : S_OK;
}
