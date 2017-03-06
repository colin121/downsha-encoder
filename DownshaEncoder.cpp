#include "stdafx.h"
#include "DownshaEncoder.h"
#include "adifall.ext"
#include "DownshaQueue.h"
#include "3glive.h"
#include "Utils.h"
#include "Log.h"

CDownshaEncoder::CDownshaEncoder()
{
	m_bRunning        = false;
	m_pConfMgmt       = NULL;
	m_pEncoderParam   = NULL;
	m_pDshowCapture   = NULL;
	m_pDownshaOutput  = NULL;

	m_pAudioGrabber   = NULL;
	m_pAudioEncoder   = NULL;
	m_pAudioQueue     = NULL;

	m_pVideoGrabber   = NULL;
	m_pVideoEncoder   = NULL;
	m_pColorConvertor = NULL;
	m_pVideoQueue     = NULL;
}

CDownshaEncoder::~CDownshaEncoder()
{
	Stop();
}

bool CDownshaEncoder::InitParam(LPCSTR szParamFile)
{
	char * pValue = NULL;
	int    nValue = 0;

	if (!m_pEncoderParam) m_pEncoderParam = new ENCODER_PARAM;
	m_pEncoderParam->m_bVideoEnable         = true;       // default: turn on
	memset(m_pEncoderParam->m_szVideoDeviceName, 0, sizeof(m_pEncoderParam->m_szVideoDeviceName));
	m_pEncoderParam->m_bVideoPreview        = false;      // default: turn off
	m_pEncoderParam->m_nVideoWidth          = 720;        // default: 720x576 (D1 standard)
	m_pEncoderParam->m_nVideoHeight         = 576;        // default: 720x576 (D1 standard)
	m_pEncoderParam->m_bVideoFPSStrict      = false;      // default: turn off strict mode
	m_pEncoderParam->m_nVideoFPS            = 25;         // default: 25 fps
	m_pEncoderParam->m_bVideoDropRaw        = false;      // default: turn off
	m_pEncoderParam->m_bVideoDropFPS        = false;      // default: turn off
	m_pEncoderParam->m_nVideoDropNum        = 0;          // default: 0
	m_pEncoderParam->m_nVideoDropDen        = 1;          // default: 1
	m_pEncoderParam->m_nVideoCSP            = ENCODER_CSP_NONE;
	m_pEncoderParam->m_nVideoRawFrameSize   = 0;          // calculated by capture
	m_pEncoderParam->m_nVideoRawQueueSize   = 25;         // default: 25

	m_pEncoderParam->m_bVideoRawFile        = false;      // default: false
	m_pEncoderParam->m_bVideoAVCFile        = false;      // default: false
	memset(m_pEncoderParam->m_szVideoAVCPreset, 0, sizeof(m_pEncoderParam->m_szVideoAVCPreset));
	strcpy(m_pEncoderParam->m_szVideoAVCPreset, "faster"); // default: faster
	memset(m_pEncoderParam->m_szVideoAVCTune, 0, sizeof(m_pEncoderParam->m_szVideoAVCTune));
	memset(m_pEncoderParam->m_szVideoAVCProfile, 0, sizeof(m_pEncoderParam->m_szVideoAVCProfile));
	strcpy(m_pEncoderParam->m_szVideoAVCProfile, "main"); // default: main
	m_pEncoderParam->m_nVideoAVCKeyInt      = 50;         // default: 50
	m_pEncoderParam->m_nVideoAVCBitRate     = 0;          // default: auto
	m_pEncoderParam->m_nVideoAVCBitRateFactor = 19200;    // default: 1280 * 720 * 25 / 1200
	m_pEncoderParam->m_nVideoAVCBFrames     = 0;          // default: 0

	m_pEncoderParam->m_nVideoAVCSPSLen      = 0;
	memset(m_pEncoderParam->m_pVideoAVCSPS, 0, sizeof(m_pEncoderParam->m_pVideoAVCSPS));
	m_pEncoderParam->m_nVideoAVCPPSLen      = 0;
	memset(m_pEncoderParam->m_pVideoAVCPPS, 0, sizeof(m_pEncoderParam->m_pVideoAVCPPS));
	memset(m_pEncoderParam->m_szVideoAVCParam, 0, sizeof(m_pEncoderParam->m_szVideoAVCParam));

	m_pEncoderParam->m_bAudioEnable         = true;       // default: turn on
	memset(m_pEncoderParam->m_szAudioDeviceName, 0, sizeof(m_pEncoderParam->m_szAudioDeviceName));
	m_pEncoderParam->m_nAudioChannels       = 2;          // default: dual channel
	m_pEncoderParam->m_nAudioSamplesPerSec  = 44100;      // default: 44KHz
	m_pEncoderParam->m_nAudioBytesPerSample = 2;          // default: two bytes
	m_pEncoderParam->m_nAudioBufferSamples  = 1024;       // default: 1024 samples in buffer suggested by FAAC encoder
	m_pEncoderParam->m_nAudioBufferCount    = 6;          // default: 6
	m_pEncoderParam->m_nAudioRawFrameSize   = 0;          // calculated by capture
	m_pEncoderParam->m_nAudioRawQueueSize   = 25;         // default: 25

	m_pEncoderParam->m_bAudioRawFile        = false;      // default: false
	m_pEncoderParam->m_bAudioAACFile        = false;      // default: false
	m_pEncoderParam->m_nAudioAACQuality     = 50;         // default: 50, range: [10-500]
	m_pEncoderParam->m_nAudioAACBandwidth   = 2000;       // default: 2000, range: [100-16000]
	m_pEncoderParam->m_nAudioAACObjectType  = 2;          // default: 2 (AAC Low Complexity)

	m_pEncoderParam->m_bOutputEnable        = true;       // default: turn on
	m_pEncoderParam->m_nOutputQueueSize     = 2000;       // default: 2000
	memset(m_pEncoderParam->m_szOutputHost, 0, sizeof(m_pEncoderParam->m_szOutputHost));
	strcpy(m_pEncoderParam->m_szOutputHost, "127.0.0.1"); // default: 127.0.0.1
	m_pEncoderParam->m_usOutputPort         = 2610;       // default: 2610

	m_pEncoderParam->m_bDebugEnable         = false;      // default: turn off

	m_pConfMgmt = conf_mgmt_init((char *)szParamFile);
	if (m_pConfMgmt)
	{
		// video settings
		nValue = conf_get_int(m_pConfMgmt, "Video", "Enable");
		if (nValue >= 0) m_pEncoderParam->m_bVideoEnable = (nValue > 0) ? true : false;
		pValue = (char *)conf_get_string(m_pConfMgmt, "Video", "DeviceName");
		if (pValue != NULL)
		{
			if (pValue[0] != '"') strcpy(m_pEncoderParam->m_szVideoDeviceName, pValue);
			else strncpy(m_pEncoderParam->m_szVideoDeviceName, pValue + 1, strlen(pValue) - 2);
		}
		nValue = conf_get_int(m_pConfMgmt, "Video", "Preview");
		if (nValue >= 0) m_pEncoderParam->m_bVideoPreview = (nValue > 0) ? true : false;
		nValue = conf_get_int(m_pConfMgmt, "Video", "Width");
		if (nValue > 0) m_pEncoderParam->m_nVideoWidth = nValue;
		nValue = conf_get_int(m_pConfMgmt, "Video", "Height");
		if (nValue > 0) m_pEncoderParam->m_nVideoHeight = nValue;
		nValue = conf_get_int(m_pConfMgmt, "Video", "FPSStrict");
		if (nValue >= 0) m_pEncoderParam->m_bVideoFPSStrict = (nValue > 0) ? true : false;
		nValue = conf_get_int(m_pConfMgmt, "Video", "FPS");
		if (nValue > 0) m_pEncoderParam->m_nVideoFPS = nValue;
		nValue = conf_get_int(m_pConfMgmt, "Video", "DropRaw");
		if (nValue >= 0) m_pEncoderParam->m_bVideoDropRaw = (nValue > 0) ? true : false;
		nValue = conf_get_int(m_pConfMgmt, "Video", "DropFPS");
		if (nValue >= 0) m_pEncoderParam->m_bVideoDropFPS = (nValue > 0) ? true : false;
		nValue = conf_get_int(m_pConfMgmt, "Video", "DropNum");
		if (nValue >= 0) m_pEncoderParam->m_nVideoDropNum = nValue;
		nValue = conf_get_int(m_pConfMgmt, "Video", "DropDen");
		if (nValue > 0) m_pEncoderParam->m_nVideoDropDen = nValue;
		pValue = (char *)conf_get_string(m_pConfMgmt, "Video", "CSP");
		if (pValue != NULL) m_pEncoderParam->m_nVideoCSP = GetCSP(pValue);
		nValue = conf_get_int(m_pConfMgmt, "Video", "RawQueueSize");
		if (nValue > 0) m_pEncoderParam->m_nVideoRawQueueSize = nValue;
		// video encoder settings
		nValue = conf_get_int(m_pConfMgmt, "Video", "DumpRawFile");
		if (nValue >= 0) m_pEncoderParam->m_bVideoRawFile = (nValue > 0) ? true : false;
		nValue = conf_get_int(m_pConfMgmt, "Video", "DumpAVCFile");
		if (nValue >= 0) m_pEncoderParam->m_bVideoAVCFile = (nValue > 0) ? true : false;
		pValue = (char *)conf_get_string(m_pConfMgmt, "Video", "AVCPreset");
		if (pValue != NULL) strcpy(m_pEncoderParam->m_szVideoAVCPreset, pValue);
		pValue = (char *)conf_get_string(m_pConfMgmt, "Video", "AVCTune");
		if (pValue != NULL) strcpy(m_pEncoderParam->m_szVideoAVCTune, pValue);
		pValue = (char *)conf_get_string(m_pConfMgmt, "Video", "AVCProfile");
		if (pValue != NULL) strcpy(m_pEncoderParam->m_szVideoAVCProfile, pValue);
		nValue = conf_get_int(m_pConfMgmt, "Video", "AVCKeyInt");
		if (nValue > 0) m_pEncoderParam->m_nVideoAVCKeyInt = nValue;
		nValue = conf_get_int(m_pConfMgmt, "Video", "AVCBitRateFactor");
		if (nValue > 0) m_pEncoderParam->m_nVideoAVCBitRateFactor = nValue;
		nValue = conf_get_int(m_pConfMgmt, "Video", "AVCBFrames");
		if (nValue >= 0) m_pEncoderParam->m_nVideoAVCBFrames = nValue;
		// audio settings
		nValue = conf_get_int(m_pConfMgmt, "Audio", "Enable");
		if (nValue >= 0) m_pEncoderParam->m_bAudioEnable = (nValue > 0) ? true : false;
		pValue = (char *)conf_get_string(m_pConfMgmt, "Audio", "DeviceName");
		if (pValue != NULL)
		{
			if (pValue[0] != '"') strcpy(m_pEncoderParam->m_szAudioDeviceName, pValue);
			else strncpy(m_pEncoderParam->m_szAudioDeviceName, pValue + 1, strlen(pValue) - 2);
		}
		nValue = conf_get_int(m_pConfMgmt, "Audio", "Channels");
		if (nValue > 0) m_pEncoderParam->m_nAudioChannels = nValue;
		nValue = conf_get_int(m_pConfMgmt, "Audio", "SampleRate");
		if (nValue > 0) m_pEncoderParam->m_nAudioSamplesPerSec  = nValue;
		nValue = conf_get_int(m_pConfMgmt, "Audio", "RawQueueSize");
		if (nValue > 0) m_pEncoderParam->m_nAudioRawQueueSize = nValue;
		// audio encoder settings
		nValue = conf_get_int(m_pConfMgmt, "Audio", "DumpRawFile");
		if (nValue >= 0) m_pEncoderParam->m_bAudioRawFile = (nValue > 0) ? true : false;
		nValue = conf_get_int(m_pConfMgmt, "Audio", "DumpAACFile");
		if (nValue >= 0) m_pEncoderParam->m_bAudioAACFile = (nValue > 0) ? true : false;
		nValue = conf_get_int(m_pConfMgmt, "Audio", "AACQuality");
		if (nValue > 0) m_pEncoderParam->m_nAudioAACQuality = nValue;
		nValue = conf_get_int(m_pConfMgmt, "Audio", "AACBandwidth");
		if (nValue > 0) m_pEncoderParam->m_nAudioAACBandwidth = nValue;
		nValue = conf_get_int(m_pConfMgmt, "Audio", "AACObjectType");
		if (nValue > 0) m_pEncoderParam->m_nAudioAACObjectType = nValue;
		// output settings
		nValue = conf_get_int(m_pConfMgmt, "Output", "Enable");
		if (nValue >= 0) m_pEncoderParam->m_bOutputEnable = (nValue > 0) ? true : false;
		nValue = conf_get_int(m_pConfMgmt, "Output", "QueueSize");
		if (nValue > 0) m_pEncoderParam->m_nOutputQueueSize = nValue;
		pValue = (char *)conf_get_string(m_pConfMgmt, "Output", "ServerHost");
		if (pValue != NULL) strcpy(m_pEncoderParam->m_szOutputHost, pValue);
		nValue = conf_get_int(m_pConfMgmt, "Output", "ServerPort");
		if (nValue > 0) m_pEncoderParam->m_usOutputPort = nValue;
		// debug settings
		nValue = conf_get_int(m_pConfMgmt, "Debug", "Enable");
		if (nValue >= 0) m_pEncoderParam->m_bDebugEnable = (nValue > 0) ? true : false;
	}

	m_pEncoderParam->m_ullStartTime         = 0;
	m_pEncoderParam->m_ullElapseTime        = 0;
	m_pEncoderParam->m_ullPrevTime          = 0;

	m_pEncoderParam->m_ullTotalBytes        = 0;
	m_pEncoderParam->m_ullVideoBytes        = 0;
	m_pEncoderParam->m_ullAudioBytes        = 0;
	m_pEncoderParam->m_dblTotalRate         = 0.0;
	m_pEncoderParam->m_dblVideoRate         = 0.0;
	m_pEncoderParam->m_dblAudioRate         = 0.0;

	m_pEncoderParam->m_ullVideoEncode       = 0;
	m_pEncoderParam->m_ullAudioEncode       = 0;
	m_pEncoderParam->m_ullVideoDrop         = 0;
	m_pEncoderParam->m_ullAudioDrop         = 0;
	m_pEncoderParam->m_dblVideoFPS          = 0.0;
	m_pEncoderParam->m_dblAudioFPS          = 0.0;

	m_pEncoderParam->m_ullVideoIFrames      = 0;
	m_pEncoderParam->m_ullVideoPFrames      = 0;
	m_pEncoderParam->m_ullVideoBFrames      = 0;

	return true;
}

