#ifndef _DOWNSHA_ENCODER_H_
#define _DOWNSHA_ENCODER_H_

#include "DshowCapture.h"
#include "AudioGrabber.h"
#include "AudioEncoder.h"
#include "VideoGrabber.h"
#include "VideoEncoder.h"
#include "ColorConvertor.h"
#include "DownshaOutput.h"

typedef bool (*ENCODER_RAW_PUSH_PROC)(void * pCaller, BYTE * pData, int nSize, int nStamp);
typedef bool (*ENCODER_RAW_PULL_PROC)(void * pCaller, BYTE ** ppData, int * pSize, int * pStamp);
typedef bool (*ENCODER_ENC_POST_PROC)(void * pCaller, BYTE * pData, int nSize, int nStamp, int nKeyFrame, int nCTOff);

#define ENCODER_CSP_NONE           0x0000 // Invalid color space
// YUV
#define ENCODER_CSP_CLPL           0x0001
#define ENCODER_CSP_YUYV           0x0002 // a.k.a YUY2 (packed 4:2:2)
#define ENCODER_CSP_IYUV           0x0003 // a.k.a I420 (planar 4:2:0)
#define ENCODER_CSP_YVU9           0x0004 // (planar 9bpp)
#define ENCODER_CSP_Y411           0x0005 // a.k.a Y41P (packed 4:1:1)
#define ENCODER_CSP_YVYU           0x0006 // (packed 4:2:2)
#define ENCODER_CSP_UYVY           0x0007 // (packed 4:2:2)
#define ENCODER_CSP_Y211           0x0008 // (packed 2:1:1)
#define ENCODER_CSP_CLJR           0x0009 // (Cirrus Logic Jr YUV 411 format with less than 8 bits per Y, U, and V sample)
#define ENCODER_CSP_IF09           0x000a // (Indeo produced YVU9 format)
#define ENCODER_CSP_CPLA           0x000b 
// RGB formats with no alpha channel
#define ENCODER_CSP_RGB1           0x0011 // (1 bpp, Palettized)
#define ENCODER_CSP_RGB4           0x0012 // (4 bpp, Palettized)
#define ENCODER_CSP_RGB8           0x0013 // (8 bpp, Palettized)
#define ENCODER_CSP_RGB555         0x0014 // (16 bpp)
#define ENCODER_CSP_RGB565         0x0015 // (16 bpp)
#define ENCODER_CSP_RGB24          0x0016 // (24 bpp)
#define ENCODER_CSP_RGB32          0x0017 // (32 bpp)
// RGB formats with alpha channel
#define ENCODER_CSP_ARGB1555       0x0021 // RGB 555 with alpha channel
#define ENCODER_CSP_ARGB32         0x0022 // RGB 32 with alpha channel
#define ENCODER_CSP_ARGB4444       0x0023 // 16-bit RGB with alpha channel; 4 bits per channel
#define ENCODER_CSP_A2R10G10B10    0x0024 // 32-bit RGB with alpha channel; 10 bits per RGB channel plus 2 bits for alpha.
#define ENCODER_CSP_A2B10G10R10    0x0025 // 32-bit RGB with alpha channel; 10 bits per RGB channel plus 2 bits for alpha.

struct ENCODER_PARAM
{
	bool  m_bVideoEnable;           // Video enable
	char  m_szVideoDeviceName[256]; // Video device name
	bool  m_bVideoPreview;          // Video preview mode
	int   m_nVideoWidth;            // Video width
	int   m_nVideoHeight;           // Video height
	bool  m_bVideoFPSStrict;        // Video fps strict mode
	int   m_nVideoFPS;              // Video fps
	bool  m_bVideoDropRaw;          // Whether drop raw frames
	bool  m_bVideoDropFPS;          // Whether drop fps
	int   m_nVideoDropNum;          // Video drop numerator
	int   m_nVideoDropDen;          // Video drop denominator
	int   m_nVideoCSP;              // Video color space
	int   m_nVideoRawFrameSize;     // Video raw frame size
	int   m_nVideoRawQueueSize;     // Video raw queue size

	bool  m_bVideoRawFile;          // Whether dump YUV file
	bool  m_bVideoAVCFile;          // Whether dump AVC file
	char  m_szVideoAVCPreset[64];   // video encoder preset, ultrafast/superfast/veryfast/faster/fast/medium/slow/slower/veryslow/placebo
	char  m_szVideoAVCTune[64];     // video encoder tune, film/animation/grain/stillimage/psnr/ssim/fastdecode/zerolatency
	char  m_szVideoAVCProfile[64];  // Video encoder profile, baseline/main/high/high422/high444
	int   m_nVideoAVCKeyInt;        // Video encoder maximum key interval
	int   m_nVideoAVCBitRate;       // Video encoder bitrate
	int   m_nVideoAVCBitRateFactor; // Video encoder bitrate factor
	int   m_nVideoAVCBFrames;       // Video encoder bframes count

