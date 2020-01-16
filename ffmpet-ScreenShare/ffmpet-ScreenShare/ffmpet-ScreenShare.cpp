// ffmpet-ScreenShare.cpp : �������̨Ӧ�ó������ڵ㡣
//

#include "stdafx.h"

#include "stdafx.h"
#include <iostream>
extern "C"
{
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <d3d9.h>
}
#pragma comment(lib, "swscale.lib")
#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib, "avformat.lib")
#pragma comment (lib,"d3d9.lib")

using namespace std;

uint8_t*  CaptureScreen();

AVFrame *readBmp2FrameMap();

int main(int argc, char *argv[])
{

	//nginx-rtmp ֱ��������rtmp����URL
	char *outUrl = "rtmp://192.168.116.144:1935/live/mystream";

	//ע�����еı������
	avcodec_register_all();

	//ע�����еķ�װ��
	av_register_all();

	//ע����������Э��
	avformat_network_init();


	//VideoCapture cam;
	//Mat frame;
	//namedWindow("video");

	//���ظ�ʽת��������
	SwsContext *vsc = NULL;

	//��������ݽṹ �洢��ѹ�������ݣ���Ƶ��ӦRGB/YUV�������ݣ���Ƶ��ӦPCM�������ݣ�
	AVFrame *yuv = NULL;

	//������������
	AVCodecContext *vc = NULL;

	//rtmp flv ��װ��
	AVFormatContext *ic = NULL;


	try
	{   ////////////////////////////////////////////////////////////////
		/// 1 ʹ��opencv��Դ��Ƶ��
		//��ȡ��Ƶ֡�ĳߴ�
		int inWidth = 0;
		int inHeight = 0;
		int nDep = 0;

		inWidth = GetSystemMetrics(SM_CXSCREEN);
		inHeight = GetSystemMetrics(SM_CYSCREEN);
		HDC hDC = ::GetDC(GetDesktopWindow());
		nDep = GetDeviceCaps(hDC, BITSPIXEL);
		::ReleaseDC(GetDesktopWindow(), hDC);

		int fps = 25;

		///2 ��ʼ����ʽת��������
		vsc = sws_getCachedContext(vsc,
			inWidth, inHeight, AV_PIX_FMT_BGR24,     //Դ���ߡ����ظ�ʽ
			inWidth, inHeight, AV_PIX_FMT_YUV420P,//Ŀ����ߡ����ظ�ʽ
			SWS_BICUBIC,  // �ߴ�仯ʹ���㷨
			0, 0, 0
			);
		if (!vsc)
		{
			throw exception("sws_getCachedContext failed!");
		}
		///3 ��ʼ����������ݽṹ��δѹ��������
		yuv = av_frame_alloc();
		yuv->format = AV_PIX_FMT_YUV420P;
		yuv->width = inWidth;
		yuv->height = inHeight;
		yuv->pts = 0;
		//����yuv�ռ�
		int ret = av_frame_get_buffer(yuv, 32);
		if (ret != 0)
		{
			char buf[1024] = { 0 };
			av_strerror(ret, buf, sizeof(buf) - 1);
			throw exception(buf);
		}

		///4 ��ʼ������������
		//a �ҵ�������
		AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_H264);
		if (!codec)
		{
			throw exception("Can`t find h264 encoder!");
		}
		//b ����������������
		vc = avcodec_alloc_context3(codec);
		if (!vc)
		{
			throw exception("avcodec_alloc_context3 failed!");
		}
		//c ���ñ���������
		vc->flags |= AV_CODEC_FLAG_GLOBAL_HEADER; //ȫ�ֲ���
		vc->codec_id = codec->id;
		vc->thread_count = 8;

		vc->bit_rate = 50 * 1024 * 8;//ѹ����ÿ����Ƶ��bitλ��С 50kB
		vc->width = inWidth;
		vc->height = inHeight;
		vc->time_base = { 1, fps };
		vc->framerate = { fps, 1 };

		//������Ĵ�С������֡һ���ؼ�֡
		vc->gop_size = 50;
		vc->max_b_frames = 0;
		vc->pix_fmt = AV_PIX_FMT_YUV420P;
		//d �򿪱�����������
		ret = avcodec_open2(vc, 0, 0);
		if (ret != 0)
		{
			char buf[1024] = { 0 };
			av_strerror(ret, buf, sizeof(buf) - 1);
			throw exception(buf);
		}
		cout << "avcodec_open2 success!" << endl;

		///5 �����װ������Ƶ������
		//a ���������װ��������
		ret = avformat_alloc_output_context2(&ic, 0, "flv", outUrl);
		if (ret != 0)
		{
			char buf[1024] = { 0 };
			av_strerror(ret, buf, sizeof(buf) - 1);
			throw exception(buf);
		}
		ic->max_interleave_delta = 0;
		//b �����Ƶ��   ����Ƶ����Ӧ�Ľṹ�壬��������Ƶ����롣
		AVStream *vs = avformat_new_stream(ic, NULL);
		if (!vs)
		{
			throw exception("avformat_new_stream failed");
		}
		//���ӱ�־�����һ��Ҫ����
		vs->codecpar->codec_tag = 0;
		//�ӱ��������Ʋ���
		avcodec_parameters_from_context(vs->codecpar, vc);
		av_dump_format(ic, 0, outUrl, 1);


		///��rtmp ���������IO  AVIOContext�����������Ӧ�Ľṹ�壬���������������д�ļ���RTMPЭ��ȣ���
		ret = avio_open(&ic->pb, outUrl, AVIO_FLAG_WRITE);
		if (ret != 0)
		{
			char buf[1024] = { 0 };
			av_strerror(ret, buf, sizeof(buf) - 1);
			throw exception(buf);
		}

		//д���װͷ
		ret = avformat_write_header(ic, NULL);
		if (ret != 0)
		{
			char buf[1024] = { 0 };
			av_strerror(ret, buf, sizeof(buf) - 1);
			throw exception(buf);
		}
		//�洢ѹ�����ݣ���Ƶ��ӦH.264���������ݣ���Ƶ��ӦAAC/MP3���������ݣ�
		AVPacket pack;
		memset(&pack, 0, sizeof(pack));
		int vpts = 0;
		for (;;)
		{

			uint8_t * data = CaptureScreen();
			///rgb to yuv
			//��������ݽṹ
			uint8_t *indata[AV_NUM_DATA_POINTERS] = { 0 };
			//indata[0] bgrbgrbgr
			//plane indata[0] bbbbb indata[1]ggggg indata[2]rrrrr 
			indata[0] = data;
			int insize[AV_NUM_DATA_POINTERS] = { 0 };
			//һ�У������ݵ��ֽ���
			insize[0] = (inWidth * 24 / 8 + 3) / 4 * 4;//frame.cols * frame.elemSize();
			int h = sws_scale(vsc, indata, insize, 0, inHeight, //Դ����
				yuv->data, yuv->linesize);
			av_free(data);
			if (h <= 0)
			{
				continue;
			}
			//cout << h << " " << flush;
			///h264����
			yuv->pts = vpts;
			vpts++;
			ret = avcodec_send_frame(vc, yuv);
			if (ret != 0)
				continue;

			ret = avcodec_receive_packet(vc, &pack);
			if (ret != 0 || pack.size > 0)
			{
				//cout << "*" << pack.size << flush;
			}
			else
			{
				continue;
			}
			//����pts dts��duration��
			pack.pts = av_rescale_q(pack.pts, vc->time_base, vs->time_base);
			pack.dts = av_rescale_q(pack.dts, vc->time_base, vs->time_base);
			pack.duration = av_rescale_q(pack.duration, vc->time_base, vs->time_base);


			ret = av_interleaved_write_frame(ic, &pack);

			if (ret == 0)
			{
				cout << "#" << flush;
			}
		}

	}
	catch (exception &ex)
	{
		if (vsc)
		{
			sws_freeContext(vsc);
			vsc = NULL;
		}

		if (vc)
		{
			avio_closep(&ic->pb);
			avcodec_free_context(&vc);
		}

		cerr << ex.what() << endl;
	}
	getchar();
	return 0;
}