void CDownshaEncoder::CleanParam()
{
	if (m_pConfMgmt)
	{
		conf_mgmt_cleanup(m_pConfMgmt);
		m_pConfMgmt = NULL;
	}
	if (m_pEncoderParam)
	{
		delete m_pEncoderParam;
		m_pEncoderParam = NULL;
	}
}

int CDownshaEncoder::GetCSP(const char * str)
{
	int csp = ENCODER_CSP_NONE;

	if (str == NULL || !stricmp(str, ""))
		csp = ENCODER_CSP_NONE;
	else if (!stricmp(str, "CLPL"))
		csp = ENCODER_CSP_CLPL;
	else if (!stricmp(str, "YUY2") || !stricmp(str, "YUYV"))
		csp = ENCODER_CSP_YUYV;
	else if (!stricmp(str, "IYUV"))
		csp = ENCODER_CSP_IYUV;
	else if (!stricmp(str, "YVU9"))
		csp = ENCODER_CSP_YVU9;
	else if (!stricmp(str, "Y411") || !stricmp(str, "Y41P"))
		csp = ENCODER_CSP_Y411;
	else if (!stricmp(str, "YVYU"))
		csp = ENCODER_CSP_YVYU;
	else if (!stricmp(str, "UYVY"))
		csp = ENCODER_CSP_UYVY;
	else if (!stricmp(str, "Y211"))
		csp = ENCODER_CSP_Y211;
	else if (!stricmp(str, "CLJR"))
		csp = ENCODER_CSP_CLJR;
	else if (!stricmp(str, "IF09"))
		csp = ENCODER_CSP_IF09;
	else if (!stricmp(str, "CPLA"))
		csp = ENCODER_CSP_CPLA;
	else if (!stricmp(str, "RGB1"))
		csp = ENCODER_CSP_RGB1;
	else if (!stricmp(str, "RGB4"))
		csp = ENCODER_CSP_RGB4;
	else if (!stricmp(str, "RGB8"))
		csp = ENCODER_CSP_RGB8;
	else if (!stricmp(str, "RGB555"))
		csp = ENCODER_CSP_RGB555;
	else if (!stricmp(str, "RGB565"))
		csp = ENCODER_CSP_RGB565;
	else if (!stricmp(str, "RGB24"))
		csp = ENCODER_CSP_RGB24;
	else if (!stricmp(str, "RGB32"))
		csp = ENCODER_CSP_RGB32;
	else if (!stricmp(str, "ARGB1555"))
		csp = ENCODER_CSP_ARGB1555;
	else if (!stricmp(str, "ARGB32"))
		csp = ENCODER_CSP_ARGB32;
	else if (!stricmp(str, "ARGB4444"))
		csp = ENCODER_CSP_ARGB4444;
	else if (!stricmp(str, "A2R10G10B10"))
		csp = ENCODER_CSP_A2R10G10B10;
	else if (!stricmp(str, "A2B10G10R10"))
		csp = ENCODER_CSP_A2B10G10R10;

	return csp;
}

