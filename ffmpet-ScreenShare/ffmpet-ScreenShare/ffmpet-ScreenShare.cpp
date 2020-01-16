// ffmpet-ScreenShare.cpp : 定义控制台应用程序的入口点。
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

	//nginx-rtmp 直播服务器rtmp推流URL
	char *outUrl = "rtmp://192.168.116.144:1935/live/mystream";

	//注册所有的编解码器
	avcodec_register_all();

	//注册所有的封装器
	av_register_all();

	//注册所有网络协议
	avformat_network_init();


	//VideoCapture cam;
	//Mat frame;
	//namedWindow("video");

	//像素格式转换上下文
	SwsContext *vsc = NULL;

	//输出的数据结构 存储非压缩的数据（视频对应RGB/YUV像素数据，音频对应PCM采样数据）
	AVFrame *yuv = NULL;

	//编码器上下文
	AVCodecContext *vc = NULL;

	//rtmp flv 封装器
	AVFormatContext *ic = NULL;


	try
	{   ////////////////////////////////////////////////////////////////
		/// 1 使用opencv打开源视频流
		//获取视频帧的尺寸
		int inWidth = 0;
		int inHeight = 0;
		int nDep = 0;

		inWidth = GetSystemMetrics(SM_CXSCREEN);
		inHeight = GetSystemMetrics(SM_CYSCREEN);
		HDC hDC = ::GetDC(GetDesktopWindow());
		nDep = GetDeviceCaps(hDC, BITSPIXEL);
		::ReleaseDC(GetDesktopWindow(), hDC);

		int fps = 25;

		///2 初始化格式转换上下文
		vsc = sws_getCachedContext(vsc,
			inWidth, inHeight, AV_PIX_FMT_BGR24,     //源宽、高、像素格式
			inWidth, inHeight, AV_PIX_FMT_YUV420P,//目标宽、高、像素格式
			SWS_BICUBIC,  // 尺寸变化使用算法
			0, 0, 0
			);
		if (!vsc)
		{
			throw exception("sws_getCachedContext failed!");
		}
		///3 初始化输出的数据结构。未压缩的数据
		yuv = av_frame_alloc();
		yuv->format = AV_PIX_FMT_YUV420P;
		yuv->width = inWidth;
		yuv->height = inHeight;
		yuv->pts = 0;
		//分配yuv空间
		int ret = av_frame_get_buffer(yuv, 32);
		if (ret != 0)
		{
			char buf[1024] = { 0 };
			av_strerror(ret, buf, sizeof(buf) - 1);
			throw exception(buf);
		}

		///4 初始化编码上下文
		//a 找到编码器
		AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_H264);
		if (!codec)
		{
			throw exception("Can`t find h264 encoder!");
		}
		//b 创建编码器上下文
		vc = avcodec_alloc_context3(codec);
		if (!vc)
		{
			throw exception("avcodec_alloc_context3 failed!");
		}
		//c 配置编码器参数
		vc->flags |= AV_CODEC_FLAG_GLOBAL_HEADER; //全局参数
		vc->codec_id = codec->id;
		vc->thread_count = 8;

		vc->bit_rate = 50 * 1024 * 8;//压缩后每秒视频的bit位大小 50kB
		vc->width = inWidth;
		vc->height = inHeight;
		vc->time_base = { 1, fps };
		vc->framerate = { fps, 1 };

		//画面组的大小，多少帧一个关键帧
		vc->gop_size = 50;
		vc->max_b_frames = 0;
		vc->pix_fmt = AV_PIX_FMT_YUV420P;
		//d 打开编码器上下文
		ret = avcodec_open2(vc, 0, 0);
		if (ret != 0)
		{
			char buf[1024] = { 0 };
			av_strerror(ret, buf, sizeof(buf) - 1);
			throw exception(buf);
		}
		cout << "avcodec_open2 success!" << endl;

		///5 输出封装器和视频流配置
		//a 创建输出封装器上下文
		ret = avformat_alloc_output_context2(&ic, 0, "flv", outUrl);
		if (ret != 0)
		{
			char buf[1024] = { 0 };
			av_strerror(ret, buf, sizeof(buf) - 1);
			throw exception(buf);
		}
		ic->max_interleave_delta = 0;
		//b 添加视频流   视音频流对应的结构体，用于视音频编解码。
		AVStream *vs = avformat_new_stream(ic, NULL);
		if (!vs)
		{
			throw exception("avformat_new_stream failed");
		}
		//附加标志，这个一定要设置
		vs->codecpar->codec_tag = 0;
		//从编码器复制参数
		avcodec_parameters_from_context(vs->codecpar, vc);
		av_dump_format(ic, 0, outUrl, 1);


		///打开rtmp 的网络输出IO  AVIOContext：输入输出对应的结构体，用于输入输出（读写文件，RTMP协议等）。
		ret = avio_open(&ic->pb, outUrl, AVIO_FLAG_WRITE);
		if (ret != 0)
		{
			char buf[1024] = { 0 };
			av_strerror(ret, buf, sizeof(buf) - 1);
			throw exception(buf);
		}

		//写入封装头
		ret = avformat_write_header(ic, NULL);
		if (ret != 0)
		{
			char buf[1024] = { 0 };
			av_strerror(ret, buf, sizeof(buf) - 1);
			throw exception(buf);
		}
		//存储压缩数据（视频对应H.264等码流数据，音频对应AAC/MP3等码流数据）
		AVPacket pack;
		memset(&pack, 0, sizeof(pack));
		int vpts = 0;
		for (;;)
		{

			uint8_t * data = CaptureScreen();
			///rgb to yuv
			//输入的数据结构
			uint8_t *indata[AV_NUM_DATA_POINTERS] = { 0 };
			//indata[0] bgrbgrbgr
			//plane indata[0] bbbbb indata[1]ggggg indata[2]rrrrr 
			indata[0] = data;
			int insize[AV_NUM_DATA_POINTERS] = { 0 };
			//一行（宽）数据的字节数
			insize[0] = (inWidth * 24 / 8 + 3) / 4 * 4;//frame.cols * frame.elemSize();
			int h = sws_scale(vsc, indata, insize, 0, inHeight, //源数据
				yuv->data, yuv->linesize);
			av_free(data);
			if (h <= 0)
			{
				continue;
			}
			//cout << h << " " << flush;
			///h264编码
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
			//计算pts dts和duration。
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

	//二进制读方式打开指定的图像文件  
	FILE *fp = fopen(bmpPath, "rb");
	if (fp == 0) return 0;

	//跳过位图文件头结构BITMAPFILEHEADER  
	fseek(fp, sizeof(BITMAPFILEHEADER), 0);
	//定义位图信息头结构变量，读取位图信息头进内存，存放在变量head中  
	BITMAPINFOHEADER head;
	fread(&head, sizeof(BITMAPINFOHEADER), 1, fp);
	//获取图像宽、高、每像素所占位数等信息  
	int biWidth = head.biWidth;
	int biHeight = head.biHeight;
	int biBitCount = head.biBitCount;
	//定义变量，计算图像每行像素所占的字节数（必须是4的倍数）  
	int lineByte = (biWidth * biBitCount / 8 + 3) / 4 * 4;
	//位图深度
	if (biBitCount != 24 && biBitCount != 32)
	{
		printf("bmp file: %s  is not  24 or 32 bit\n ", bmpPath);
		return 0;
	}
	//申请位图数据所需要的空间，读位图数据进内存  
	uint8_t* bmpBuffer = (uint8_t*)av_malloc(lineByte* biHeight);
	/*QTime time_;
	time_.start();*/
	fread(bmpBuffer, 1, lineByte * biHeight, fp);
	//关闭文件  
	fclose(fp);
	/*std::cout << "read time: " << time_.elapsed() << "\n";*/
	//倒置（转正）
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

	HWND DeskWnd = ::GetDesktopWindow();//获取桌面窗口句柄
	RECT DeskRC;
	::GetClientRect(DeskWnd, &DeskRC);//获取窗口大小
	HDC DeskDC = GetDC(DeskWnd);//获取窗口DC
	HBITMAP DeskBmp = ::CreateCompatibleBitmap(DeskDC, DeskRC.right, DeskRC.bottom);//兼容位图
	HDC memDC = ::CreateCompatibleDC(DeskDC);//兼容DC
	SelectObject(memDC, DeskBmp);//把兼容位图选入兼容DC中
	BitBlt(memDC, 0, 0, DeskRC.right, DeskRC.bottom, DeskDC, 0, 0, SRCCOPY);
	BITMAP bmInfo;
	DWORD bmDataSize;
	char *bmData;//位图数据
	GetObject(DeskBmp, sizeof(BITMAP), &bmInfo);//根据位图句柄，获取位图信息
	bmDataSize = bmInfo.bmWidthBytes*bmInfo.bmHeight;//计算位图数据大小
	bmData = new char[bmDataSize];//分配数据
	BITMAPFILEHEADER bfh;//位图文件头
	bfh.bfType = 0x4d42;
	bfh.bfSize = bmDataSize + 54;
	bfh.bfReserved1 = 0;
	bfh.bfReserved2 = 0;
	bfh.bfOffBits = 54;

	BITMAPINFOHEADER bih;//位图信息头
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
	::GetDIBits(DeskDC, DeskBmp, 0, bmInfo.bmHeight, bmData, (BITMAPINFO *)&bih, DIB_RGB_COLORS);//获取位图数据部分
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

	//倒置（转正）
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
	HWND DeskWnd = ::GetDesktopWindow();//获取桌面窗口句柄
	RECT DeskRC;
	::GetClientRect(DeskWnd, &DeskRC);//获取窗口大小
	HDC DeskDC = GetDC(DeskWnd);//获取窗口DC
	HBITMAP DeskBmp = ::CreateCompatibleBitmap(DeskDC, DeskRC.right, DeskRC.bottom);//兼容位图
	HDC memDC = ::CreateCompatibleDC(DeskDC);//兼容DC
	SelectObject(memDC, DeskBmp);//把兼容位图选入兼容DC中
	BitBlt(memDC, 0, 0, DeskRC.right, DeskRC.bottom, DeskDC, 0, 0, SRCCOPY);
	BITMAP bmInfo;
	DWORD bmDataSize;
	char *bmData;//位图数据
	GetObject(DeskBmp, sizeof(BITMAP), &bmInfo);//根据位图句柄，获取位图信息
	bmDataSize = bmInfo.bmWidthBytes*bmInfo.bmHeight;//计算位图数据大小
	bmData = new char[bmDataSize];//分配数据
	BITMAPFILEHEADER bfh;//位图文件头
	bfh.bfType = 0x4d42;
	bfh.bfSize = bmDataSize + 54;
	bfh.bfReserved1 = 0;
	bfh.bfReserved2 = 0;
	bfh.bfOffBits = 54;
	BITMAPINFOHEADER bih;//位图信息头
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
	::GetDIBits(DeskDC, DeskBmp, 0, bmInfo.bmHeight, bmData, (BITMAPINFO *)&bih, DIB_RGB_COLORS);//获取位图数据部分
	ReleaseDC(DeskWnd, DeskDC);
	DeleteDC(memDC);
	DeleteObject(DeskBmp);

	int lineByte = (bmInfo.bmWidth * bih.biBitCount / 8 + 3) / 4 * 4;

	//倒置（转正）
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

