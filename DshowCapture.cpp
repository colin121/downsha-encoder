#include "stdafx.h"
#include "DshowCapture.h"
#include "DownshaEncoder.h"
#include "Utils.h"
#include "Log.h"

CDshowCapture::CDshowCapture()
{
	m_pEncoderParam        = NULL;
	m_bRunning             = false;
	m_pGraphBuilder        = NULL;
	m_pCaptureGraphBuilder = NULL;
	m_pMediaControl        = NULL;

	m_pVideoDeviceFilter   = NULL;
	m_pVideoGrabberFilter  = NULL;
	m_pVideoGrabber        = NULL;
	m_pVideoGrabberCB      = NULL;
	m_pVideoMediaType      = NULL;

	m_pAudioDeviceFilter   = NULL;
	m_pAudioGrabberFilter  = NULL;
	m_pAudioGrabber        = NULL;
	m_pAudioGrabberCB      = NULL;
	m_pAudioMediaType      = NULL;
}

CDshowCapture::~CDshowCapture()
{
	Stop();
}

bool CDshowCapture::Start(ENCODER_PARAM * pEncoderParam, ISampleGrabberCB * pVideoGrabberCB, ISampleGrabberCB * pAudioGrabberCB)
{
	HRESULT           hr;
	IVideoWindow    * pVideoWindow    = NULL;

	if (m_bRunning)
	{
		LOG(LOG_WARNING, _T("dshow capture already started"));
		return false;
	}
	m_pEncoderParam   = pEncoderParam;
	/************************************************************************/
	/* WARNING: DO NOT call AddRef() or Release() for grabber callback.     */
	/* DirectShow will call AddRef() in the ISampleGrabber::SetCallback(),  */
	/* and call Release() in the graph destruction internally.              */
	/************************************************************************/
	m_pVideoGrabberCB = pVideoGrabberCB;
	m_pAudioGrabberCB = pAudioGrabberCB;
	LOG(LOG_DEBUG, _T("dshow capture initializing"));

	// Setup filter graph builder and capture graph builder
	hr = ::CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, IID_IGraphBuilder, (void **)&m_pGraphBuilder);
	if (FAILED(hr) || !m_pGraphBuilder)
	{
		LOG(LOG_ERROR, _T("CoCreateInstance CLSID_FilterGraph failed. Error: %08X"), hr);
		return false;
	}
	hr = ::CoCreateInstance(CLSID_CaptureGraphBuilder2, NULL, CLSCTX_INPROC_SERVER, IID_ICaptureGraphBuilder2, (void**)&m_pCaptureGraphBuilder);
	if (FAILED(hr) || !m_pCaptureGraphBuilder)
	{
		LOG(LOG_ERROR, _T("CoCreateInstance CLSID_CaptureGraphBuilder2 failed. Error: %08X"), hr);
		return false;
	}
	// Associate the filter graph with the capture graph builder
	hr = m_pCaptureGraphBuilder->SetFiltergraph(m_pGraphBuilder);
	if (FAILED(hr))
	{
		LOG(LOG_ERROR, _T("SetFiltergraph failed. Error: %08X"), hr);
		return false;
	}
	hr = m_pGraphBuilder->QueryInterface(IID_IMediaControl, (void**)&m_pMediaControl);
	if (FAILED(hr) || !m_pMediaControl)
	{
		LOG(LOG_ERROR, _T("QueryInterface IID_IMediaControl failed. Error: %08X"), hr);
		return false;
	}

	// Setup video capture stream
	if (m_pEncoderParam->m_bVideoEnable)
	{
		if (!BuildVideoDevice())
		{
			LOG(LOG_ERROR, _T("BuildVideoDevice failed"));
			return false;
		}
		if (!BuildVideoGrabber())
		{
			LOG(LOG_ERROR, _T("BuildVideoGrabber failed"));
			return false;
		}
		hr = m_pCaptureGraphBuilder->RenderStream(&PIN_CATEGORY_CAPTURE, 
			&MEDIATYPE_Video, m_pVideoDeviceFilter, m_pVideoGrabberFilter, NULL);
		if (FAILED(hr))
		{
			LOG(LOG_ERROR, _T("RenderStream video failed. Error: %08X"), hr);
			return false;
		}
		if (!m_pEncoderParam->m_bVideoPreview)
		{
			hr = m_pGraphBuilder->QueryInterface(IID_IVideoWindow, (LPVOID*)&pVideoWindow);
			if (SUCCEEDED(hr))
			{
				pVideoWindow->put_AutoShow(OAFALSE); // hide video window
				pVideoWindow->Release();
				pVideoWindow = NULL;
			}
		}
	}

	// Setup audio capture stream
	if (m_pEncoderParam->m_bAudioEnable)
	{
		if (!BuildAudioDevice())
		{
			LOG(LOG_ERROR, _T("BuildAudioDevice failed"));
			return false;
		}
		if (!BuildAudioGrabber())
		{
			LOG(LOG_ERROR, _T("BuildAudioGrabber failed"));
			return false;
		}
		hr = m_pCaptureGraphBuilder->RenderStream(&PIN_CATEGORY_CAPTURE, 
			&MEDIATYPE_Audio, m_pAudioDeviceFilter, m_pAudioGrabberFilter, NULL);
		if (FAILED(hr))
		{
			LOG(LOG_ERROR, _T("RenderStream audio failed. Error: %08X"), hr);
			return false;
		}
	}

	hr = m_pMediaControl->Run();
	if (FAILED(hr))
	{
		LOG(LOG_ERROR, _T("MediaControl run failed. Error: %08X"), hr);
		return false;
	}

	// You can also set the filter graph to run with no clock, by calling SetSyncSource with the value NULL. 
	// If there is no clock, the graph runs as quickly as possible. 
	// With no clock, renderer filters do not wait for a sample's presentation time. 
	// Instead, they render each sample as soon as it arrives. 
	// Setting the graph to run without a clock is useful 
	// if you want to process data quickly, rather than previewing it in real time.
	// hr = pMediaFilter->SetSyncSource(NULL);
#if 0
	IMediaFilter    * pMediaFilter    = NULL;
	IReferenceClock * pReferenceClock = NULL;
	hr = m_pGraphBuilder->QueryInterface(IID_IMediaFilter, (void**)&pMediaFilter);
	if (SUCCEEDED(hr))
	{
		hr = pMediaFilter->GetSyncSource(&pReferenceClock);
		if (SUCCEEDED(hr))
		{
			REFERENCE_TIME rtStartTime = 0;
			hr = pReferenceClock->GetTime(&rtStartTime);
			if (SUCCEEDED(hr))
			{
				// Reference clocks measure time in 100-nanosecond intervals since system startup.
				m_pEncoderParam->m_ullStartTime = rtStartTime / 10000;
			}
			pReferenceClock->Release();
			pReferenceClock = NULL;
		}
		pMediaFilter->Release();
		pMediaFilter = NULL;
	}
	if (m_pEncoderParam->m_ullStartTime == 0)
	{
		LOG(LOG_WARNING, _T("direct show reference clock failure, use system time instead."));
		m_pEncoderParam->m_ullStartTime = Utils::GetSystemMilliSeconds();
	}