void CDownshaEncoder::PrintStat()
{
	char       szEncoder[256] = {0};
	char       szOutput[256]  = {0};
	char       szIPTerm[32]   = {0};
	int        nElapse        = 0;
	int        nIndex         = 0;
	SessStat * pSessStat      = NULL;

	m_pEncoderParam->m_ullElapseTime = Utils::GetSystemMilliSeconds();
	m_pEncoderParam->m_ullElapseTime -= m_pEncoderParam->m_ullStartTime;

	if (m_pEncoderParam->m_ullPrevTime > 0 && (m_pEncoderParam->m_ullElapseTime - m_pEncoderParam->m_ullPrevTime < 1000))
		return ;

	m_pEncoderParam->m_ullPrevTime = m_pEncoderParam->m_ullElapseTime;
	m_pEncoderParam->m_dblVideoRate = (m_pEncoderParam->m_ullElapseTime > 0) ?
		((m_pEncoderParam->m_ullVideoBytes / m_pEncoderParam->m_ullElapseTime) * (8000. / 1024)) : 0;
	m_pEncoderParam->m_dblAudioRate = (m_pEncoderParam->m_ullElapseTime > 0) ?
		((m_pEncoderParam->m_ullAudioBytes / m_pEncoderParam->m_ullElapseTime) * (8000. / 1024)) : 0;
	m_pEncoderParam->m_dblTotalRate  = m_pEncoderParam->m_dblVideoRate + m_pEncoderParam->m_dblAudioRate;
	m_pEncoderParam->m_ullTotalBytes = m_pEncoderParam->m_ullVideoBytes + m_pEncoderParam->m_ullAudioBytes;

	m_pEncoderParam->m_dblVideoFPS = (m_pEncoderParam->m_ullElapseTime > 0) ? 
		(m_pEncoderParam->m_ullVideoEncode * 1000. / m_pEncoderParam->m_ullElapseTime) : 0;
	m_pEncoderParam->m_dblAudioFPS = (m_pEncoderParam->m_ullElapseTime > 0) ?
		(m_pEncoderParam->m_ullAudioEncode * 1000. / m_pEncoderParam->m_ullElapseTime) : 0;

	nElapse = m_pEncoderParam->m_ullElapseTime / 1000;
	sprintf(szEncoder, "DownshaEncoder %I64u[I:%I64u]/%I64u %.1f fps %.1f kb/s %.1f MB %d:%02d:%02d", 
		m_pEncoderParam->m_ullVideoEncode, m_pEncoderParam->m_ullVideoIFrames, m_pEncoderParam->m_ullVideoDrop, 
		m_pEncoderParam->m_dblVideoFPS, m_pEncoderParam->m_dblTotalRate, (m_pEncoderParam->m_ullTotalBytes * 1. / (1024 * 1024)), 
		nElapse / 3600, (nElapse / 60) % 60, nElapse % 60);
	SetConsoleTitleA(szEncoder);

	if (m_pDownshaOutput != NULL)
	{
		pSessStat = (SessStat * )m_pDownshaOutput->GetStat();
		if (pSessStat != NULL)
		{
			sprintf(szOutput, "ID:%d FIFO:%d/%d/%d PDU:%d/%d/%d %.0fk %d@%d", 
				pSessStat->sessid, pSessStat->nalu_num, pSessStat->last_naluid, pSessStat->bgn_nalu, 
				pSessStat->wait_resp_num, pSessStat->wait_pack_num, pSessStat->nalu_pool_num,
				pSessStat->send_rate * 8., pSessStat->tcpnum, pSessStat->termnum);
			for (nIndex = 0; nIndex < pSessStat->termnum; nIndex ++)
			{
				sprintf(szIPTerm, " %s:%s#%.0fk", 
					(pSessStat->ipterm[nIndex].iptype == ADDR_TYPE_ETHERNET) ? "E" : 
					(pSessStat->ipterm[nIndex].iptype == ADDR_TYPE_PPP)      ? "P" : 
					(pSessStat->ipterm[nIndex].iptype == ADDR_TYPE_SLIP)     ? "S" : 
					(pSessStat->ipterm[nIndex].iptype == ADDR_TYPE_WIFI)     ? "W" : "U",
					inet_ntoa(pSessStat->ipterm[nIndex].ip), 
					pSessStat->ipterm[nIndex].send_rate * 8.);
				strcat(szOutput, szIPTerm);
			}
			fprintf(stderr, "%s\t\t\r", szOutput);
			fflush(stderr);
		}
	}
}

