#ifndef _DSHOW_CAPTURE_H_
#define _DSHOW_CAPTURE_H_

struct ENCODER_PARAM;
class CDshowCapture
{
public:
	CDshowCapture();
	virtual ~CDshowCapture();

	bool Start     (ENCODER_PARAM * pEncoderParam, ISampleGrabberCB * pVideoGrabberCB, ISampleGrabberCB * pAudioGrabberCB);
	void Stop      ();
	bool IsRunning ();

private:
	bool    BindDeviceFilter   (const IID & clsidDeviceClass, LPCTSTR pszDeviceName, IBaseFilter **ppDeviceFilter);
	bool    BuildVideoDevice   ();
	bool    BuildVideoGrabber  ();
	bool    BuildAudioDevice   ();
	bool    BuildAudioGrabber  ();

	LPCTSTR GetMediaGUID       (GUID & guid, LPTSTR str);
	LPCTSTR GetMediaMajorType  (AM_MEDIA_TYPE * pMediaType, LPTSTR szGUID);
	LPCTSTR GetMediaSubType    (AM_MEDIA_TYPE * pMediaType, LPTSTR szGUID);
	LPCTSTR GetMediaFormatType (AM_MEDIA_TYPE * pMediaType, LPTSTR szGUID);
	void    PrintMediaType     (AM_MEDIA_TYPE * pMediaType);
	void    DeleteMediaType    (AM_MEDIA_TYPE * pMediaType);

	int     GUID2CSP           (GUID & guid);
	GUID    CSP2GUID           (int csp);

private:
	ENCODER_PARAM         * m_pEncoderParam;
	bool                    m_bRunning;
	IGraphBuilder         * m_pGraphBuilder;
	ICaptureGraphBuilder2 * m_pCaptureGraphBuilder;
	IMediaControl         * m_pMediaControl;

	IBaseFilter           * m_pVideoDeviceFilter;
	IBaseFilter           * m_pVideoGrabberFilter;
	ISampleGrabber        * m_pVideoGrabber;
	ISampleGrabberCB      * m_pVideoGrabberCB;
	AM_MEDIA_TYPE         * m_pVideoMediaType;

	IBaseFilter           * m_pAudioDeviceFilter;
	IBaseFilter           * m_pAudioGrabberFilter;
	ISampleGrabber        * m_pAudioGrabber;
	ISampleGrabberCB      * m_pAudioGrabberCB;
	AM_MEDIA_TYPE         * m_pAudioMediaType;
};

#endif // _DSHOW_CAPTURE_H_
