#include "stdafx.h"
#include "AudioEncoder.h"
#include "DownshaEncoder.h"
#include "Log.h"

CAudioEncoder::CAudioEncoder()
{
	m_bRunning         = false;
	m_pEncoderParam    = NULL;

	m_hFaacEncoder     = NULL;
	m_ulInputSamples   = 0;
	m_ulMaxOutputBytes = 0;
	m_pOutputBuffer    = NULL;

	m_pDumpFile        = NULL;
	m_pPullProc        = NULL;
	m_pPostProc        = NULL;
	m_pCaller          = NULL;
}

CAudioEncoder::~CAudioEncoder()
{
	Stop();
}

bool CAudioEncoder::Start(ENCODER_PARAM * pEncoderParam, void * pPullProc, void * pPostProc, void * pCaller)
{
	if (m_bRunning)
	{
		LOG(LOG_WARNING, _T("audio encoder already started"));
		return false;
	}
	m_pEncoderParam = pEncoderParam;
	m_pPullProc     = pPullProc;
	m_pPostProc     = pPostProc;
	m_pCaller       = pCaller;

	if (!FaacOpen())
	{
		LOG(LOG_ERROR, _T("faac open failed"));
		return false;
	}

	if (!OS::CThread::Create())
	{
		LOG(LOG_ERROR, _T("OS::CThread::Create failed"));
		return false;
	}

	m_bRunning = true;
	LOG(LOG_DEBUG, _T("audio encoder started. AAC %s quality: %d, bandwidth: %d"),
		m_pEncoderParam->m_nAudioAACObjectType == 1 ? _T("MAIN") : 
		m_pEncoderParam->m_nAudioAACObjectType == 2 ? _T("LC") : 
		m_pEncoderParam->m_nAudioAACObjectType == 3 ? _T("SSR") : 
		m_pEncoderParam->m_nAudioAACObjectType == 4 ? _T("LTP") : _T("UNKNOWN"), 
		m_pEncoderParam->m_nAudioAACQuality, m_pEncoderParam->m_nAudioAACBandwidth);
	return true;
}

void CAudioEncoder::Stop()
{
	if (m_bRunning)
	{
		m_bRunning = false;
		OS::CThread::Close();
		FaacClose();
		LOG(LOG_DEBUG, _T("audio encoder stopped"));
	}
}

bool CAudioEncoder::IsRunning()
{
	return m_bRunning;
}

bool CAudioEncoder::FaacOpen()
{
	faacEncConfigurationPtr pFaacConfig = NULL;

	m_hFaacEncoder = faacEncOpen(m_pEncoderParam->m_nAudioSamplesPerSec, m_pEncoderParam->m_nAudioChannels, &m_ulInputSamples, &m_ulMaxOutputBytes);
	if (!m_hFaacEncoder)
	{
		LOG(LOG_ERROR, _T("faacEncOpen failed"));
		return false;
	}

	pFaacConfig = faacEncGetCurrentConfiguration(m_hFaacEncoder);
	if (!pFaacConfig)
	{
		LOG(LOG_ERROR, _T("faacEncGetCurrentConfiguration failed"));
		return false;
	}
	
	// AAC object type = Low Complexity. MAIN: 1, LOW: 2, SSR: 3, LTP: 4 [default]
	// http://wiki.multimedia.cx/index.php?title=MPEG-4_Audio#Audio_Object_Types
	pFaacConfig->aacObjectType = m_pEncoderParam->m_nAudioAACObjectType;
	pFaacConfig->mpegVersion   = MPEG4; // MPEG version, 2 or 4. MPEG2: 1, MPEG4: 0 [default]
	pFaacConfig->useTns        = 1;     // Use Temporal Noise Shaping
	pFaacConfig->allowMidside  = 1;     // Allow mid/side coding
	
	// Note: VBR output bitrate depends on quality AND bandwidth.
	// quality: quantizer quality. default: 100, range: [10-500].
	// bandwidth: AAC file frequency bandwidth in Hz. default value depends on sample rate, range: [100-16000].
	// quality: 100 and bandwidth: 16000 introduce 128 kbps VBR for a stereo input at 16 bit and 44.1 kHz sample rate.
	// quality: 100 bandwidth: 16000 (~129 kbps)
	// quality: 90  bandwidth: 14000 (~103 kbps)
	// quality: 80  bandwidth: 12000 (~79 kbps)
	// quality: 70  bandwidth: 10000 (~62 kbps)
	pFaacConfig->bandWidth     = m_pEncoderParam->m_nAudioAACBandwidth;
	pFaacConfig->quantqual     = m_pEncoderParam->m_nAudioAACQuality;

	// BitStream output format (0 = Raw; 1 = ADTS [default] )
	// Audio Data Transport Stream (ADTS) is a format, used by MPEG TS or Shoutcast to stream audio, usually AAC.
	// http://wiki.multimedia.cx/index.php?title=ADTS
	pFaacConfig->outputFormat = 0;

	// PCM Sample Input Format
	if (m_pEncoderParam->m_nAudioBytesPerSample == 2)
		pFaacConfig->inputFormat = FAAC_INPUT_16BIT; // native endian 16bit
	else if (m_pEncoderParam->m_nAudioBytesPerSample == 3)
		pFaacConfig->inputFormat = FAAC_INPUT_24BIT; // native endian 24bit in 24 bits (not implemented)
	else if (m_pEncoderParam->m_nAudioBytesPerSample == 4)
		pFaacConfig->inputFormat = FAAC_INPUT_32BIT; // native endian 24bit in 32 bits (DEFAULT)

	if (faacEncSetConfiguration(m_hFaacEncoder, pFaacConfig) == 0)
	{
		LOG(LOG_ERROR, _T("faacEncSetConfiguration failed"));
		return false;
	}

	if (!m_pOutputBuffer && m_ulMaxOutputBytes > 0)
		m_pOutputBuffer = new BYTE[m_ulMaxOutputBytes];

	return true;
}