bool CDownshaEncoder::Start(LPCSTR szParamFile)
{
	if (m_bRunning)
	{
		LOG(LOG_WARNING, _T("encoder already started"));
		return false;
	}
	if (!InitParam(szParamFile))
	{
		LOG(LOG_ERROR, _T("parse param failed"));
		return false;
	}

	if (m_pEncoderParam->m_bVideoEnable)
	{
		if (!m_pVideoGrabber)
		{
			m_pVideoGrabber = new CVideoGrabber();
			m_pVideoGrabber->AddRef();
		}
	}
	if (m_pEncoderParam->m_bAudioEnable)
	{
		if (!m_pAudioGrabber)
		{
			m_pAudioGrabber = new CAudioGrabber();
			m_pAudioGrabber->AddRef();
		}
	}
	if (!m_pDshowCapture) m_pDshowCapture = new CDshowCapture();
	if (!m_pDshowCapture->Start(m_pEncoderParam, m_pVideoGrabber, m_pAudioGrabber))
	{
		LOG(LOG_ERROR, _T("dshow capture start failed"));
		return false;
	}
	// save the start time of direct show capture
	m_pEncoderParam->m_ullStartTime = Utils::GetSystemMilliSeconds();

	if (m_pEncoderParam->m_bVideoEnable)
	{
		LOG(LOG_DEBUG, _T("video raw queue size: %d, frame size: %d"), m_pEncoderParam->m_nVideoRawQueueSize, m_pEncoderParam->m_nVideoRawFrameSize);
		if (!m_pVideoQueue) m_pVideoQueue = downsha_queue_init(m_pEncoderParam->m_nVideoRawQueueSize, m_pEncoderParam->m_nVideoRawFrameSize);
		if (!m_pVideoQueue)
		{
			LOG(LOG_ERROR, _T("downsha_queue_init failed"));
			return false;
		}
		if (!m_pVideoGrabber->Start(m_pEncoderParam, CDownshaEncoder::VideoRawPushProc, this))
		{
			LOG(LOG_ERROR, _T("video grabber start failed"));
			return false;
		}
		if (!m_pColorConvertor) m_pColorConvertor = new CColorConvertor();
		if (!m_pColorConvertor->Start(m_pEncoderParam))
		{
			LOG(LOG_ERROR, _T("color convertor start failed"));
			return false;
		}
		if (!m_pVideoEncoder) m_pVideoEncoder = new CVideoEncoder();
		if (!m_pVideoEncoder->Start(m_pEncoderParam, CDownshaEncoder::VideoRawPullProc, CDownshaEncoder::VideoEncPostProc, this))
		{
			LOG(LOG_ERROR, _T("video encoder start failed"));
			return false;
		}
	}

	if (m_pEncoderParam->m_bAudioEnable)
	{
		LOG(LOG_DEBUG, _T("audio raw queue size: %d, frame size: %d"), m_pEncoderParam->m_nAudioRawQueueSize, m_pEncoderParam->m_nAudioRawFrameSize);
		if (!m_pAudioQueue) m_pAudioQueue = downsha_queue_init(m_pEncoderParam->m_nAudioRawQueueSize, m_pEncoderParam->m_nAudioRawFrameSize);
		if (!m_pAudioQueue)
		{
			LOG(LOG_ERROR, _T("downsha_queue_init failed"));
			return false;
		}
		if (!m_pAudioGrabber->Start(m_pEncoderParam, CDownshaEncoder::AudioRawPushProc, this))
		{
			LOG(LOG_ERROR, _T("audio grabber start failed"));
			return false;
		}
		if (!m_pAudioEncoder) m_pAudioEncoder = new CAudioEncoder();
		if (!m_pAudioEncoder->Start(m_pEncoderParam, CDownshaEncoder::AudioRawPullProc, CDownshaEncoder::AudioEncPostProc, this))
		{
			LOG(LOG_ERROR, _T("audio encoder start failed"));
			return false;
		}
	}

	if (m_pEncoderParam->m_bOutputEnable)
	{
		if (!m_pDownshaOutput) m_pDownshaOutput = new CDownshaOutput();
		if (!m_pDownshaOutput->Start(m_pEncoderParam))
		{
			LOG(LOG_ERROR, _T("output start failed"));
			return false;
		}
	}

	m_bRunning = true;
	LOG(LOG_DEBUG, _T("encoder started. start time: %I64u"), m_pEncoderParam->m_ullStartTime);
	return true;
}