#endif

	m_bRunning = true;
	LOG(LOG_DEBUG, _T("dshow capture started. has video: %s, device: %S, has audio: %s, device: %S"),
		m_pEncoderParam->m_bVideoEnable ? _T("yes") : _T("no"),
		(strlen(m_pEncoderParam->m_szVideoDeviceName) > 0) ? m_pEncoderParam->m_szVideoDeviceName : "auto", 
		m_pEncoderParam->m_bAudioEnable ? _T("yes") : _T("no"),
		(strlen(m_pEncoderParam->m_szAudioDeviceName) > 0) ? m_pEncoderParam->m_szAudioDeviceName : "auto");

	return true;
}

void CDshowCapture::Stop()
{
	if (m_bRunning)
	{
		m_bRunning = false;
		if (m_pMediaControl)
			m_pMediaControl->Stop();
		LOG(LOG_DEBUG, _T("capture stopped"));
	}

	// Stop audio capture stream
	if (m_pAudioMediaType)
	{
		DeleteMediaType(m_pAudioMediaType);
		m_pAudioMediaType = NULL;
	}
	if (m_pAudioGrabber)
	{
		m_pAudioGrabber->Release();
		m_pAudioGrabber = NULL;
	}
	if (m_pAudioGrabberFilter)
	{
		m_pAudioGrabberFilter->Release();
		m_pAudioGrabberFilter = NULL;
	}
	if (m_pAudioDeviceFilter)
	{
		m_pAudioDeviceFilter->Release();
		m_pAudioDeviceFilter = NULL;
	}
	
	// Stop video capture stream
	if (m_pVideoMediaType)
	{
		DeleteMediaType(m_pVideoMediaType);
		m_pVideoMediaType = NULL;
	}
	if (m_pVideoGrabber)
	{
		m_pVideoGrabber->Release();
		m_pVideoGrabber = NULL;
	}
	if (m_pVideoGrabberFilter)
	{
		m_pVideoGrabberFilter->Release();
		m_pVideoGrabberFilter = NULL;
	}
	if (m_pVideoDeviceFilter)
	{
		m_pVideoDeviceFilter->Release();
		m_pVideoDeviceFilter = NULL;
	}

	// Stop capture graph builder
	if (m_pMediaControl)
	{
		m_pMediaControl->Release();
		m_pMediaControl = NULL;
	}
	if (m_pCaptureGraphBuilder)
	{
		m_pCaptureGraphBuilder->Release();
		m_pCaptureGraphBuilder = NULL;
	}
	if (m_pGraphBuilder)
	{
		m_pGraphBuilder->Release();
		m_pGraphBuilder = NULL;
	}
}

bool CDshowCapture::IsRunning()
{
	return m_bRunning;
}

bool CDshowCapture::BindDeviceFilter(const IID & clsidDeviceClass, LPCTSTR pszDeviceName, IBaseFilter **ppDeviceFilter)
{
	HRESULT          hr;
	ICreateDevEnum * pCreateDevEnum = NULL;
	IEnumMoniker   * pEnumMoniker   = NULL;
	IMoniker       * pMoniker       = NULL;
	ULONG            cFetched       = 0;
	IPropertyBag   * pPropertyBag   = NULL;
	bool             bBinded        = false;
	bool             bFound         = false;

	hr = ::CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER, IID_ICreateDevEnum, (void**)&pCreateDevEnum);
	if (SUCCEEDED(hr))
	{
		hr = pCreateDevEnum->CreateClassEnumerator(clsidDeviceClass, &pEnumMoniker, 0);
		if (SUCCEEDED(hr))
		{
			hr = pEnumMoniker->Reset();
			if (SUCCEEDED(hr))
			{
				while (bBinded != true)
				{
					hr = pEnumMoniker->Next(1, &pMoniker, &cFetched);
					if (FAILED(hr) || !pMoniker) break;

					bFound = ((pszDeviceName == NULL) || _tcslen(pszDeviceName) == 0) ? true : false;
					hr = pMoniker->BindToStorage(0, 0, IID_IPropertyBag, (void **)&pPropertyBag);
					if (SUCCEEDED(hr))
					{
						VARIANT varPropValue;
						varPropValue.vt = VT_BSTR;
						hr = pPropertyBag->Read(L"FriendlyName", &varPropValue, NULL);
						if (SUCCEEDED(hr))
						{
							LOG(LOG_DEBUG, _T("detect device : %s, type: %s"), varPropValue.bstrVal, 
								(clsidDeviceClass == CLSID_VideoInputDeviceCategory ? _T("video") : _T("audio")));
							if (pszDeviceName != NULL && _tcslen(pszDeviceName) > 0 && 0 == _tcsicmp(pszDeviceName, varPropValue.bstrVal))
							{
								bFound = true;
							}
							SysFreeString(varPropValue.bstrVal);
						}
						pPropertyBag->Release();
						pPropertyBag = NULL;
					}

					if (bFound)
					{
						hr = pMoniker->BindToObject(0, 0, IID_IBaseFilter, (void**)ppDeviceFilter);
						if (SUCCEEDED(hr))
						{
							bBinded = true;
						}
					}
					pMoniker->Release();
					pMoniker = NULL;
				}
			}
			pEnumMoniker->Release();
			pEnumMoniker = NULL;
		}
		pCreateDevEnum->Release();
		pCreateDevEnum = NULL;
	}

	return bBinded;
}

