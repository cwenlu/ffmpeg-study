#pragma once
#include <string>
#include <vector>



extern "C"
{
#include <libavformat/avformat.h>
}
using std::vector;
using std::string;

class SplitVideo
{
	public:
	/************************************************************************/
	/* 参数infile: 输入视频文件名(包括文件路径),outfile: 输出视频文件名(包括路径)*/
	/************************************************************************/
	SplitVideo(string infile,string outfile);


	~SplitVideo();


	/************************************************************************/
	/*            执行分片，参数splitSeconds为分片视频的时长（单位:秒）     */
	/************************************************************************/
	bool executeSplit(unsigned int splitSeconds);
	vector<string> getResultName();
	private:
	uint64_t splitFrameSize;
	vector<string> vecResultName;
	string suffixName;
	string inputFileName;
	string outputFileName;
	int video_index,audio_index;
	AVFormatContext *ifmtCtx=NULL, *ofmtCtx=NULL;
	bool writeVideoHeader(AVFormatContext *ifmt_ctx, AVFormatContext *ofmt_ctx, string out_filename);
	void flush_encoder(AVFormatContext *fmt_ctx, unsigned int stream_index);
	bool getInputFormatCtx(AVFormatContext *ifmt_ctx, string inputfile);
};

