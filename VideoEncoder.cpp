#include "stdafx.h"
#include "VideoEncoder.h"
#include "DownshaEncoder.h"
#include "Log.h"

CVideoEncoder::CVideoEncoder()
{
	m_bRunning         = false;
	m_pEncoderParam    = NULL;
	m_hX264Encoder     = NULL;
	m_pX264Picture     = NULL;

	m_pDumpFile        = NULL;
	m_pPullProc        = NULL;
	m_pPostProc        = NULL;
	m_pCaller          = NULL;
}

CVideoEncoder::~CVideoEncoder()
{
	Stop();
}

bool CVideoEncoder::Start(ENCODER_PARAM * pEncoderParam, void * pPullProc, void * pPostProc, void * pCaller)
{
	if (m_bRunning)
	{
		LOG(LOG_WARNING, _T("video encoder already started"));
		return false;
	}
	m_pEncoderParam = pEncoderParam;
	m_pPullProc     = pPullProc;
	m_pPostProc     = pPostProc;
	m_pCaller       = pCaller;

	if (X264Open() < 0)
	{
		LOG(LOG_ERROR, _T("x264 open failed"));
		return false;
	}

	if (!OS::CThread::Create())
	{
		LOG(LOG_ERROR, _T("OS::CThread::Create failed"));
		return false;
	}

	m_bRunning = true;
	LOG(LOG_DEBUG, _T("video encoder started. AVC %S keyint: %d, b-frames: %d, bitrate: %d kbps, sps: %d, pps: %d"),
		(0 != strcmp(m_pEncoderParam->m_szVideoAVCProfile, "")) ? _strupr(m_pEncoderParam->m_szVideoAVCProfile) : "null", 
		//(0 != strcmp(m_pEncoderParam->m_szVideoAVCPreset, "")) ? m_pEncoderParam->m_szVideoAVCPreset : "null", 
		//(0 != strcmp(m_pEncoderParam->m_szVideoAVCTune, "")) ? m_pEncoderParam->m_szVideoAVCTune : "null", 
		m_pEncoderParam->m_nVideoAVCKeyInt, m_pEncoderParam->m_nVideoAVCBFrames, m_pEncoderParam->m_nVideoAVCBitRate,
		m_pEncoderParam->m_nVideoAVCSPSLen, m_pEncoderParam->m_nVideoAVCPPSLen);
	return true;
}

void CVideoEncoder::Stop()
{
	if (m_bRunning)
	{
		m_bRunning = false;
		OS::CThread::Close();
		X264Close();
		LOG(LOG_DEBUG, _T("video encoder stopped"));
	}
}

bool CVideoEncoder::IsRunning()
{
	return m_bRunning;
}