bool CDshowCapture::BuildVideoDevice()
{
	HRESULT           hr;
	IAMStreamConfig * pStreamConfig = NULL;
	AM_MEDIA_TYPE   * pMediaType = NULL;
	int               nIndex = 0;
	int               nCount = 0;
	int               nSize = 0;
	bool              bMatched = false;
	bool              bSetted = false;
	TCHAR             szVideoDeviceName[256] = {0};

	Utils::UTF8Decode(m_pEncoderParam->m_szVideoDeviceName, szVideoDeviceName);
	if (!BindDeviceFilter(CLSID_VideoInputDeviceCategory, szVideoDeviceName, &m_pVideoDeviceFilter))
	{
		LOG(LOG_ERROR, _T("BindDeviceFilter for video device failed"));
		return false;
	}

	hr = m_pGraphBuilder->AddFilter(m_pVideoDeviceFilter, L"Video Device Filter");
	if (FAILED(hr))
	{
		LOG(LOG_ERROR, _T("AddFilter for video device failed. Error: %08X"), hr);
		return false;
	}

	hr = m_pCaptureGraphBuilder->FindInterface(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, 
		m_pVideoDeviceFilter, IID_IAMStreamConfig, (void **)&pStreamConfig);
	if (FAILED(hr) || !pStreamConfig)
	{
		LOG(LOG_ERROR, _T("FindInterface IID_IAMStreamConfig for video device failed. Error: %08X"), hr);
		return false;
	}

	hr = pStreamConfig->GetNumberOfCapabilities(&nCount, &nSize);
	if (SUCCEEDED(hr))
	{
		if (nSize == sizeof(VIDEO_STREAM_CONFIG_CAPS))
		{
			LOG(LOG_DEBUG, _T("stream configuration count: %d"), nCount);
			for (nIndex = 0; nIndex < nCount; nIndex++)
			{
				VIDEO_STREAM_CONFIG_CAPS stConfigCaps;
				hr = pStreamConfig->GetStreamCaps(nIndex, &pMediaType, (BYTE *)&stConfigCaps);
				if (SUCCEEDED(hr))
				{
					LOG(LOG_DEBUG, _T("CONFIG_CAPS[%d]: MinOutput(%dx%d), MaxOutput(%dx%d)"), nIndex + 1, 
						stConfigCaps.MinOutputSize.cx, stConfigCaps.MinOutputSize.cy, 
						stConfigCaps.MaxOutputSize.cx, stConfigCaps.MaxOutputSize.cy);
					PrintMediaType(pMediaType);

					if (!bSetted && m_pEncoderParam->m_nVideoWidth > 0 && m_pEncoderParam->m_nVideoHeight > 0)
					{
						if ((pMediaType->majortype == MEDIATYPE_Video) &&                     // major type: video
							(m_pEncoderParam->m_nVideoCSP == ENCODER_CSP_NONE ||              // sub type: any color space
							pMediaType->subtype == CSP2GUID(m_pEncoderParam->m_nVideoCSP)) && // sub type: specific color space
							(pMediaType->formattype == FORMAT_VideoInfo) &&                   // format type: video info
							(pMediaType->cbFormat >= sizeof(VIDEOINFOHEADER)) && 
							(pMediaType->pbFormat != NULL))
						{
							VIDEOINFOHEADER * pVideoInfoHeader = (VIDEOINFOHEADER *)pMediaType->pbFormat;
							if ((m_pEncoderParam->m_nVideoWidth == pVideoInfoHeader->bmiHeader.biWidth) &&
								(m_pEncoderParam->m_nVideoHeight == pVideoInfoHeader->bmiHeader.biHeight))
							{
								bMatched = false;
								if (m_pEncoderParam->m_nVideoFPS > 0)
								{
									// AvgTimePerFrame is average display time of the video frames, in 100-nanosecond units.
									int nFramesPerSec = ((int)pVideoInfoHeader->AvgTimePerFrame > 0) ? (10000000 / (int)pVideoInfoHeader->AvgTimePerFrame) : 0;
									if (nFramesPerSec == m_pEncoderParam->m_nVideoFPS)
									{
										bMatched = true;
									}
									else if (!m_pEncoderParam->m_bVideoFPSStrict && (nFramesPerSec > m_pEncoderParam->m_nVideoFPS))
									{
										bMatched = true;
										pVideoInfoHeader->AvgTimePerFrame = 10000000 / m_pEncoderParam->m_nVideoFPS;
										pVideoInfoHeader->bmiHeader.biSizeImage = DIBSIZE(pVideoInfoHeader->bmiHeader);
										pVideoInfoHeader->dwBitRate = pVideoInfoHeader->bmiHeader.biSizeImage * m_pEncoderParam->m_nVideoFPS;
									}
								}
								else
								{
									bMatched = true;
								}
								if (bMatched)
								{
									hr = pStreamConfig->SetFormat(pMediaType);
									if (SUCCEEDED(hr))
									{
										bSetted = true;
										LOG(LOG_DEBUG, _T("video stream configuration succeed"));
									}
								}
							}
						}
					}
					DeleteMediaType(pMediaType);
					pMediaType = NULL;
				}
			}
		}
	}

	hr = pStreamConfig->GetFormat(&m_pVideoMediaType);
	if (FAILED(hr) || !m_pVideoMediaType)
	{
		LOG(LOG_ERROR, _T("GetFormat for video device failed. Error: %08X"), hr);
		return false;
	}

	LOG(LOG_DEBUG, _T("############### CURRENT VIDEO FORMAT ################"));
	PrintMediaType(m_pVideoMediaType);

	if (m_pVideoMediaType->majortype == MEDIATYPE_Video)
	{
		m_pEncoderParam->m_nVideoCSP = GUID2CSP(m_pVideoMediaType->subtype);
		if ((m_pVideoMediaType->formattype == FORMAT_VideoInfo) && 
			(m_pVideoMediaType->cbFormat >= sizeof(VIDEOINFOHEADER)) && 
			(m_pVideoMediaType->pbFormat != NULL))
		{
			VIDEOINFOHEADER * pVideoInfoHeader    = (VIDEOINFOHEADER *)m_pVideoMediaType->pbFormat;
			m_pEncoderParam->m_nVideoWidth        = pVideoInfoHeader->bmiHeader.biWidth;
			m_pEncoderParam->m_nVideoHeight       = pVideoInfoHeader->bmiHeader.biHeight;
			m_pEncoderParam->m_nVideoFPS          = ((int)pVideoInfoHeader->AvgTimePerFrame > 0) ? (10000000 / (int)pVideoInfoHeader->AvgTimePerFrame) : 0;
			if (m_pEncoderParam->m_bVideoDropRaw && m_pEncoderParam->m_bVideoDropFPS && 
				m_pEncoderParam->m_nVideoDropDen > 0 && m_pEncoderParam->m_nVideoDropDen > m_pEncoderParam->m_nVideoDropNum)
			{
				m_pEncoderParam->m_nVideoFPS      *= (m_pEncoderParam->m_nVideoDropDen - m_pEncoderParam->m_nVideoDropNum) * 1.0 / m_pEncoderParam->m_nVideoDropDen;
			}
			m_pEncoderParam->m_nVideoRawFrameSize = pVideoInfoHeader->bmiHeader.biSizeImage;
			m_pEncoderParam->m_nVideoAVCBitRate   = (m_pEncoderParam->m_nVideoAVCBitRateFactor > 0) ? (m_pEncoderParam->m_nVideoWidth * m_pEncoderParam->m_nVideoHeight * m_pEncoderParam->m_nVideoFPS / m_pEncoderParam->m_nVideoAVCBitRateFactor) : 0;	
		}
	}

#if 0
	IBaseFilter * m_pConvertorFilter = NULL;
	IPin        * m_pColorOutPin     = NULL;
	IPin        * m_pColorInPin      = NULL;
	IDMOWrapperFilter * m_pDMOWrapperFilter = NULL;

	hr = ::CoCreateInstance(CLSID_DMOWrapperFilter, NULL, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void**)&m_pConvertorFilter);
	if (FAILED(hr) || !m_pConvertorFilter)
		return false;

	hr = m_pConvertorFilter->QueryInterface(IID_IDMOWrapperFilter, (void **)&m_pDMOWrapperFilter);
	if (FAILED(hr) || !m_pDMOWrapperFilter)
		return false;

	hr = m_pDMOWrapperFilter->Init(CLSID_CColorConvertDMO, DMOCATEGORY_VIDEO_EFFECT);
	if (FAILED(hr))
		return false;

	hr = m_pGraphBuilder->AddFilter(m_pConvertorFilter, _T("Video Color Convertor Filter"));
	if (FAILED(hr))
		return false;

	hr = m_pConvertorFilter->FindPin(L"in0",  &m_pColorInPin);
	if (FAILED(hr))
		return false;

	hr = m_pConvertorFilter->FindPin(L"out0",  &m_pColorOutPin);
	if (FAILED(hr))
		return false;

	IEnumMediaTypes * pMediaTypeEnum = NULL;
	hr = m_pColorInPin->EnumMediaTypes(&pMediaTypeEnum);
	if (SUCCEEDED(hr))
	{
		hr = pMediaTypeEnum->Reset();
		while (pMediaTypeEnum->Next(1, &pMediaType, NULL) == S_OK)
		{
			PrintMediaType(pMediaType);
			hr = m_pColorInPin->QueryAccept(pMediaType);
			if (FAILED(hr))
			{
				LOG(LOG_WARNING, _T("QueryAccept failed"));
			}
			DeleteMediaType(pMediaType);
			pMediaType = NULL;
		}
		pMediaTypeEnum->Release();
		pMediaTypeEnum = NULL;
	}