void CDownshaEncoder::Stop()
{
	if (m_bRunning)
	{
		LOG(LOG_DEBUG, _T("encoder terminating"));
		m_bRunning = false;
	}
	if (m_pDshowCapture)
	{
		m_pDshowCapture->Stop();
		delete m_pDshowCapture;
		m_pDshowCapture = NULL;
	}
	if (m_pAudioGrabber)
	{
		m_pAudioGrabber->Stop();
		m_pAudioGrabber->Release();
		m_pAudioGrabber = NULL;
	}
	if (m_pVideoGrabber)
	{
		m_pVideoGrabber->Stop();
		m_pVideoGrabber->Release();
		m_pVideoGrabber = NULL;
	}
	if (m_pAudioEncoder)
	{
		m_pAudioEncoder->Stop();
		delete m_pAudioEncoder;
		m_pAudioEncoder = NULL;
	}
	if (m_pVideoEncoder)
	{
		m_pVideoEncoder->Stop();
		delete m_pVideoEncoder;
		m_pVideoEncoder = NULL;
	}
	if (m_pColorConvertor)
	{
		m_pColorConvertor->Stop();
		delete m_pColorConvertor;
		m_pColorConvertor = NULL;
	}
	if (m_pAudioQueue)
	{
		downsha_queue_clean(m_pAudioQueue);
		m_pAudioQueue = NULL;
	}
	if (m_pVideoQueue)
	{
		downsha_queue_clean(m_pVideoQueue);
		m_pVideoQueue = NULL;
	}
	if (m_pDownshaOutput)
	{
		m_pDownshaOutput->Stop();
		delete m_pDownshaOutput;
		m_pDownshaOutput = NULL;
	}

	CleanParam();
}