int CVideoEncoder::X264Open()
{
	x264_param_t x264Param;
	char         szValue[64] = {0};
	x264_nal_t * pbNalu      = NULL;
	int          cbNalu      = 0;
	const char * szPreset    = NULL;
	const char * szTune      = NULL;
	const char * szProfile   = NULL;

	if (strcmp(m_pEncoderParam->m_szVideoAVCPreset, ""))
		szPreset = m_pEncoderParam->m_szVideoAVCPreset;
	if (strcmp(m_pEncoderParam->m_szVideoAVCTune, ""))
		szTune = m_pEncoderParam->m_szVideoAVCTune;
	if (strcmp(m_pEncoderParam->m_szVideoAVCProfile, ""))
		szProfile = m_pEncoderParam->m_szVideoAVCProfile;

	// Presets are applied before all other options.
	if (x264_param_default_preset(&x264Param, szPreset, szTune) < 0)
	{
		LOG(LOG_ERROR, _T("x264_param_default_preset failed"));
		return -1;
	}
	sprintf(szValue, "%d", m_pEncoderParam->m_nVideoAVCBFrames);
	if (x264_param_parse(&x264Param, "bframes", szValue) < 0)
	{
		LOG(LOG_ERROR, _T("x264_param_parse --bframes=%s failed"), szValue);
		return -1;
	}
	if (m_pEncoderParam->m_nVideoAVCBitRate > 0)
	{
		sprintf(szValue, "%d", m_pEncoderParam->m_nVideoAVCBitRate);
		if (x264_param_parse(&x264Param, "bitrate", szValue) < 0)
		{
			LOG(LOG_ERROR, _T("x264_param_parse --bitrate=%s failed"), szValue);
			return -1;
		}
	}
	if (m_pEncoderParam->m_nVideoFPS > 0)
	{
		sprintf(szValue, "%d/1", m_pEncoderParam->m_nVideoFPS);
		if (x264_param_parse(&x264Param, "fps", szValue) < 0)
		{
			LOG(LOG_ERROR, _T("x264_param_parse --fps=%s failed"), szValue);
			return -1;
		}
	}
	if (m_pEncoderParam->m_nVideoAVCKeyInt > 0)
	{
		sprintf(szValue, "%d", m_pEncoderParam->m_nVideoAVCKeyInt);
		if (x264_param_parse(&x264Param, "keyint", szValue) < 0)
		{
			LOG(LOG_ERROR, _T("x264_param_parse --keyint=%s failed"), szValue);
			return -1;
		}
	}

	// Apply profile restrictions.
	if (x264_param_apply_profile(&x264Param, szProfile) < 0)
	{
		LOG(LOG_ERROR, _T("x264_param_apply_profile failed"));
		return -1;
	}

	// annexb = true: write start-code (0x00000001) to the head of nalu
	// annexb = false: write body-size to the head of nalu
#if 0
	if (x264_param_parse(&x264Param, "annexb", "false") < 0)
	{
		LOG(LOG_ERROR, _T("x264_param_parse --annexb=false failed"));
		return -1;
	}
#endif

	x264Param.i_width          = m_pEncoderParam->m_nVideoWidth;
	x264Param.i_height         = m_pEncoderParam->m_nVideoHeight;
	x264Param.rc.b_mb_tree     = 0;
	x264Param.rc.i_lookahead   = 0;
	x264Param.i_sync_lookahead = 0;
	x264Param.b_vfr_input      = 0;
	if (!m_pEncoderParam->m_bDebugEnable)
		x264Param.i_log_level  = X264_LOG_NONE;

	m_hX264Encoder = x264_encoder_open(&x264Param);
	if (!m_hX264Encoder)
	{
		LOG(LOG_ERROR, _T("x264_encoder_open failed"));
		return -1;
	}

	if (x264_encoder_headers(m_hX264Encoder, &pbNalu, &cbNalu) < 0)
	{
		LOG(LOG_ERROR, _T("x264_encoder_headers failed"));
		return -1;
	}
	if ((pbNalu == NULL) || (cbNalu < 3))
	{
		LOG(LOG_ERROR, _T("header nalu invalid"));
		return -1;
	}

	// NAL[0]: SPS, NAL[1]: PPS, NAL[2]: SEI
	// NOTE: x264_nal_encode() will automatically add 4 or 3 bytes (start code or size depending on annexb) in the head of payload.
	if ((pbNalu[0].p_payload != NULL) && (pbNalu[0].i_payload > 0) && (pbNalu[0].i_payload < sizeof(m_pEncoderParam->m_pVideoAVCSPS)))
	{
		m_pEncoderParam->m_nVideoAVCSPSLen = pbNalu[0].i_payload - 4;
		memcpy(m_pEncoderParam->m_pVideoAVCSPS, pbNalu[0].p_payload + 4, pbNalu[0].i_payload - 4);
	}
	if ((pbNalu[1].p_payload != NULL) && (pbNalu[1].i_payload > 0) && (pbNalu[1].i_payload < sizeof(m_pEncoderParam->m_pVideoAVCPPS)))
	{
		m_pEncoderParam->m_nVideoAVCPPSLen = pbNalu[1].i_payload - 4;
		memcpy(m_pEncoderParam->m_pVideoAVCPPS, pbNalu[1].p_payload + 4, pbNalu[1].i_payload - 4);
	}
	if ((pbNalu[2].p_payload != NULL) && (pbNalu[2].i_payload > 25))
	{
		// Skip 23 bytes, including start code (3 bytes), nalu type (1 bytes), guid (15 bytes), etc
		memset(m_pEncoderParam->m_szVideoAVCParam, 0, sizeof(m_pEncoderParam->m_szVideoAVCParam));
		memcpy(m_pEncoderParam->m_szVideoAVCParam, pbNalu[2].p_payload + 24, pbNalu[2].i_payload - 25);
		if (m_pEncoderParam->m_bDebugEnable)
			LOG(LOG_DEBUG, _T("%S"), m_pEncoderParam->m_szVideoAVCParam);
	}

	if (m_pEncoderParam->m_bVideoAVCFile)
	{
		if (!m_pDumpFile)
		{
			char szFileName[64] = {0};
			sprintf(szFileName, "%dx%dp_%dfps_%dkbps.264", 
				m_pEncoderParam->m_nVideoWidth, m_pEncoderParam->m_nVideoHeight, 
				m_pEncoderParam->m_nVideoFPS, m_pEncoderParam->m_nVideoAVCBitRate);
			m_pDumpFile = fopen(szFileName, "wb");
		}
		if (m_pDumpFile)
		{
			// the payloads of all output NALs are guaranteed to be sequential in memory.
			int nHeaderSize = pbNalu[0].i_payload + pbNalu[1].i_payload + pbNalu[2].i_payload;
			fwrite(pbNalu[0].p_payload, nHeaderSize, 1, m_pDumpFile);
		}
	}
	
	m_pX264Picture = new x264_picture_t;
	if (x264_picture_alloc(m_pX264Picture, X264_CSP_I420, m_pEncoderParam->m_nVideoWidth, m_pEncoderParam->m_nVideoHeight) < 0)
	{
		LOG(LOG_ERROR, _T("x264_picture_alloc failed"));
		return -1;
	}

	return 0;
}

