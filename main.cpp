#include <io.h>
#include <direct.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <atltime.h>
#include <chrono>

#include "HK_camera.h"
#include "MTCNN_opencv.h"
#include <opencv2/face.hpp>

using namespace std;
using namespace cv;

void detectAndDisplay(cv::Mat frame, HK_camera camera);

//全局变量
LONG lPort; //全局的播放库port号
cv::Mat g_BGRImage;

 //PID调节器参数
float uT = 0.1f, delta_T = 0;
float Ek = 0, Ek_1 = 0, Ek_2 = 0;
float kP = 0.0015f, kI = 0.0005f, kD = 0.0005f;
 //
bool Focal_Plus = false;
bool Focal_Minus = false;

auto Time_st = chrono::system_clock::now();

//数据解码回调函数，
//功能：将YV_12格式的视频数据流转码为可供opencv处理的BGR类型的图片数据
void CALLBACK DecCBFun(long nPort, char* pBuf, long nSize, FRAME_INFO* pFrameInfo, long nUser, long nReserved2) {

	if (pFrameInfo->nType == T_YV12) {
		if (g_BGRImage.empty()) {
			g_BGRImage.create(pFrameInfo->nHeight, pFrameInfo->nWidth, CV_8UC3);
		}
		Mat YUVImage(pFrameInfo->nHeight + pFrameInfo->nHeight / 2, pFrameInfo->nWidth, CV_8UC1, (unsigned char*)pBuf);
		cvtColor(YUVImage, g_BGRImage, COLOR_YUV2BGR_YV12);

		YUVImage.~Mat();  //Mat的析构函数，析构函数调用 Mat::release()
	}
}

//实时视频码流数据获取 回调函数
void CALLBACK g_RealDataCallBack_V30(LONG lPlayHandle, DWORD dwDataType, BYTE *pBuffer, DWORD dwBufSize, void* pUser) {
	switch (dwDataType) {
	case NET_DVR_STREAMDATA://码流数据
		if (dwBufSize > 0 && lPort != -1){
			if (!PlayM4_InputData(lPort, pBuffer, dwBufSize)) break;
		}
		break;
	default: //其他数据
		break;
	}
}

// 异常消息回调函数
void CALLBACK g_ExceptionCallBack(DWORD dwType, LONG lUserID, LONG lHandle, void *pUser)
{
	char tempbuf[256] = { 0 };
	switch (dwType)
	{
	case EXCEPTION_RECONNECT:    //预览时重连
		printf("----------reconnect--------%Id\n", time(NULL));
		break;
	default:
		break;
	}
}

//构造函数
HK_camera::HK_camera(void)
{
}
//析构函数
HK_camera::~HK_camera(void)
{
}

//初始化函数，用作初始化状态检测
bool HK_camera::Init() {
	if (NET_DVR_Init()) {
		cout << "Initial sucessfully" << endl;
		return true;
	}
	else {
		printf("Initial failed, error code: %d\n", NET_DVR_GetLastError());
		return false;
	}
}

//登录函数，用作摄像头id以及密码输入登录
bool HK_camera::Login(char* sDeviceAddress, char* sUserName, char* sPassword, WORD wPort) {
	pLoginInfo.bUseAsynLogin = 0;     //同步登录方式
	strcpy_s(pLoginInfo.sDeviceAddress, sDeviceAddress);
	strcpy_s(pLoginInfo.sUserName, sUserName);
	strcpy_s(pLoginInfo.sPassword, sPassword);
	pLoginInfo.wPort = wPort;

	lUserID = NET_DVR_Login_V40(&pLoginInfo, &lpDeviceInfo);  //对于同步登录，接口返回-1表示登录失败，其他值表示返回的用户ID值。

	if (lUserID < 0) {
		printf("Login failed, error code: %d\n", NET_DVR_GetLastError());
		NET_DVR_Cleanup();
		return false;
	}
	cout << "Login successfully" << endl;
	return true;
}