AVFrame* readBmp2FrameMap(const char*bmpPath, int num)
{

	//�����ƶ���ʽ��ָ����ͼ���ļ�  
	FILE *fp = fopen(bmpPath, "rb");
	if (fp == 0) return 0;

	//����λͼ�ļ�ͷ�ṹBITMAPFILEHEADER  
	fseek(fp, sizeof(BITMAPFILEHEADER), 0);
	//����λͼ��Ϣͷ�ṹ��������ȡλͼ��Ϣͷ���ڴ棬����ڱ���head��  
	BITMAPINFOHEADER head;
	fread(&head, sizeof(BITMAPINFOHEADER), 1, fp);
	//��ȡͼ����ߡ�ÿ������ռλ������Ϣ  
	int biWidth = head.biWidth;
	int biHeight = head.biHeight;
	int biBitCount = head.biBitCount;
	//�������������ͼ��ÿ��������ռ���ֽ�����������4�ı�����  
	int lineByte = (biWidth * biBitCount / 8 + 3) / 4 * 4;
	//λͼ���
	if (biBitCount != 24 && biBitCount != 32)
	{
		printf("bmp file: %s  is not  24 or 32 bit\n ", bmpPath);
		return 0;
	}
	//����λͼ��������Ҫ�Ŀռ䣬��λͼ���ݽ��ڴ�  
	uint8_t* bmpBuffer = (uint8_t*)av_malloc(lineByte* biHeight);
	/*QTime time_;
	time_.start();*/
	fread(bmpBuffer, 1, lineByte * biHeight, fp);
	//�ر��ļ�  
	fclose(fp);
	/*std::cout << "read time: " << time_.elapsed() << "\n";*/
	//���ã�ת����
	uint8_t* tempData = (uint8_t*)av_malloc(lineByte*biHeight);
	for (int h = 0; h < biHeight; h++)
	{
		memcpy(tempData + (biHeight - 1 - h)*lineByte, bmpBuffer + (h*lineByte), lineByte);
	}
	memcpy(bmpBuffer, tempData, lineByte*biHeight);
	av_free(tempData);
	//
	AVFrame* rgbFrame = av_frame_alloc();
	AVPixelFormat pixFmt;
	if (biBitCount == 24)
		pixFmt = AV_PIX_FMT_RGB24;
	else if (biBitCount == 32)
		pixFmt = AV_PIX_FMT_RGB32;
	avpicture_fill((AVPicture *)rgbFrame, bmpBuffer, pixFmt, biWidth, biHeight);
	rgbFrame->width = biWidth;
	rgbFrame->height = biHeight;
	rgbFrame->linesize[0] = lineByte;
	rgbFrame->format = pixFmt;
	return rgbFrame;
}