void CVideoEncoder::X264Close()
{
	if (m_hX264Encoder)
	{
		x264_encoder_close(m_hX264Encoder);
		m_hX264Encoder = NULL;
	}
	if (m_pX264Picture)
	{
		x264_picture_clean(m_pX264Picture);
		delete m_pX264Picture;
		m_pX264Picture = NULL;
	}
	if (m_pDumpFile)
	{
		fclose(m_pDumpFile);
		m_pDumpFile = NULL;
	}
}

int CVideoEncoder::EncodeFrame(BYTE * pFrameData, int nFrameSize, int nFrameTime)
{
	x264_picture_t x264PicOut;
	x264_nal_t   * pbNalu         = NULL;
	int            cbNalu         = 0;
	int            nEncodeSize    = 0;
	int            nIndex         = 0;
	int            nCTOff         = 0;    // composition time offset
	bool           bLongStartCode = true; // long start code
	int            nPayloadOffset = 0;    // payload offset

	if (!m_bRunning || (pFrameData == NULL) || (nFrameSize <= 0))
	{
		LOG(LOG_TRACE, _T("video encoder state: %s, input data: %p, size: %d, stamp: %d, IGNORED"), 
			(m_bRunning ? _T("on") : _T("off")), pFrameData, nFrameSize, nFrameTime);
		return -1;
	}

	memcpy(m_pX264Picture->img.plane[0], pFrameData, nFrameSize);
	nEncodeSize = x264_encoder_encode(m_hX264Encoder, &pbNalu, &cbNalu, m_pX264Picture, &x264PicOut);
	if (nEncodeSize <= 0)
	{
		LOG(LOG_TRACE, _T("x264 encode input size: %d, output size: %d, nalu count: %d, stamp: %d"), nFrameSize, nEncodeSize, cbNalu, nFrameTime);
		return nEncodeSize;
	}

#if 0
	LOG(LOG_TRACE, _T("x264 encode input size: %d, output size: %d, nalu count: %d, stamp: %d"), nFrameSize, nEncodeSize, cbNalu, nFrameTime);
#endif

	if (m_pEncoderParam->m_bVideoAVCFile)
	{
		// the payloads of all output NALs are guaranteed to be sequential in memory.
		if (m_pDumpFile)
			fwrite(pbNalu[0].p_payload, nEncodeSize, 1, m_pDumpFile);
	}
	if (m_pPostProc)
	{
		if (IS_X264_TYPE_I(x264PicOut.i_type))
			m_pEncoderParam->m_ullVideoIFrames ++;
		else if (IS_X264_TYPE_B(x264PicOut.i_type))
			m_pEncoderParam->m_ullVideoBFrames ++;
		else
			m_pEncoderParam->m_ullVideoPFrames ++;

#if 0
		LOG(LOG_TRACE, _T("video frame: %s, key: %d, size: %d"), 
			(x264PicOut.i_type == X264_TYPE_IDR)  ? _T("IDR") : 
			(x264PicOut.i_type == X264_TYPE_I)    ? _T("I") : 
			(x264PicOut.i_type == X264_TYPE_P)    ? _T("P") : 
			(x264PicOut.i_type == X264_TYPE_BREF) ? _T("BREF") : 
			(x264PicOut.i_type == X264_TYPE_B)    ? _T("B") : _T("NULL"),
			x264PicOut.b_keyframe, nEncodeSize);
#endif

		for (nIndex = 0; nIndex < cbNalu; nIndex ++)
		{
			if (pbNalu[nIndex].i_type != NAL_SEI && 
				pbNalu[nIndex].i_type != NAL_SPS && 
				pbNalu[nIndex].i_type != NAL_PPS)
			{
				if (x264PicOut.i_pts > x264PicOut.i_dts)
				{
					// CTS (Composition Time Stamp) defines when to compose(present) this picture.
					// DTS (Decode Time Stamp) defines when to decode this picture.
					// Composition Time Offset:  offset between decoding time and composition time.
					// Since decoding time must be less than the composition time, 
					// the offsets are expressed as unsigned numbers.
					nCTOff = (int)(x264PicOut.i_pts - x264PicOut.i_dts) * (1000 / m_pEncoderParam->m_nVideoFPS);
				}

				// NOTE: x264_nal_encode() will automatically add 4 or 3 bytes (start code or size depending on annexb) in the head of payload.
				bLongStartCode = !nIndex || pbNalu[nIndex].i_type == NAL_SPS || pbNalu[nIndex].i_type == NAL_PPS;
				nPayloadOffset = bLongStartCode ? 4 : 3;

				((ENCODER_ENC_POST_PROC)m_pPostProc)(m_pCaller, pbNalu[nIndex].p_payload + nPayloadOffset, 
					pbNalu[nIndex].i_payload - 4, nFrameTime, x264PicOut.b_keyframe, nCTOff);
			}
		}
	}

	return nEncodeSize;
}

DWORD CVideoEncoder::ThreadProc()
{
	BYTE * pFrameData  = NULL;
	int    nFrameSize  = 0; 
	int    nFrameTime  = 0;
	int    nEncodeSize = 0;
	bool   bRet        = false;

	while (m_bRunning)
	{
		bRet = ((ENCODER_RAW_PULL_PROC)m_pPullProc)(m_pCaller, &pFrameData, &nFrameSize, &nFrameTime);
		if (bRet)
		{
			nEncodeSize = EncodeFrame(pFrameData, nFrameSize, nFrameTime);
			if (nEncodeSize > 0)
			{
				m_pEncoderParam->m_ullVideoEncode ++;
				m_pEncoderParam->m_ullVideoBytes += nEncodeSize;
			}
			else
			{
				m_pEncoderParam->m_ullVideoDrop ++;
			}
		}
	}

	return 0;
}