//视频流显示函数
void HK_camera::Show() {
	if (PlayM4_GetPort(&lPort)) {     //获取播放库通道号
		if (PlayM4_SetStreamOpenMode(lPort, STREAME_REALTIME)) {     //设置流模式
			if (PlayM4_OpenStream(lPort, NULL, 0, 1024 * 1024)) {      //打开流
				if (PlayM4_SetDecCallBackExMend(lPort, DecCBFun, NULL, 0, NULL)) {
					if (PlayM4_Play(lPort, NULL)) {
						std::cout << "success to set play mode" << std::endl;
					}
					else {
						std::cout << "fail to set play mode" << std::endl;
					}
				}
				else {
					std::cout << "fail to set dec callback " << std::endl;
				}
			}
			else {
				std::cout << "fail to open stream" << std::endl;
			}
		}
		else {
			std::cout << "fail to set stream open mode" << std::endl;
		}
	}
	else {
		std::cout << "fail to get port" << std::endl;
	}
	//启动预览并设置回调数据流
	NET_DVR_PREVIEWINFO struPlayInfo = { 0 };
	struPlayInfo.hPlayWnd = NULL; //窗口为空，设备SDK不解码只取流
	struPlayInfo.lChannel = 1; //Channel number 设备通道
	struPlayInfo.dwStreamType = 0; // 码流类型，0-主码流，1-子码流，2-码流3，3-码流4, 4-码流5,5-码流6,7-码流7,8-码流8,9-码流9,10-码流10
	struPlayInfo.dwLinkMode = 0;// 0：TCP方式,1：UDP方式,2：多播方式,3 - RTP方式，4-RTP/RTSP, 5-RSTP/HTTP
	struPlayInfo.bBlocked = 0; //0-非阻塞取流, 1-阻塞取流, 如果阻塞SDK内部connect失败将会有5s的超时才能够返回,不适合于轮询取流操作.
	lRealPlayHandle = NET_DVR_RealPlay_V40(lUserID, &struPlayInfo, g_RealDataCallBack_V30, NULL);
	if (lRealPlayHandle < 0) {
		printf("NET_DVR_RealPlay_V40 error, %d\n", NET_DVR_GetLastError());
		NET_DVR_Logout(lUserID);
		NET_DVR_Cleanup();
	}
	waitKey(200);
}

void HK_camera::Logout() {
	//关闭预览
	if (!NET_DVR_StopRealPlay(lRealPlayHandle))
	{
		printf("NET_DVR_StopRealPlay error! Error number: %d\n", NET_DVR_GetLastError());
	}
	//释放播放库资源
	PlayM4_Stop(lPort);
	PlayM4_CloseStream(lPort);
	PlayM4_FreePort(lPort);
	//注销用户
	NET_DVR_Logout(lUserID);
	NET_DVR_Cleanup();
}

//设置光学变倍值
void HK_camera::setFocusMode(float fOpticalZoomLevel = 10) {
	int iRet = 0;
	DWORD dwReturnLen;
	NET_DVR_FOCUSMODE_CFG struParams = { 0 };
	//获取光学变倍值
	iRet = NET_DVR_GetDVRConfig(lUserID, NET_DVR_GET_FOCUSMODECFG, 1, &struParams, \
		sizeof(NET_DVR_FOCUSMODE_CFG), &dwReturnLen);
	if (!iRet) {
		printf("NET_DVR_GetDVRConfig NET_DVR_GET_FOCUSMODECFG error.\n");
		NET_DVR_Logout(lUserID);
		NET_DVR_Cleanup();
		return;
	}

	//修改光学变倍值
	struParams.byFocusMode = 0;
	struParams.fOpticalZoomLevel = fOpticalZoomLevel;//设置没有生效

	iRet = NET_DVR_SetDVRConfig(lUserID, NET_DVR_SET_FOCUSMODECFG, 1, &struParams, \
		sizeof(NET_DVR_FOCUSMODE_CFG));
	if (!iRet) {
		printf("NET_DVR_SetDVRConfig NET_DVR_SET_FOCUSMODECFG error.\n");
		NET_DVR_Logout(lUserID);
		NET_DVR_Cleanup();
		return;
	}

	iRet = NET_DVR_GetDVRConfig(lUserID, NET_DVR_GET_FOCUSMODECFG, 1, &struParams, \
		sizeof(NET_DVR_FOCUSMODE_CFG), &dwReturnLen);
	if (!iRet) {
		printf("NET_DVR_GetDVRConfig NET_DVR_GET_FOCUSMODECFG error.\n");
		NET_DVR_Logout(lUserID);
		NET_DVR_Cleanup();
		return;
	}
	printf("byFocusMode: %d\n", struParams.byFocusMode);
	printf("byAutoFocusMode: %d\n", struParams.byAutoFocusMode);
	printf("wMinFocusDistance: %d\n", struParams.wMinFocusDistance);
	printf("byZoomSpeedLevel: %d\n", struParams.byZoomSpeedLevel);
	printf("byFocusSpeedLevel: %d\n", struParams.byFocusSpeedLevel);
	//printf("fOpticalZoomLevel: %f\n", struParams.fOpticalZoomLevel);
	cout << "光学变倍值a：" << struParams.fOpticalZoomLevel << endl;
}

