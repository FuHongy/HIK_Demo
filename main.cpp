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

//ȫ�ֱ���
LONG lPort; //ȫ�ֵĲ��ſ�port��
cv::Mat g_BGRImage;

 //PID����������
float uT = 0.1f, delta_T = 0;
float Ek = 0, Ek_1 = 0, Ek_2 = 0;
float kP = 0.0015f, kI = 0.0005f, kD = 0.0005f;
 //
bool Focal_Plus = false;
bool Focal_Minus = false;

auto Time_st = chrono::system_clock::now();

//���ݽ���ص�������
//���ܣ���YV_12��ʽ����Ƶ������ת��Ϊ�ɹ�opencv�����BGR���͵�ͼƬ����
void CALLBACK DecCBFun(long nPort, char* pBuf, long nSize, FRAME_INFO* pFrameInfo, long nUser, long nReserved2) {

	if (pFrameInfo->nType == T_YV12) {
		if (g_BGRImage.empty()) {
			g_BGRImage.create(pFrameInfo->nHeight, pFrameInfo->nWidth, CV_8UC3);
		}
		Mat YUVImage(pFrameInfo->nHeight + pFrameInfo->nHeight / 2, pFrameInfo->nWidth, CV_8UC1, (unsigned char*)pBuf);
		cvtColor(YUVImage, g_BGRImage, COLOR_YUV2BGR_YV12);

		YUVImage.~Mat();  //Mat������������������������ Mat::release()
	}
}

//ʵʱ��Ƶ�������ݻ�ȡ �ص�����
void CALLBACK g_RealDataCallBack_V30(LONG lPlayHandle, DWORD dwDataType, BYTE *pBuffer, DWORD dwBufSize, void* pUser) {
	switch (dwDataType) {
	case NET_DVR_STREAMDATA://��������
		if (dwBufSize > 0 && lPort != -1){
			if (!PlayM4_InputData(lPort, pBuffer, dwBufSize)) break;
		}
		break;
	default: //��������
		break;
	}
}

// �쳣��Ϣ�ص�����
void CALLBACK g_ExceptionCallBack(DWORD dwType, LONG lUserID, LONG lHandle, void *pUser)
{
	char tempbuf[256] = { 0 };
	switch (dwType)
	{
	case EXCEPTION_RECONNECT:    //Ԥ��ʱ����
		printf("----------reconnect--------%Id\n", time(NULL));
		break;
	default:
		break;
	}
}

//���캯��
HK_camera::HK_camera(void)
{
}
//��������
HK_camera::~HK_camera(void)
{
}

//��ʼ��������������ʼ��״̬���
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

//��¼��������������ͷid�Լ����������¼
bool HK_camera::Login(char* sDeviceAddress, char* sUserName, char* sPassword, WORD wPort) {
	pLoginInfo.bUseAsynLogin = 0;     //ͬ����¼��ʽ
	strcpy_s(pLoginInfo.sDeviceAddress, sDeviceAddress);
	strcpy_s(pLoginInfo.sUserName, sUserName);
	strcpy_s(pLoginInfo.sPassword, sPassword);
	pLoginInfo.wPort = wPort;

	lUserID = NET_DVR_Login_V40(&pLoginInfo, &lpDeviceInfo);  //����ͬ����¼���ӿڷ���-1��ʾ��¼ʧ�ܣ�����ֵ��ʾ���ص��û�IDֵ��

	if (lUserID < 0) {
		printf("Login failed, error code: %d\n", NET_DVR_GetLastError());
		NET_DVR_Cleanup();
		return false;
	}
	cout << "Login successfully" << endl;
	return true;
}