	int   m_nVideoAVCSPSLen;        // Video AVC SPS length
	BYTE  m_pVideoAVCSPS[64];       // Video AVC SPS
	int   m_nVideoAVCPPSLen;        // Video AVC PPS length
	BYTE  m_pVideoAVCPPS[64];       // Video AVC PPS
	char  m_szVideoAVCParam[1024];  // Video AVC Parameters

	bool  m_bAudioEnable;           // Audio enable
	char  m_szAudioDeviceName[256]; // Audio device name (optional)
	int   m_nAudioChannels;         // Audio channels
	int   m_nAudioSamplesPerSec;    // Audio sampling rate
	int   m_nAudioBytesPerSample;   // Audio bytes per sample
	int   m_nAudioBufferSamples;    // Audio buffer samples
	int   m_nAudioBufferCount;      // Audio buffer count
	int   m_nAudioRawFrameSize;     // Audio raw frame size
	int   m_nAudioRawQueueSize;     // Audio raw queue size

	bool  m_bAudioRawFile;          // Whether dump PCM file
	bool  m_bAudioAACFile;          // Whether dump AAC file
	int   m_nAudioAACQuality;       // AAC quantizer quality
	int   m_nAudioAACBandwidth;     // AAC file frequency bandwidth in Hz
	int   m_nAudioAACObjectType;    // AAC Object Type

	bool  m_bOutputEnable;          // Output enable
	int   m_nOutputQueueSize;       // Output queue size
	char  m_szOutputHost[64];       // Output server host
	unsigned short m_usOutputPort;  // Output server port

	bool  m_bDebugEnable;           // Debug enable

	unsigned __int64 m_ullStartTime;     // Start time in milliseconds
	unsigned __int64 m_ullElapseTime;    // Elapse time in milliseconds
	unsigned __int64 m_ullPrevTime;      // Previous time in milliseconds

	unsigned __int64 m_ullTotalBytes;    // total bytes
	unsigned __int64 m_ullVideoBytes;    // video bytes
	unsigned __int64 m_ullAudioBytes;    // audio bytes
	double           m_dblTotalRate;     // total bitrate
	double           m_dblVideoRate;     // video bitrate
	double           m_dblAudioRate;     // audio bitrate

	unsigned __int64 m_ullVideoEncode;  // encode video frames
	unsigned __int64 m_ullAudioEncode;  // encode audio frames
	unsigned __int64 m_ullVideoDrop;    // discard video frames
	unsigned __int64 m_ullAudioDrop;    // discard audio frames
	double           m_dblVideoFPS;     // encoding video fps
	double           m_dblAudioFPS;     // encoding audio fps

	unsigned __int64 m_ullVideoIFrames; // I-frames
	unsigned __int64 m_ullVideoPFrames; // P-frames
	unsigned __int64 m_ullVideoBFrames; // B-frames
};

class CDownshaEncoder
{
public:
	CDownshaEncoder();
	virtual ~CDownshaEncoder();

	bool Start     (LPCSTR szParamFile);
	void Stop      ();
	bool IsRunning ();
	void PrintStat ();

	static bool AudioRawPushProc(void * pCaller, BYTE * pData, int nSize, int nStamp);
	static bool VideoRawPushProc(void * pCaller, BYTE * pData, int nSize, int nStamp);
	static bool AudioRawPullProc(void * pCaller, BYTE ** ppData, int * pSize, int * pStamp);
	static bool VideoRawPullProc(void * pCaller, BYTE ** ppData, int * pSize, int * pStamp);
	static bool AudioEncPostProc(void * pCaller, BYTE * pData, int nSize, int nStamp, int nKeyFrame, int nCTOff);
	static bool VideoEncPostProc(void * pCaller, BYTE * pData, int nSize, int nStamp, int nKeyFrame, int nCTOff);

private:
	bool InitParam  (LPCSTR szParamFile);
	void CleanParam ();
	int  GetCSP     (const char * str);

private:
	bool              m_bRunning;        // encoder running state
	void            * m_pConfMgmt;       // configure management
	ENCODER_PARAM   * m_pEncoderParam;   // encoder parameters
	CDshowCapture   * m_pDshowCapture;   // direct show capture
	CDownshaOutput  * m_pDownshaOutput;  // media output module

	CAudioGrabber   * m_pAudioGrabber;   // audio grabber callback
	CAudioEncoder   * m_pAudioEncoder;   // audio encoder module
	void            * m_pAudioQueue;     // audio queue
	
	CVideoGrabber   * m_pVideoGrabber;   // video grabber callback
	CVideoEncoder   * m_pVideoEncoder;   // video encoder module
	CColorConvertor * m_pColorConvertor; // video color convertor
	void            * m_pVideoQueue;     // video queue
};

#endif // _DOWNSHA_ENCODER_H_
