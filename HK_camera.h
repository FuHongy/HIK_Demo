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
	bool Recording = false;//用于判断是否正在录像

	//初始化
	bool Init();
	//登陆
	bool Login(char* sDeviceAddress, char* sUserName, char* sPassword, WORD wPort);
	//注销
	void Logout();
	//显示图像
	void Show();
	//设置光学变倍值
	void setFocusMode(float fOpticalZoomLevel);
	//设置主码流压缩参数(帧率、分辨率)
	void setCompressionParms(int FrameRate, int Resolution);
	//云台控制  11: 焦距变大，12：焦距变小；stop——0：开始，1：停止
	void PTZControl(DWORD dwPTZCommand, DWORD stop);
	//录像功能  stop——0：开始，1：停止
	void Recorder(char* sFileName, bool stop);

private:
	LONG lUserID;
	LONG lRealPlayHandle;
	
	NET_DVR_USER_LOGIN_INFO pLoginInfo = { 0 };
	NET_DVR_DEVICEINFO_V40 lpDeviceInfo = { 0 };  //设备信息, 输出参数
	
};
#endif
