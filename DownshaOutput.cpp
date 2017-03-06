#include "stdafx.h"
#include "DownshaOutput.h"
#include "DownshaEncoder.h"
#include "adifall.ext"
#include "probe.h"
#include "3glive.h"
#include "Log.h"

CDownshaOutput::CDownshaOutput()
{
	m_bRunning         = false;
	m_pEncoderParam    = NULL;
	m_pLiveMgmt        = NULL;
	m_pProbeCore       = NULL;
	m_pLiveSess        = NULL;
	m_pLiveStat        = NULL;
}

CDownshaOutput::~CDownshaOutput()
{
	Stop();
}

bool CDownshaOutput::Start(ENCODER_PARAM * pEncoderParam)
{
	if (m_bRunning)
	{
		LOG(LOG_WARNING, _T("output already started"));
		return false;
	}
	m_pEncoderParam = pEncoderParam;

	m_pProbeCore = probe_init(5, 20, 8, 600, 0);
	if (!m_pProbeCore)
	{
		LOG(LOG_ERROR, _T("probe_init failed"));
		return false;
	}

	EncodePara tEncodePara;
	memset(&tEncodePara, 0, sizeof(tEncodePara));
	tEncodePara.start_timestamp  = m_pEncoderParam->m_ullStartTime;
	tEncodePara.has_video        = m_pEncoderParam->m_bVideoEnable ? 1 : 0;
	tEncodePara.has_audio        = m_pEncoderParam->m_bAudioEnable ? 1 : 0;
	tEncodePara.video_width      = m_pEncoderParam->m_nVideoWidth;
	tEncodePara.video_height     = m_pEncoderParam->m_nVideoHeight;
	tEncodePara.video_fps        = m_pEncoderParam->m_nVideoFPS;
	tEncodePara.audio_samplerate = m_pEncoderParam->m_nAudioSamplesPerSec;

	tEncodePara.avc_sps_len      = m_pEncoderParam->m_nVideoAVCSPSLen;
	if (tEncodePara.avc_sps_len > 0 && tEncodePara.avc_sps_len < sizeof(tEncodePara.avc_sps))
		memcpy(tEncodePara.avc_sps, m_pEncoderParam->m_pVideoAVCSPS, tEncodePara.avc_sps_len);
	tEncodePara.avc_pps_len      = m_pEncoderParam->m_nVideoAVCPPSLen;
	if (tEncodePara.avc_pps_len > 0 && tEncodePara.avc_pps_len < sizeof(tEncodePara.avc_pps))
		memcpy(tEncodePara.avc_pps, m_pEncoderParam->m_pVideoAVCPPS, tEncodePara.avc_pps_len);

	// 1-AAC MAIN, 2-AAC LC, 3-AAC SSR, 4-AAC LTP
	// http://wiki.multimedia.cx/index.php?title=MPEG-4_Audio#Audio_Object_Types
	tEncodePara.aac_objtype      = m_pEncoderParam->m_nAudioAACObjectType;

	// 1-mono, 2-stereo
	// http://wiki.multimedia.cx/index.php?title=MPEG-4_Audio#Channel_Configurations
	tEncodePara.aac_channel      = m_pEncoderParam->m_nAudioChannels;

	// 0=96000Hz, 1=88200Hz, 2=64000Hz, 3=48000Hz, 4=44100Hz
	// http://wiki.multimedia.cx/index.php?title=MPEG-4_Audio#Sampling_Frequencies
	switch (m_pEncoderParam->m_nAudioSamplesPerSec)
	{
	case 44100: tEncodePara.aac_samplefreqind = 4; break;
	case 48000: tEncodePara.aac_samplefreqind = 3; break;
	case 64000: tEncodePara.aac_samplefreqind = 2; break;
	case 88200: tEncodePara.aac_samplefreqind = 1; break;
	case 96000: tEncodePara.aac_samplefreqind = 0; break;
	}
	m_pLiveMgmt = live_mgmt_init(m_pProbeCore, NULL);
	if (!m_pLiveMgmt)
	{
		LOG(LOG_ERROR, _T("live_mgmt_init failed"));
		return false;
	}
	m_pLiveSess = live_sess_open(m_pLiveMgmt, &tEncodePara, m_pEncoderParam->m_nOutputQueueSize, (uint8 *)m_pEncoderParam->m_szOutputHost, m_pEncoderParam->m_usOutputPort);
	if (!m_pLiveSess)
	{
		LOG(LOG_ERROR, _T("live_sess_open failed"));
		return false;
	}
	m_pLiveStat = malloc(sizeof(SessStat));
	if (!m_pLiveStat)
	{
		LOG(LOG_ERROR, _T("live stat malloc failed"));
		return false;
	}
	StartProbe(m_pProbeCore, 1);

	m_bRunning = true;
	LOG(LOG_DEBUG, _T("output started. server: %S:%u, fifo: %d"), m_pEncoderParam->m_szOutputHost, m_pEncoderParam->m_usOutputPort, m_pEncoderParam->m_nOutputQueueSize);
	return true;
}

void CDownshaOutput::Stop()
{
	if (m_bRunning)
	{
		m_bRunning = false;
		LOG(LOG_DEBUG, _T("output stopped"));
	}
	if (m_pLiveStat)
	{
		free(m_pLiveStat);
		m_pLiveStat = NULL;
	}
	if (m_pLiveSess)
	{
		live_sess_close(m_pLiveSess);
		m_pLiveSess = NULL;
	}
	if (m_pLiveMgmt)
	{
		live_mgmt_clean(m_pLiveMgmt);
		m_pLiveMgmt = NULL;
	}
	if (m_pProbeCore)
	{
		probe_cleanup(m_pProbeCore);
		m_pProbeCore = NULL;
	}
}

bool CDownshaOutput::IsRunning()
{
	return m_bRunning;
}

int CDownshaOutput::PushNalu(unsigned char nMediaType, unsigned char * pData, int nSize, int nStamp, int nKeyFrame, int nCTOff)
{
	LiveNalu nalu;
	int      ret = 0;

	if (!m_bRunning || (pData == NULL) || (nSize <= 0))
	{
		LOG(LOG_TRACE, _T("output state: %s, input data: %p, size: %d, stamp: %d, IGNORED"), 
			(m_bRunning ? _T("on") : _T("off")), pData, nSize, nStamp);
		return -1;
	}

	memset(&nalu, 0, sizeof(LiveNalu));
	nalu.type      = nMediaType; // 0-video, 1-audio
	nalu.timestamp = nStamp;     // milliseconds relative to the start time
	nalu.keyframe  = nKeyFrame;  // 0-no key frame, 1-key frame
	nalu.ctoff     = nCTOff;     // Composition Time Offset
	nalu.pbyte     = pData;      // media raw data
	nalu.bytelen   = nSize;      // media data length

	ret = live_fifo_push(m_pLiveSess, (void *)&nalu);
	if (ret < 0)
	{
		LOG(LOG_TRACE, _T("live_fifo_push return: %d, nalu type: %d, timestamp: %d, bytelen: %d"), 
			ret, nalu.type, nalu.timestamp, nalu.bytelen);
		return ret;
	}

	return 0;
}

void * CDownshaOutput::GetStat()
{
	if (!m_pLiveMgmt || !m_pLiveSess || !m_pLiveStat)
		return NULL;
	
	live_sess_stat(m_pLiveSess, m_pLiveStat);

	return m_pLiveStat;
}