#endif

	return true;
}

bool CDshowCapture::BuildVideoGrabber()
{
	HRESULT hr;

	hr = ::CoCreateInstance(CLSID_SampleGrabber, NULL, CLSCTX_INPROC_SERVER, IID_ISampleGrabber, (void**)&m_pVideoGrabber);
	if (FAILED(hr) || !m_pVideoGrabber)
	{
		LOG(LOG_ERROR, _T("CoCreateInstance CLSID_SampleGrabber for video grabber failed. Error: %08X"), hr);
		return false;
	}

	// http://msdn.microsoft.com/en-us/library/dd376991(v=vs.85).aspx
	// FALSE = Use the callback mode. To set the callback method, call ISampleGrabber::SetCallback.
	// TRUE = Use the buffering mode. To get the copied buffer, call ISampleGrabber::GetCurrentBuffer.
	// Do not call ISampleGrabber::GetCurrentBuffer while the filter graph is running. 
	// While the filter graph is running, the Sample Grabber filter overwrites the contents of the buffer whenever it receives a new sample. 
	// The best way to use buffering method is to use "one-shot mode" which stops the graph after receiving the first sample.
	hr = m_pVideoGrabber->SetBufferSamples(FALSE);
	// The SetOneShot method specifies whether the Sample Grabber filter halts after the filter receives a sample.
	hr = m_pVideoGrabber->SetOneShot(FALSE);
	// 0 = Use the SampleCB callback, 1 = Use th BufferCB callback
	if (m_pVideoGrabberCB)
		hr = m_pVideoGrabber->SetCallback(m_pVideoGrabberCB, 1);

	// http://msdn.microsoft.com/en-us/library/dd376993(v=vs.85).aspx
	// By default, the Sample Grabber has no preferred media type. 
	// To ensure that the Sample Grabber connects to the correct filter, 
	// call this method before building the filter graph.

	// For video media types, the Sample Grabber ignores the format block. 
	// Therefore, it will accept any video size and frame rate. 
	// When you call SetMediaType, set the format block (pbFormat) to NULL and the size (cbFormat) to zero.
	// hr = m_pVideoGrabber->SetMediaType(m_pMediaType);

	hr = m_pVideoGrabber->QueryInterface(IID_IBaseFilter, (void**)&m_pVideoGrabberFilter);
	if (FAILED(hr) || !m_pVideoGrabberFilter)
	{
		LOG(LOG_ERROR, _T("QueryInterface IID_IBaseFilter for video grabber failed. Error: %08X"), hr);
		return false;
	}

	hr = m_pGraphBuilder->AddFilter(m_pVideoGrabberFilter, L"Video Grabber Filter");
	if (FAILED(hr))
	{
		LOG(LOG_ERROR, _T("AddFilter for video grabber failed. Error: %08X"), hr);
		return false;
	}

	// http://msdn.microsoft.com/en-us/library/dd376988(v=vs.85).aspx
	// The GetConnectedMediaType method retrieves the media type for the connection on the input pin of the Sample Grabber.
#if 0
	m_pVideoMediaType = new AM_MEDIA_TYPE;
	memset(m_pVideoMediaType, 0, sizeof(AM_MEDIA_TYPE));
	hr = m_pVideoGrabber->GetConnectedMediaType(m_pVideoMediaType);
	if (FAILED(hr))
		return false;
#endif

	return true;
}