AVFrame* readBmp2FrameMap()
{

	HWND DeskWnd = ::GetDesktopWindow();//��ȡ���洰�ھ��
	RECT DeskRC;
	::GetClientRect(DeskWnd, &DeskRC);//��ȡ���ڴ�С
	HDC DeskDC = GetDC(DeskWnd);//��ȡ����DC
	HBITMAP DeskBmp = ::CreateCompatibleBitmap(DeskDC, DeskRC.right, DeskRC.bottom);//����λͼ
	HDC memDC = ::CreateCompatibleDC(DeskDC);//����DC
	SelectObject(memDC, DeskBmp);//�Ѽ���λͼѡ�����DC��
	BitBlt(memDC, 0, 0, DeskRC.right, DeskRC.bottom, DeskDC, 0, 0, SRCCOPY);
	BITMAP bmInfo;
	DWORD bmDataSize;
	char *bmData;//λͼ����
	GetObject(DeskBmp, sizeof(BITMAP), &bmInfo);//����λͼ�������ȡλͼ��Ϣ
	bmDataSize = bmInfo.bmWidthBytes*bmInfo.bmHeight;//����λͼ���ݴ�С
	bmData = new char[bmDataSize];//��������
	BITMAPFILEHEADER bfh;//λͼ�ļ�ͷ
	bfh.bfType = 0x4d42;
	bfh.bfSize = bmDataSize + 54;
	bfh.bfReserved1 = 0;
	bfh.bfReserved2 = 0;
	bfh.bfOffBits = 54;

	BITMAPINFOHEADER bih;//λͼ��Ϣͷ
	bih.biSize = 40;
	bih.biWidth = bmInfo.bmWidth;
	bih.biHeight = bmInfo.bmHeight;
	bih.biPlanes = 1;
	bih.biBitCount = 24;
	bih.biCompression = BI_RGB;
	bih.biSizeImage = bmDataSize;
	bih.biXPelsPerMeter = 0;
	bih.biYPelsPerMeter = 0;
	bih.biClrUsed = 0;
	bih.biClrImportant = 0;
	::GetDIBits(DeskDC, DeskBmp, 0, bmInfo.bmHeight, bmData, (BITMAPINFO *)&bih, DIB_RGB_COLORS);//��ȡλͼ���ݲ���
	ReleaseDC(DeskWnd, DeskDC);
	DeleteDC(memDC);
	DeleteObject(DeskBmp);

	int sumSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + bmDataSize;
	char * stream = new char[sumSize];
	memcpy(stream, &bfh, sizeof(BITMAPFILEHEADER));
	memcpy(stream + sizeof(BITMAPFILEHEADER), &bih, sizeof(BITMAPINFOHEADER));
	memcpy(stream + sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER), bmData, bmDataSize);
	delete bmData;
	bmData = NULL;


	int lineByte = (bmInfo.bmWidth * bih.biBitCount / 8 + 3) / 4 * 4;

	//���ã�ת����
	uint8_t* tempData = (uint8_t*)av_malloc(bmDataSize);
	for (int h = 0; h < bmInfo.bmHeight; h++)
	{
		memcpy(tempData + (bmInfo.bmHeight - 1 - h)*lineByte, stream + (h*lineByte), lineByte);
	}
	memcpy(stream, tempData, lineByte*bmInfo.bmHeight);
	av_free(tempData);
	//
	AVFrame* rgbFrame = av_frame_alloc();
	AVPixelFormat pixFmt;
	pixFmt = AV_PIX_FMT_RGB24;
	avpicture_fill((AVPicture *)rgbFrame, (const uint8_t *)stream, pixFmt, bmInfo.bmWidth, bmInfo.bmHeight);
	rgbFrame->width = bmInfo.bmWidth;
	rgbFrame->height = bmInfo.bmHeight;
	rgbFrame->linesize[0] = lineByte;
	rgbFrame->format = pixFmt;
	return rgbFrame;
}