//��Ƶ����ʾ����
void HK_camera::Show() {
	if (PlayM4_GetPort(&lPort)) {     //��ȡ���ſ�ͨ����
		if (PlayM4_SetStreamOpenMode(lPort, STREAME_REALTIME)) {     //������ģʽ
			if (PlayM4_OpenStream(lPort, NULL, 0, 1024 * 1024)) {      //����
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
	//����Ԥ�������ûص�������
	NET_DVR_PREVIEWINFO struPlayInfo = { 0 };
	struPlayInfo.hPlayWnd = NULL; //����Ϊ�գ��豸SDK������ֻȡ��
	struPlayInfo.lChannel = 1; //Channel number �豸ͨ��
	struPlayInfo.dwStreamType = 0; // �������ͣ�0-��������1-��������2-����3��3-����4, 4-����5,5-����6,7-����7,8-����8,9-����9,10-����10
	struPlayInfo.dwLinkMode = 0;// 0��TCP��ʽ,1��UDP��ʽ,2���ಥ��ʽ,3 - RTP��ʽ��4-RTP/RTSP, 5-RSTP/HTTP
	struPlayInfo.bBlocked = 0; //0-������ȡ��, 1-����ȡ��, �������SDK�ڲ�connectʧ�ܽ�����5s�ĳ�ʱ���ܹ�����,���ʺ�����ѯȡ������.
	lRealPlayHandle = NET_DVR_RealPlay_V40(lUserID, &struPlayInfo, g_RealDataCallBack_V30, NULL);
	if (lRealPlayHandle < 0) {
		printf("NET_DVR_RealPlay_V40 error, %d\n", NET_DVR_GetLastError());
		NET_DVR_Logout(lUserID);
		NET_DVR_Cleanup();
	}
	waitKey(200);
}

void HK_camera::Logout() {
	//�ر�Ԥ��
	if (!NET_DVR_StopRealPlay(lRealPlayHandle))
	{
		printf("NET_DVR_StopRealPlay error! Error number: %d\n", NET_DVR_GetLastError());
	}
	//�ͷŲ��ſ���Դ
	PlayM4_Stop(lPort);
	PlayM4_CloseStream(lPort);
	PlayM4_FreePort(lPort);
	//ע���û�
	NET_DVR_Logout(lUserID);
	NET_DVR_Cleanup();
}

//���ù�ѧ�䱶ֵ
void HK_camera::setFocusMode(float fOpticalZoomLevel = 10) {
	int iRet = 0;
	DWORD dwReturnLen;
	NET_DVR_FOCUSMODE_CFG struParams = { 0 };
	//��ȡ��ѧ�䱶ֵ
	iRet = NET_DVR_GetDVRConfig(lUserID, NET_DVR_GET_FOCUSMODECFG, 1, &struParams, \
		sizeof(NET_DVR_FOCUSMODE_CFG), &dwReturnLen);
	if (!iRet) {
		printf("NET_DVR_GetDVRConfig NET_DVR_GET_FOCUSMODECFG error.\n");
		NET_DVR_Logout(lUserID);
		NET_DVR_Cleanup();
		return;
	}

	//�޸Ĺ�ѧ�䱶ֵ
	struParams.byFocusMode = 0;
	struParams.fOpticalZoomLevel = fOpticalZoomLevel;//����û����Ч

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
	cout << "��ѧ�䱶ֵa��" << struParams.fOpticalZoomLevel << endl;
}

//����������ѹ������(֡�ʡ��ֱ���)
void HK_camera::setCompressionParms(int FrameRate = 17, int Resolution = 19) {  //F����18��30fps��17��25fps��R����19��720p��39��1080p
	int iRet = 0;
	DWORD dwReturnLen;
	NET_DVR_COMPRESSIONCFG_V30 struParams = { 0 }; //ѹ������
												   //��ȡѹ������
	iRet = NET_DVR_GetDVRConfig(lUserID, NET_DVR_GET_COMPRESSCFG_V30, 1, \
		&struParams, sizeof(NET_DVR_COMPRESSIONCFG_V30), &dwReturnLen);
	if (!iRet) {
		printf("NET_DVR_GetDVRConfig NET_DVR_GET_COMPRESSCFG_V30 error.\n");
		NET_DVR_Logout(lUserID);
		NET_DVR_Cleanup();
		return;
	}
	//����֡��, �ֱ���
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
	//��ȡѹ������
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

//��̨����  11: ������12�������С��stop����0����ʼ��1��ֹͣ
void HK_camera::PTZControl(DWORD dwPTZCommand, DWORD stop) {
	int iRet = 0;
	iRet = NET_DVR_PTZControl(lRealPlayHandle, dwPTZCommand, stop);//0: ��ʼ
	if (!iRet) {
		cout << "PTZControl ERROR! Error number: " << NET_DVR_GetLastError() << endl;
		return;
	}
}

//¼����  stop����0����ʼ��1��ֹͣ
void HK_camera::Recorder(char* sFileName, bool stop)
{
	if (stop == 0){
		//���Ŀ¼�Ƿ���ڣ��������򴴽�
		if (_access(sFileName, 0) != 0) {//Ŀ¼������  �ڱ�׼C++�У��ú���Ϊ_access��λ��ͷ�ļ�<io.h>�У�
										 //����Linux�£��ú���Ϊaccess��λ��ͷ�ļ�<unistd.h>��
			if (_mkdir(sFileName))//����Ŀ¼   windows��Ϊ_mkdir, #include <direct.h>. 
								  //linux��Ϊmkdir�� #include <sys/stat.h>
				cout << "����Ŀ¼�ɹ�" << endl;
			else
				cout << "����Ŀ¼ʧ��" << endl;
		}

		int iRet = 0;
		char RecName[256] = { 0 };

		CTime CurTime = CTime::GetCurrentTime();//CTime�࣬��atltime.hͷ�ļ���
		sprintf_s(RecName, "%s%04d%02d%02d%02d%02d%02d.mp4", sFileName, CurTime.GetYear(), CurTime.GetMonth(), CurTime.GetDay(), \
			CurTime.GetHour(), CurTime.GetMinute(), CurTime.GetSecond());
		iRet = NET_DVR_SaveRealData(lRealPlayHandle, RecName);
		if (!iRet)
			cout << "����¼��ʧ��! Error number: " << NET_DVR_GetLastError() << endl;
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

			//�佹����
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
			if ((char)c == 27) break; //ESC���˳�
		}
	}

	camera.Logout();
	cv::destroyAllWindows();
	//system("pause");
	return 0;
}

//������ʾ
void detectAndDisplay(cv::Mat frame, HK_camera camera) {
	MTCNN mtcnn_detector("../3rd_x64/MTCNN_model");

	std::vector<cv::Mat> rgbChannels(3);
	cv::split(frame, rgbChannels);
	cv::Mat frame_gray = rgbChannels[2];// ת��Ϊ����ɫͨ���ĻҶ�ͼ��.�ӿ����ٶ�
	float factor = 0.709f;
	float threshold[3] = { 0.7f, 0.6f, 0.6f };
	int minSize = 100;

	vector<FaceInfo> faceInfo = mtcnn_detector.Detect_mtcnn(frame, minSize, threshold, factor, 3);
	for (int i = 0; i < faceInfo.size(); ++i) {
		int x = (int)faceInfo[i].bbox.xmin;
		int y = (int)faceInfo[i].bbox.ymin;
		int w = (int)(faceInfo[i].bbox.xmax - faceInfo[i].bbox.xmin + 1);
		int h = (int)(faceInfo[i].bbox.ymax - faceInfo[i].bbox.ymin + 1);

		//ʵ�ʵı佹�����㷨
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