bool CDshowCapture::BuildAudioDevice()
{
	HRESULT                hr;
	IAMBufferNegotiation * pBufferNegotiation = NULL;
	IAMStreamConfig      * pStreamConfig      = NULL;
	AM_MEDIA_TYPE        * pMediaType         = NULL;
	IEnumPins            * pEnumPins          = NULL;
	ULONG                  cFetched           = 0;
	IPin                 * pPin               = NULL;
	TCHAR                  szAudioDeviceName[256] = {0};

	Utils::UTF8Decode(m_pEncoderParam->m_szAudioDeviceName, szAudioDeviceName);
	if (!BindDeviceFilter(CLSID_AudioInputDeviceCategory, szAudioDeviceName, &m_pAudioDeviceFilter))
	{
		LOG(LOG_ERROR, _T("BindDeviceFilter for audio device failed"));
		return false;
	}

	hr = m_pGraphBuilder->AddFilter(m_pAudioDeviceFilter, L"Audio Device Filter");
	if (FAILED(hr))
	{
		LOG(LOG_ERROR, _T("AddFilter for audio device failed. Error: %08X"), hr);
		return false;
	}

	hr = m_pAudioDeviceFilter->EnumPins(&pEnumPins);
	if (SUCCEEDED(hr))
	{
		while (S_OK == pEnumPins->Next(1, &pPin, &cFetched))
		{
			PIN_DIRECTION nPinDirection;
			hr = pPin->QueryDirection(&nPinDirection);
			if (SUCCEEDED(hr))
			{
				// there could be both a Capture and a Preview out pin.
				if (nPinDirection == PINDIR_OUTPUT)
				{
					hr = pPin->QueryInterface(IID_IAMBufferNegotiation, (void **)&pBufferNegotiation);
					if (SUCCEEDED(hr))
					{
						// Change buffer size to reduce sound delay.
						// Default buffer size is 500ms PCM sound data.
						ALLOCATOR_PROPERTIES stAllocProp = {0};
						m_pEncoderParam->m_nAudioRawFrameSize = m_pEncoderParam->m_nAudioBufferSamples * m_pEncoderParam->m_nAudioBytesPerSample * m_pEncoderParam->m_nAudioChannels;
						stAllocProp.cbBuffer = m_pEncoderParam->m_nAudioRawFrameSize;
						stAllocProp.cBuffers = m_pEncoderParam->m_nAudioBufferCount;
						stAllocProp.cbAlign = m_pEncoderParam->m_nAudioBytesPerSample * m_pEncoderParam->m_nAudioChannels;
						hr = pBufferNegotiation->SuggestAllocatorProperties(&stAllocProp);
						if (SUCCEEDED(hr))
						{
							LOG(LOG_DEBUG, _T("audio buffer negotiation succeed"));
						}
						pBufferNegotiation->Release();
						pBufferNegotiation = NULL;
					}

					hr = pPin->QueryInterface(IID_IAMStreamConfig, (void **)&pStreamConfig);
					if (SUCCEEDED(hr))
					{
						hr = pStreamConfig->GetFormat(&pMediaType);
						if (SUCCEEDED(hr))
						{
							WAVEFORMATEX * pWaveFormat = (WAVEFORMATEX *)pMediaType->pbFormat;
							pWaveFormat->nChannels = (WORD)m_pEncoderParam->m_nAudioChannels;
							pWaveFormat->nSamplesPerSec = (DWORD)m_pEncoderParam->m_nAudioSamplesPerSec;
							pWaveFormat->nAvgBytesPerSec = (DWORD)(m_pEncoderParam->m_nAudioSamplesPerSec * m_pEncoderParam->m_nAudioBytesPerSample * m_pEncoderParam->m_nAudioChannels);
							pWaveFormat->wBitsPerSample = (WORD)(m_pEncoderParam->m_nAudioBytesPerSample * 8);
							pWaveFormat->nBlockAlign = (WORD)(m_pEncoderParam->m_nAudioBytesPerSample * m_pEncoderParam->m_nAudioChannels);

							hr = pStreamConfig->SetFormat(pMediaType);
							if (SUCCEEDED(hr))
							{
								LOG(LOG_DEBUG, _T("audio stream configuration succeed"));
								LOG(LOG_DEBUG, _T("############### CURRENT AUDIO FORMAT ################"));
								PrintMediaType(pMediaType);
							}
							DeleteMediaType(pMediaType);
							pMediaType = NULL;
						}
						pStreamConfig->Release();
						pStreamConfig = NULL;
					}
				}
			}
			pPin->Release();
			pPin = NULL;
		}
		pEnumPins->Release();
		pEnumPins = NULL;
	}

	return true;
}

bool CDshowCapture::BuildAudioGrabber()
{
	HRESULT hr;

	hr = ::CoCreateInstance(CLSID_SampleGrabber, NULL, CLSCTX_INPROC_SERVER, IID_ISampleGrabber, (void**)&m_pAudioGrabber);
	if (FAILED(hr) || !m_pAudioGrabber)
	{
		LOG(LOG_ERROR, _T("CoCreateInstance CLSID_SampleGrabber for audio grabber failed. Error: %08X"), hr);
		return false;
	}

	hr = m_pAudioGrabber->SetBufferSamples(FALSE);
	hr = m_pAudioGrabber->SetOneShot(FALSE);
	if (m_pAudioGrabberCB)
		hr = m_pAudioGrabber->SetCallback(m_pAudioGrabberCB, 1);

	hr = m_pAudioGrabber->QueryInterface(IID_IBaseFilter, (void**)&m_pAudioGrabberFilter);
	if (FAILED(hr) || !m_pAudioGrabberFilter)
	{
		LOG(LOG_ERROR, _T("QueryInterface IID_IBaseFilter for audio grabber failed. Error: %08X"), hr);
		return false;
	}

	hr = m_pGraphBuilder->AddFilter(m_pAudioGrabberFilter, L"Audio Grabber Filter");
	if (FAILED(hr))
	{
		LOG(LOG_ERROR, _T("AddFilter for audio grabber failed. Error: %08X"), hr);
		return false;
	}

	// http://msdn.microsoft.com/en-us/library/dd376988(v=vs.85).aspx
	// The GetConnectedMediaType method retrieves the media type for the connection on the input pin of the Sample Grabber.
#if 0
	m_pAudioMediaType = new AM_MEDIA_TYPE;
	memset(m_pAudioMediaType, 0, sizeof(AM_MEDIA_TYPE));
	hr = m_pAudioGrabber->GetConnectedMediaType(m_pAudioMediaType);
	if (FAILED(hr))
		return false;
#endif

	return true;
}

LPCTSTR CDshowCapture::GetMediaGUID(GUID & guid, LPTSTR str)
{
	if (str != NULL)
	{
		// GUID header file: \microsoft sdks\windows\v7.0a\include\uuids.h
		// example: 32595559-0000-0010-8000-00AA00389B71  'YUY2' == MEDIASUBTYPE_YUY2
		_stprintf(str, _T("%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X"),
			guid.Data1, guid.Data2, guid.Data3, 
			guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3], 
			guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
	}
	return str;
}