uint8_t*  CaptureScreen()
{
	HWND DeskWnd = ::GetDesktopWindow();//��ȡ���洰�ھ��
	RECT DeskRC;
	::GetClientRect(DeskWnd, &DeskRC);//��ȡ���ڴ�С
	HDC DeskDC = GetDC(DeskWnd);//��ȡ����DC
	HBITMAP DeskBmp = ::CreateCompatibleBitmap(DeskDC, DeskRC.right, DeskRC.bottom);//����λͼ
	HDC memDC = ::CreateCompatibleDC(DeskDC);//����DC
	SelectObject(memDC, DeskBmp);//�Ѽ���λͼѡ�����DC��
	BitBlt(memDC, 0, 0, DeskRC.right, DeskRC.bottom, DeskDC, 0, 0, SRCCOPY);
	BITMAP bmInfo;
	DWORD bmDataSize;
	char *bmData;//λͼ����
	GetObject(DeskBmp, sizeof(BITMAP), &bmInfo);//����λͼ�������ȡλͼ��Ϣ
	bmDataSize = bmInfo.bmWidthBytes*bmInfo.bmHeight;//����λͼ���ݴ�С
	bmData = new char[bmDataSize];//��������
	BITMAPFILEHEADER bfh;//λͼ�ļ�ͷ
	bfh.bfType = 0x4d42;
	bfh.bfSize = bmDataSize + 54;
	bfh.bfReserved1 = 0;
	bfh.bfReserved2 = 0;
	bfh.bfOffBits = 54;
	BITMAPINFOHEADER bih;//λͼ��Ϣͷ
	bih.biSize = 40;
	bih.biWidth = bmInfo.bmWidth;
	bih.biHeight = bmInfo.bmHeight;
	bih.biPlanes = 1;
	bih.biBitCount = 24;
	bih.biCompression = BI_RGB;
	bih.biSizeImage = bmDataSize;
	bih.biXPelsPerMeter = 0;
	bih.biYPelsPerMeter = 0;
	bih.biClrUsed = 0;
	bih.biClrImportant = 0;
	::GetDIBits(DeskDC, DeskBmp, 0, bmInfo.bmHeight, bmData, (BITMAPINFO *)&bih, DIB_RGB_COLORS);//��ȡλͼ���ݲ���
	ReleaseDC(DeskWnd, DeskDC);
	DeleteDC(memDC);
	DeleteObject(DeskBmp);

	int lineByte = (bmInfo.bmWidth * bih.biBitCount / 8 + 3) / 4 * 4;

	//���ã�ת����
	uint8_t* tempData = (uint8_t*)av_malloc(bmDataSize);
	for (int h = 0; h < bmInfo.bmHeight; h++)
	{
		memcpy(tempData + (bmInfo.bmHeight - 1 - h)*lineByte, bmData + (h*lineByte), lineByte);
	}
	//av_free(tempData);
	delete bmData;
	bmData = NULL;
	printf("*");
	return tempData;

}