void CAudioEncoder::FaacClose()
{
	if (m_hFaacEncoder)
	{
		faacEncClose(m_hFaacEncoder);
		m_hFaacEncoder = NULL;
	}
	if (m_pOutputBuffer)
	{
		delete m_pOutputBuffer;
		m_pOutputBuffer = NULL;
	}
	if (m_pDumpFile)
	{
		fclose(m_pDumpFile);
		m_pDumpFile = NULL;
	}
}

int CAudioEncoder::EncodeFrame(BYTE * pFrameData, int nFrameSize, int nFrameTime)
{
	int nEncodeSize = 0;

	if (!m_bRunning || (pFrameData == NULL) || (nFrameSize <= 0))
	{
		LOG(LOG_TRACE, _T("audio encoder state: %s, input data: %p, size: %d, stamp: %d, IGNORED"), 
			(m_bRunning ? _T("on") : _T("off")), pFrameData, nFrameSize, nFrameTime);
		return -1;
	}

	nEncodeSize = faacEncEncode(m_hFaacEncoder, (int32_t *)pFrameData, m_ulInputSamples, m_pOutputBuffer, m_ulMaxOutputBytes);
	if (nEncodeSize <= 0)
	{
		LOG(LOG_TRACE, _T("faac encode input size: %d, output size: %d, stamp: %d ms"), nFrameSize, nEncodeSize, nFrameTime);
		return nEncodeSize;
	}

#if 0
	LOG(LOG_TRACE, _T("faac encode input size: %d, output size: %d, stamp: %d ms"), nFrameSize, nEncodeSize, nFrameTime);
#endif

	if (m_pEncoderParam->m_bAudioAACFile)
	{
		if (!m_pDumpFile)
		{
			char szFileName[64] = {0};
			sprintf(szFileName, "%d_%s.aac", m_pEncoderParam->m_nAudioSamplesPerSec, (m_pEncoderParam->m_nAudioChannels > 1 ? "stereo" : "mono"));
			m_pDumpFile = fopen(szFileName, "wb");
		}
		if (m_pDumpFile)
			fwrite(m_pOutputBuffer, nEncodeSize, 1, m_pDumpFile);
	}
	if (m_pPostProc)
	{
		// If the output format is ADTS, we should remove ADTS header here.
		((ENCODER_ENC_POST_PROC)m_pPostProc)(m_pCaller, m_pOutputBuffer, nEncodeSize, nFrameTime, 0, 0);
	}

	return nEncodeSize;
}

DWORD CAudioEncoder::ThreadProc()
{
	BYTE * pFrameData = NULL;
	int    nFrameSize = 0; 
	int    nFrameTime = 0;
	int    nEncodeSize = 0;
	bool   bRet       = false;

	while (m_bRunning)
	{
		bRet = ((ENCODER_RAW_PULL_PROC)m_pPullProc)(m_pCaller, &pFrameData, &nFrameSize, &nFrameTime);
		if (bRet)
		{
			nEncodeSize = EncodeFrame(pFrameData, nFrameSize, nFrameTime);
			if (nEncodeSize > 0)
			{
				m_pEncoderParam->m_ullAudioEncode ++;
				m_pEncoderParam->m_ullAudioBytes += nEncodeSize;
			}
			else
			{
				m_pEncoderParam->m_ullAudioDrop ++;
			}
		}
	}

	return 0;
}