LPCTSTR CDshowCapture::GetMediaMajorType(AM_MEDIA_TYPE * pMediaType, LPTSTR szGUID)
{
	if (pMediaType != NULL)
	{
		if (pMediaType->majortype == MEDIATYPE_Video)                return _T("Video");
		else if (pMediaType->majortype == MEDIATYPE_Audio)           return _T("Audio");
		else if (pMediaType->majortype == MEDIATYPE_Text)            return _T("Text");
		else if (pMediaType->majortype == MEDIATYPE_Midi)            return _T("Midi");
		else if (pMediaType->majortype == MEDIATYPE_Stream)          return _T("Stream");
		else if (pMediaType->majortype == MEDIATYPE_Interleaved)     return _T("Interleaved");
		else if (pMediaType->majortype == MEDIATYPE_File)            return _T("File");
		else if (pMediaType->majortype == MEDIATYPE_ScriptCommand)   return _T("ScriptCommand");
		else if (pMediaType->majortype == MEDIATYPE_AUXLine21Data)   return _T("AUXLine21Data");
		else if (pMediaType->majortype == MEDIATYPE_AUXTeletextPage) return _T("AUXTeletextPage");
		else if (pMediaType->majortype == MEDIATYPE_CC_CONTAINER)    return _T("CC_CONTAINER");
		else if (pMediaType->majortype == MEDIATYPE_DTVCCData)       return _T("DTVCCData");
		else if (pMediaType->majortype == MEDIATYPE_MSTVCaption)     return _T("MSTVCaption");
		else if (pMediaType->majortype == MEDIATYPE_VBI)             return _T("VBI");
		else if (pMediaType->majortype == MEDIATYPE_Timecode)        return _T("Timecode");
		else if (pMediaType->majortype == MEDIATYPE_LMRT)            return _T("LMRT");
		else if (pMediaType->majortype == MEDIATYPE_URL_STREAM)      return _T("URL_STREAM");
		else                                                         return GetMediaGUID(pMediaType->majortype, szGUID);
	}
	return _T("");
}

LPCTSTR CDshowCapture::GetMediaSubType(AM_MEDIA_TYPE * pMediaType, LPTSTR szGUID)
{
	if (pMediaType != NULL)
	{
		if (pMediaType->subtype == MEDIASUBTYPE_CLPL)             return _T("CLPL");
		else if (pMediaType->subtype == MEDIASUBTYPE_YUYV)        return _T("YUYV (Used by Canopus. Same as YUY2)");
		else if (pMediaType->subtype == MEDIASUBTYPE_IYUV)        return _T("IYUV");
		else if (pMediaType->subtype == MEDIASUBTYPE_YVU9)        return _T("YVU9 (Standard YVU9 format uncompressed data)");
		else if (pMediaType->subtype == MEDIASUBTYPE_Y411)        return _T("Y411 (YUV 411 format data. Same as Y41P)");
		else if (pMediaType->subtype == MEDIASUBTYPE_Y41P)        return _T("Y41P (packed 4:1:1)");
		else if (pMediaType->subtype == MEDIASUBTYPE_YUY2)        return _T("YUY2 (packed 4:2:2)");
		else if (pMediaType->subtype == MEDIASUBTYPE_YVYU)        return _T("YVYU (packed 4:2:2)");
		else if (pMediaType->subtype == MEDIASUBTYPE_UYVY)        return _T("UYVY (packed 4:2:2)");
		else if (pMediaType->subtype == MEDIASUBTYPE_Y211)        return _T("Y211");
		else if (pMediaType->subtype == MEDIASUBTYPE_CLJR)        return _T("CLJR (Cirrus Logic Jr YUV 411 format with less than 8 bits per Y, U, and V sample)");
		else if (pMediaType->subtype == MEDIASUBTYPE_IF09)        return _T("IF09 (Indeo produced YVU9 format)");
		else if (pMediaType->subtype == MEDIASUBTYPE_CPLA)        return _T("CPLA");
		else if (pMediaType->subtype == MEDIASUBTYPE_MJPG)        return _T("MJPG (Motion JPEG compressed video)");
		else if (pMediaType->subtype == MEDIASUBTYPE_TVMJ)        return _T("TVMJ (TrueVision MJPG format)");
		else if (pMediaType->subtype == MEDIASUBTYPE_WAKE)        return _T("WAKE (MJPG format produced by some cards)");
		else if (pMediaType->subtype == MEDIASUBTYPE_CFCC)        return _T("CFCC (MJPG format produced by some cards)");
		else if (pMediaType->subtype == MEDIASUBTYPE_IJPG)        return _T("IJPG (Intergraph JPEG format)");
		else if (pMediaType->subtype == MEDIASUBTYPE_Plum)        return _T("Plum (Plum MJPG format)");
		else if (pMediaType->subtype == MEDIASUBTYPE_DVCS)        return _T("DVCS (FAST DV-Master)");
		else if (pMediaType->subtype == MEDIASUBTYPE_H264)        return _T("H264 (H.264 compressed video stream)");
		else if (pMediaType->subtype == MEDIASUBTYPE_DVSD)        return _T("DVSD (FAST DV-Master)");
		else if (pMediaType->subtype == MEDIASUBTYPE_MDVF)        return _T("MDVF (MIROVideo DV)");
		// RGB
		else if (pMediaType->subtype == MEDIASUBTYPE_RGB1)        return _T("RGB1 (1 bpp, Palettized)");
		else if (pMediaType->subtype == MEDIASUBTYPE_RGB4)        return _T("RGB4 (4 bpp, Palettized)");
		else if (pMediaType->subtype == MEDIASUBTYPE_RGB8)        return _T("RGB8 (8 bpp, Palettized)");
		else if (pMediaType->subtype == MEDIASUBTYPE_RGB555)      return _T("RGB555 (16 bpp)");
		else if (pMediaType->subtype == MEDIASUBTYPE_RGB565)      return _T("RGB565 (16 bpp)");
		else if (pMediaType->subtype == MEDIASUBTYPE_RGB24)       return _T("RGB24 (24 bpp)");
		else if (pMediaType->subtype == MEDIASUBTYPE_RGB32)       return _T("RGB32 (32 bpp)");
		// RGB surfaces that contain per pixel alpha values.
		else if (pMediaType->subtype == MEDIASUBTYPE_ARGB1555)    return _T("ARGB1555 (RGB 555 with alpha channel)");
		else if (pMediaType->subtype == MEDIASUBTYPE_ARGB32)      return _T("ARGB32 (RGB 32 with alpha channel)");
		else if (pMediaType->subtype == MEDIASUBTYPE_ARGB4444)    return _T("ARGB4444 (16-bit RGB with alpha channel; 4 bits per channel)");
		else if (pMediaType->subtype == MEDIASUBTYPE_A2R10G10B10) return _T("A2R10G10B10 (32-bit RGB with alpha channel; 10 bits per RGB channel plus 2 bits for alpha)");
		else if (pMediaType->subtype == MEDIASUBTYPE_A2B10G10R10) return _T("A2B10G10R10 (32-bit RGB with alpha channel; 10 bits per RGB channel plus 2 bits for alpha)");
		// See the DX-VA header and documentation for a description of this format.
		else if (pMediaType->subtype == MEDIASUBTYPE_AYUV)        return _T("AYUV (4:4:4 YUV format)");
		else if (pMediaType->subtype == MEDIASUBTYPE_AI44)        return _T("AI44");
		else if (pMediaType->subtype == MEDIASUBTYPE_IA44)        return _T("IA44");
		// Audio
		else if (pMediaType->subtype == MEDIASUBTYPE_PCMAudio_Obsolete) return _T("PCMAudio_Obsolete");
		else if (pMediaType->subtype == MEDIASUBTYPE_PCM)         return _T("PCM");
		else if (pMediaType->subtype == MEDIASUBTYPE_WAVE)        return _T("WAVE"); // Data from WAV file
		else if (pMediaType->subtype == MEDIASUBTYPE_AU)          return _T("AU");   // Data from AU file
		else if (pMediaType->subtype == MEDIASUBTYPE_AIFF)        return _T("AIFF"); // Data from AIFF file
		// DV format
		else if (pMediaType->subtype == MEDIASUBTYPE_dvsd)        return _T("dvsd (Standard DV format)");
		else if (pMediaType->subtype == MEDIASUBTYPE_dvhd)        return _T("dvhd (High Definition DV format)");
		else if (pMediaType->subtype == MEDIASUBTYPE_dvsl)        return _T("dvsl (Long Play DV format)");
		else if (pMediaType->subtype == MEDIASUBTYPE_dv25)        return _T("dv25");
		else if (pMediaType->subtype == MEDIASUBTYPE_dv50)        return _T("dv50");
		else if (pMediaType->subtype == MEDIASUBTYPE_dvh1)        return _T("dvh1");
		else                                                      return GetMediaGUID(pMediaType->subtype, szGUID);
	}
	return _T("");
}

