#ifndef _HK_CAMERA_H
#define _HK_CAMERA_H

#include<HCNetSDK.h>
#include<plaympeg4.h>
#include<PlayM4.h>    
#include <opencv2\core\core.hpp>
#include <opencv2\highgui\highgui.hpp>
#include <opencv2\imgproc\imgproc.hpp>


class HK_camera
{
public:
	HK_camera(void);
	~HK_camera(void);

public:
	bool Recording = false;//�����ж��Ƿ�����¼��

	//��ʼ��
	bool Init();
	//��½
	bool Login(char* sDeviceAddress, char* sUserName, char* sPassword, WORD wPort);
	//ע��
	void Logout();
	//��ʾͼ��
	void Show();
	//���ù�ѧ�䱶ֵ
	void setFocusMode(float fOpticalZoomLevel);
	//����������ѹ������(֡�ʡ��ֱ���)
	void setCompressionParms(int FrameRate, int Resolution);
	//��̨����  11: ������12�������С��stop����0����ʼ��1��ֹͣ
	void PTZControl(DWORD dwPTZCommand, DWORD stop);
	//¼����  stop����0����ʼ��1��ֹͣ
	void Recorder(char* sFileName, bool stop);

private:
	LONG lUserID;
	LONG lRealPlayHandle;
	
	NET_DVR_USER_LOGIN_INFO pLoginInfo = { 0 };
	NET_DVR_DEVICEINFO_V40 lpDeviceInfo = { 0 };  //�豸��Ϣ, �������
	
};
#endif