bool CDownshaEncoder::IsRunning()
{
	return m_bRunning;
}

bool CDownshaEncoder::AudioRawPushProc(void * pCaller, BYTE * pData, int nSize, int nStamp)
{
	CDownshaEncoder * pThis = NULL;
	int               nRet  = 0;
	bool              bRet  = false;

	pThis = (CDownshaEncoder *)pCaller;
	if (pThis == NULL)
		return false;

	if (!pData || nSize <= 0)
		return false;

	if (pThis->m_pAudioQueue)
	{
		nRet = downsha_queue_push(pThis->m_pAudioQueue, pData, nSize, nStamp);
		bRet = (nRet >= 0) ? true : false;
		if (!bRet) pThis->m_pEncoderParam->m_ullAudioDrop ++;
	}

	return bRet;
}

bool CDownshaEncoder::VideoRawPushProc(void * pCaller, BYTE * pData, int nSize, int nStamp)
{
	CDownshaEncoder * pThis = NULL;
	int               nRet  = 0;
	bool              bRet  = false;

	pThis = (CDownshaEncoder *)pCaller;
	if (pThis == NULL)
		return false;

	if (!pData || nSize <= 0)
		return false;

	if (pThis->m_pVideoQueue)
	{
		nRet = downsha_queue_push(pThis->m_pVideoQueue, pData, nSize, nStamp);
		bRet = (nRet >= 0) ? true : false;
		if (!bRet) pThis->m_pEncoderParam->m_ullVideoDrop ++;
	}

	return bRet;
}