//设置主码流压缩参数(帧率、分辨率)
void HK_camera::setCompressionParms(int FrameRate = 17, int Resolution = 19) {  //F——18：30fps，17：25fps；R——19：720p，39：1080p
	int iRet = 0;
	DWORD dwReturnLen;
	NET_DVR_COMPRESSIONCFG_V30 struParams = { 0 }; //压缩参数
												   //获取压缩参数
	iRet = NET_DVR_GetDVRConfig(lUserID, NET_DVR_GET_COMPRESSCFG_V30, 1, \
		&struParams, sizeof(NET_DVR_COMPRESSIONCFG_V30), &dwReturnLen);
	if (!iRet) {
		printf("NET_DVR_GetDVRConfig NET_DVR_GET_COMPRESSCFG_V30 error.\n");
		NET_DVR_Logout(lUserID);
		NET_DVR_Cleanup();
		return;
	}
	//设置帧率, 分辨率
	struParams.struNormHighRecordPara.dwVideoFrameRate = FrameRate;
	struParams.struNormHighRecordPara.byResolution = Resolution;

	iRet = NET_DVR_SetDVRConfig(lUserID, NET_DVR_SET_COMPRESSCFG_V30, 1, \
		&struParams, sizeof(NET_DVR_COMPRESSIONCFG_V30));
	if (!iRet) {
		printf("NET_DVR_GetDVRConfig NET_DVR_SET_COMPRESSCFG_V30 error.\n");
		NET_DVR_Logout(lUserID);
		NET_DVR_Cleanup();
		return;
	}
	//获取压缩参数
	iRet = NET_DVR_GetDVRConfig(lUserID, NET_DVR_GET_COMPRESSCFG_V30, 1, \
		&struParams, sizeof(NET_DVR_COMPRESSIONCFG_V30), &dwReturnLen);
	if (!iRet)
	{
		printf("NET_DVR_GetDVRConfig NET_DVR_GET_COMPRESSCFG_V30 error.\n");
		NET_DVR_Logout(lUserID);
		NET_DVR_Cleanup();
		return;
	}
	printf("Video FrameRate is %d\n", struParams.struNormHighRecordPara.dwVideoFrameRate);
	printf("Video Resolution is %d\n", struParams.struNormHighRecordPara.byResolution);
}

//云台控制  11: 焦距变大，12：焦距变小；stop——0：开始，1：停止
void HK_camera::PTZControl(DWORD dwPTZCommand, DWORD stop) {
	int iRet = 0;
	iRet = NET_DVR_PTZControl(lRealPlayHandle, dwPTZCommand, stop);//0: 开始
	if (!iRet) {
		cout << "PTZControl ERROR! Error number: " << NET_DVR_GetLastError() << endl;
		return;
	}
}

//录像功能  stop——0：开始，1：停止
void HK_camera::Recorder(char* sFileName, bool stop)
{
	if (stop == 0){
		//检查目录是否存在，不存在则创建
		if (_access(sFileName, 0) != 0) {//目录不存在  在标准C++中，该函数为_access，位于头文件<io.h>中，
										 //而在Linux下，该函数为access，位于头文件<unistd.h>中
			if (_mkdir(sFileName))//创建目录   windows下为_mkdir, #include <direct.h>. 
								  //linux下为mkdir， #include <sys/stat.h>
				cout << "创建目录成功" << endl;
			else
				cout << "创建目录失败" << endl;
		}

		int iRet = 0;
		char RecName[256] = { 0 };

		CTime CurTime = CTime::GetCurrentTime();//CTime类，在atltime.h头文件中
		sprintf_s(RecName, "%s%04d%02d%02d%02d%02d%02d.mp4", sFileName, CurTime.GetYear(), CurTime.GetMonth(), CurTime.GetDay(), \
			CurTime.GetHour(), CurTime.GetMinute(), CurTime.GetSecond());
		iRet = NET_DVR_SaveRealData(lRealPlayHandle, RecName);
		if (!iRet)
			cout << "启动录像失败! Error number: " << NET_DVR_GetLastError() << endl;
		Recording = true;
	}
	else if (stop == 1){
		NET_DVR_StopSaveRealData(lRealPlayHandle);
		Recording = false;
	}
}