LPCTSTR CDshowCapture::GetMediaFormatType(AM_MEDIA_TYPE * pMediaType, LPTSTR szGUID)
{
	if (pMediaType != NULL)
	{
		if (pMediaType->formattype == FORMAT_VideoInfo)         return _T("VideoInfo");
		else if (pMediaType->formattype == FORMAT_VideoInfo2)   return _T("VideoInfo2");
		else if (pMediaType->formattype == FORMAT_DvInfo)       return _T("DvInfo");
		else if (pMediaType->formattype == FORMAT_MPEGVideo)    return _T("MPEGVideo");
		else if (pMediaType->formattype == FORMAT_MPEG2Video)   return _T("MPEG2Video");
		else if (pMediaType->formattype == FORMAT_WaveFormatEx) return _T("WaveFormatEx");
		else if (pMediaType->formattype == FORMAT_None)         return _T("None");
		else if (pMediaType->formattype == GUID_NULL)           return _T("NULL");
		else                                                    return GetMediaGUID(pMediaType->formattype, szGUID);
	}
	return _T("");
}

void CDshowCapture::PrintMediaType(AM_MEDIA_TYPE * pMediaType)
{
	TCHAR szGUID[64] = {0};

	if (pMediaType != NULL)
	{
		LOG(LOG_DEBUG, _T("-----------------------------------------------------"));
		LOG(LOG_DEBUG, _T("Media Major Type  : %s"), this->GetMediaMajorType(pMediaType, szGUID));
		LOG(LOG_DEBUG, _T("Media Sub Type    : %s"), this->GetMediaSubType(pMediaType, szGUID)); 
		LOG(LOG_DEBUG, _T("Media Format Type : %s"), this->GetMediaFormatType(pMediaType, szGUID));

		if (pMediaType->formattype == FORMAT_VideoInfo)
		{
			if (pMediaType->pbFormat != NULL && pMediaType->cbFormat > 0)
			{
				VIDEOINFOHEADER * pVideoInfoHeader = (VIDEOINFOHEADER *)pMediaType->pbFormat;
				// AvgTimePerFrame is average display time of the video frames, in 100-nanosecond units.
				int nFramesPerSec = ((int)pVideoInfoHeader->AvgTimePerFrame > 0) ? (10000000 / (int)pVideoInfoHeader->AvgTimePerFrame) : 0;
				LOG(LOG_DEBUG, _T("*****************************************************"));
				LOG(LOG_DEBUG, _T("Video Resolution  : %dx%d"), pVideoInfoHeader->bmiHeader.biWidth, pVideoInfoHeader->bmiHeader.biHeight);
				LOG(LOG_DEBUG, _T("Video FPS         : %d"), nFramesPerSec);
				LOG(LOG_DEBUG, _T("Video Image Size  : %d"), pVideoInfoHeader->bmiHeader.biSizeImage);
			}
		}
		else if (pMediaType->formattype == FORMAT_VideoInfo2)
		{
			if (pMediaType->pbFormat != NULL && pMediaType->cbFormat > 0)
			{
				VIDEOINFOHEADER2 * pVideoInfoHeader2 = (VIDEOINFOHEADER2 *)pMediaType->pbFormat;
				// AvgTimePerFrame is average display time of the video frames, in 100-nanosecond units.
				int nFramesPerSec = ((int)pVideoInfoHeader2->AvgTimePerFrame > 0) ? (10000000 / (int)pVideoInfoHeader2->AvgTimePerFrame) : 0;
				LOG(LOG_DEBUG, _T("*****************************************************"));
				LOG(LOG_DEBUG, _T("Video Resolution  : %dx%d"), pVideoInfoHeader2->bmiHeader.biWidth, pVideoInfoHeader2->bmiHeader.biHeight);
				LOG(LOG_DEBUG, _T("Video FPS         : %d"), nFramesPerSec);
				LOG(LOG_DEBUG, _T("Video Image Size  : %d"), pVideoInfoHeader2->bmiHeader.biSizeImage);
			}
		}
		else if (pMediaType->formattype == FORMAT_WaveFormatEx)
		{
			if (pMediaType->pbFormat != NULL && pMediaType->cbFormat > 0)
			{
				WAVEFORMATEX * pWaveFormat = (WAVEFORMATEX *)pMediaType->pbFormat;
				LOG(LOG_DEBUG, _T("*****************************************************"));
				LOG(LOG_DEBUG, _T("Audio Channel     : %d"), pWaveFormat->nChannels);
				LOG(LOG_DEBUG, _T("Audio SamplesRate : %d"), pWaveFormat->nSamplesPerSec);
				LOG(LOG_DEBUG, _T("Audio BytesPerSec : %d"), pWaveFormat->nAvgBytesPerSec);
				LOG(LOG_DEBUG, _T("Audio BitsPerSamp : %d"), pWaveFormat->wBitsPerSample);
				LOG(LOG_DEBUG, _T("Audio BlockAlign  : %d"), pWaveFormat->nBlockAlign);
			}
		}
		LOG(LOG_DEBUG, _T("-----------------------------------------------------"));
	}
}