bool CDownshaEncoder::AudioRawPullProc(void * pCaller, BYTE ** ppData, int * pSize, int * pStamp)
{
	DownshaUnit     * pUnit = NULL;
	CDownshaEncoder * pThis = NULL;
	bool              bRet  = false;

	pThis = (CDownshaEncoder *)pCaller;
	if (pThis == NULL)
		return false;

	if (!ppData || !pSize || !pStamp)
		return false;

	if (pThis->m_pAudioQueue)
	{
		pUnit = (DownshaUnit *)downsha_queue_pull(pThis->m_pAudioQueue);
		if (pUnit && pUnit->data && pUnit->size > 0)
		{
			*ppData = (BYTE *)pUnit->data;
			*pSize  = pUnit->size;
			*pStamp = pUnit->stamp;
			bRet    = true;
		}
	}

	return bRet;
}

bool CDownshaEncoder::VideoRawPullProc(void * pCaller, BYTE ** ppData, int * pSize, int * pStamp)
{
	DownshaUnit     * pUnit = NULL;
	CDownshaEncoder * pThis = NULL;
	bool              bRet  = false;

	pThis = (CDownshaEncoder *)pCaller;
	if (pThis == NULL)
		return false;

	if (!ppData || !pSize || !pStamp)
		return false;

	if (pThis->m_pVideoQueue && pThis->m_pColorConvertor)
	{
		pUnit = (DownshaUnit *)downsha_queue_pull(pThis->m_pVideoQueue);
		if (pUnit && pUnit->data && pUnit->size > 0)
		{
			*pStamp = pUnit->stamp;
			bRet    = pThis->m_pColorConvertor->Convert((BYTE *)pUnit->data, pUnit->size, ppData, pSize);
		}
	}

	return bRet;
}