int main()
{
	std::string main_window_name = "Capture - Face detection";
	cv::namedWindow(main_window_name, CV_WINDOW_NORMAL);//CV_WINDOW_AUTOSIZE 
	cv::moveWindow(main_window_name, 0, 0);

	HK_camera camera;

	if (!camera.Init()) return -1;

	if (!camera.Login("10.193.239.2", "admin", "qwertyui,123", 8000)) return -1;

	camera.Show();

	while (true)
	{
		if (!g_BGRImage.empty())
		{
			detectAndDisplay(g_BGRImage, camera);

			//变焦控制
			auto duration = chrono::duration_cast<chrono::microseconds>(chrono::system_clock::now() - Time_st);
			if ((double(duration.count()) * chrono::microseconds::period::num / chrono::microseconds::period::den) > uT)
			{
				if (Focal_Plus == true)
				{
					camera.PTZControl(ZOOM_IN, 1);
					Focal_Plus = false;
				}
				else if (Focal_Minus == true)
				{
					camera.PTZControl(ZOOM_OUT, 1);
					Focal_Minus = false;
				}

				Time_st = chrono::system_clock::now();
			}

			cv::imshow(main_window_name, g_BGRImage);

			int c = cv::waitKey(10);
			if ((char)c == 27) break; //ESC键退出
		}
	}

	camera.Logout();
	cv::destroyAllWindows();
	//system("pause");
	return 0;
}

//检测和显示
void detectAndDisplay(cv::Mat frame, HK_camera camera) {
	MTCNN mtcnn_detector("../3rd_x64/MTCNN_model");

	std::vector<cv::Mat> rgbChannels(3);
	cv::split(frame, rgbChannels);
	cv::Mat frame_gray = rgbChannels[2];// 转换为单颜色通道的灰度图像.加快检测速度
	float factor = 0.709f;
	float threshold[3] = { 0.7f, 0.6f, 0.6f };
	int minSize = 100;

	vector<FaceInfo> faceInfo = mtcnn_detector.Detect_mtcnn(frame, minSize, threshold, factor, 3);
	for (int i = 0; i < faceInfo.size(); ++i) {
		int x = (int)faceInfo[i].bbox.xmin;
		int y = (int)faceInfo[i].bbox.ymin;
		int w = (int)(faceInfo[i].bbox.xmax - faceInfo[i].bbox.xmin + 1);
		int h = (int)(faceInfo[i].bbox.ymax - faceInfo[i].bbox.ymin + 1);

		//实际的变焦控制算法
		if ((w < 160) && (Focal_Plus == false)) {
			Ek_2 = Ek_1;
			Ek_1 = Ek;
			Ek = 160 - w;
			delta_T = kP*(Ek - Ek_1) + kI*Ek + kD*(Ek - 2 * Ek_1 + Ek_2);
			uT += delta_T;
			if (uT > 0.2809)
				uT = 0.2809f;
			camera.PTZControl(ZOOM_IN, 0);
			Focal_Plus = true;
			Time_st = chrono::system_clock::now();
		}else if ((w > 220) && (Focal_Minus == false)) {
			Ek_2 = Ek_1;
			Ek_1 = Ek;
			Ek = w - 220;
			delta_T = kP*(Ek - Ek_1) + kI*Ek + kD*(Ek - 2 * Ek_1 + Ek_2);
			uT += delta_T;
			if (uT > 0.2809) 
				uT = 0.2809f;
			camera.PTZControl(ZOOM_OUT, 0);
			Focal_Minus = true;
			Time_st = chrono::system_clock::now();
		}
	}
}