void CDshowCapture::DeleteMediaType(AM_MEDIA_TYPE * pMediaType)
{
	if (pMediaType != NULL)
	{
		if (pMediaType->pbFormat != NULL)
		{
			::CoTaskMemFree(pMediaType->pbFormat);
			pMediaType->pbFormat = NULL;
			pMediaType->cbFormat = 0;
		}
		if (pMediaType->pUnk != NULL)
		{
			pMediaType->pUnk->Release();
			pMediaType->pUnk = NULL;
		}
		::CoTaskMemFree(pMediaType);
		pMediaType = NULL;
	}
}

int CDshowCapture::GUID2CSP(GUID & guid)
{
	int csp = ENCODER_CSP_NONE;
	if (guid == MEDIASUBTYPE_CLPL)
		csp = ENCODER_CSP_CLPL;
	else if (guid == MEDIASUBTYPE_YUY2 || guid == MEDIASUBTYPE_YUYV)
		csp = ENCODER_CSP_YUYV;
	else if (guid == MEDIASUBTYPE_IYUV)
		csp = ENCODER_CSP_IYUV;
	else if (guid == MEDIASUBTYPE_YVU9)
		csp = ENCODER_CSP_YVU9;
	else if (guid == MEDIASUBTYPE_Y411 || guid == MEDIASUBTYPE_Y41P)
		csp = ENCODER_CSP_Y411;
	else if (guid == MEDIASUBTYPE_YVYU)
		csp = ENCODER_CSP_YVYU;
	else if (guid == MEDIASUBTYPE_UYVY)
		csp = ENCODER_CSP_UYVY;
	else if (guid == MEDIASUBTYPE_Y211)
		csp = ENCODER_CSP_Y211;
	else if (guid == MEDIASUBTYPE_CLJR)
		csp = ENCODER_CSP_CLJR;
	else if (guid == MEDIASUBTYPE_IF09)
		csp = ENCODER_CSP_IF09;
	else if (guid == MEDIASUBTYPE_CPLA)
		csp = ENCODER_CSP_CPLA;
	else if (guid == MEDIASUBTYPE_RGB1)
		csp = ENCODER_CSP_RGB1;
	else if (guid == MEDIASUBTYPE_RGB4)
		csp = ENCODER_CSP_RGB4;
	else if (guid == MEDIASUBTYPE_RGB8)
		csp = ENCODER_CSP_RGB8;
	else if (guid == MEDIASUBTYPE_RGB555)
		csp = ENCODER_CSP_RGB555;
	else if (guid == MEDIASUBTYPE_RGB565)
		csp = ENCODER_CSP_RGB565;
	else if (guid == MEDIASUBTYPE_RGB24)
		csp = ENCODER_CSP_RGB24;
	else if (guid == MEDIASUBTYPE_RGB32)
		csp = ENCODER_CSP_RGB32;
	else if (guid == MEDIASUBTYPE_ARGB1555)
		csp = ENCODER_CSP_ARGB1555;
	else if (guid == MEDIASUBTYPE_ARGB32)
		csp = ENCODER_CSP_ARGB32;
	else if (guid == MEDIASUBTYPE_ARGB4444)
		csp = ENCODER_CSP_ARGB4444;
	else if (guid == MEDIASUBTYPE_A2R10G10B10)
		csp = ENCODER_CSP_A2R10G10B10;
	else if (guid == MEDIASUBTYPE_A2B10G10R10)
		csp = ENCODER_CSP_A2B10G10R10;

	return csp;
}

GUID CDshowCapture::CSP2GUID(int csp)
{
	GUID guid = GUID_NULL;
	if (csp == ENCODER_CSP_CLPL)
		guid = MEDIASUBTYPE_CLPL;
	else if (csp == ENCODER_CSP_YUYV)
		guid = MEDIASUBTYPE_YUY2;
	else if (csp == ENCODER_CSP_IYUV)
		guid = MEDIASUBTYPE_IYUV;
	else if (csp == ENCODER_CSP_YVU9)
		guid = MEDIASUBTYPE_YVU9;
	else if (csp == ENCODER_CSP_Y411)
		guid = MEDIASUBTYPE_Y411;
	else if (csp == ENCODER_CSP_YVYU)
		guid = MEDIASUBTYPE_YVYU;
	else if (csp == ENCODER_CSP_UYVY)
		guid = MEDIASUBTYPE_UYVY;
	else if (csp == ENCODER_CSP_Y211)
		guid = MEDIASUBTYPE_Y211;
	else if (csp == ENCODER_CSP_CLJR)
		guid = MEDIASUBTYPE_CLJR;
	else if (csp == ENCODER_CSP_IF09)
		guid = MEDIASUBTYPE_IF09;
	else if (csp == ENCODER_CSP_CPLA)
		guid = MEDIASUBTYPE_CPLA;
	else if (csp == ENCODER_CSP_RGB1)
		guid = MEDIASUBTYPE_RGB1;
	else if (csp == ENCODER_CSP_RGB4)
		guid = MEDIASUBTYPE_RGB4;
	else if (csp == ENCODER_CSP_RGB8)
		guid = MEDIASUBTYPE_RGB8;
	else if (csp == ENCODER_CSP_RGB555)
		guid = MEDIASUBTYPE_RGB555;
	else if (csp == ENCODER_CSP_RGB565)
		guid = MEDIASUBTYPE_RGB565;
	else if (csp == ENCODER_CSP_RGB24)
		guid = MEDIASUBTYPE_RGB24;
	else if (csp == ENCODER_CSP_RGB32)
		guid = MEDIASUBTYPE_RGB32;
	else if (csp == ENCODER_CSP_ARGB1555)
		guid = MEDIASUBTYPE_ARGB1555;
	else if (csp == ENCODER_CSP_ARGB32)
		guid = MEDIASUBTYPE_ARGB32;
	else if (csp == ENCODER_CSP_ARGB4444)
		guid = MEDIASUBTYPE_ARGB4444;
	else if (csp == ENCODER_CSP_A2R10G10B10)
		guid = MEDIASUBTYPE_A2R10G10B10;
	else if (csp == ENCODER_CSP_A2B10G10R10)
		guid = MEDIASUBTYPE_A2B10G10R10;

	return guid;
}