bool CDownshaEncoder::AudioEncPostProc(void * pCaller, BYTE * pData, int nSize, int nStamp, int nKeyFrame, int nCTOff)
{
	CDownshaEncoder * pThis = NULL;
	int               nRet  = 0;
	bool              bRet  = false;

	pThis = (CDownshaEncoder *)pCaller;
	if (pThis == NULL)
		return false;

	if (!pData || nSize <= 0)
		return false;

	if (pThis->m_pDownshaOutput)
	{
		nRet = pThis->m_pDownshaOutput->PushNalu(1, pData, nSize, nStamp, nKeyFrame, nCTOff);
		bRet = (nRet >= 0) ? true : false;
	}

	return bRet;
}

bool CDownshaEncoder::VideoEncPostProc(void * pCaller, BYTE * pData, int nSize, int nStamp, int nKeyFrame, int nCTOff)
{
	CDownshaEncoder * pThis = NULL;
	int               nRet  = 0;
	bool              bRet  = false;

	pThis = (CDownshaEncoder *)pCaller;
	if (pThis == NULL)
		return false;

	if (!pData || nSize <= 0)
		return false;

	if (pThis->m_pDownshaOutput)
	{
		nRet = pThis->m_pDownshaOutput->PushNalu(0, pData, nSize, nStamp, nKeyFrame, nCTOff);
		bRet = (nRet >= 0) ? true : false;
	}

	return bRet;
}
