#include "RtmpSmartPusher.h"
#include <arpa/inet.h>


extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

#define RTMP_HEAD_SIZE   (sizeof(RTMPPacket)+RTMP_MAX_HEADER_SIZE)

RtmpSmartPusher::RtmpSmartPusher():m_rtmp(NULL),m_audiotime(0),m_videotime(0),m_audioover(0),m_videoover(0)
{
	memset(&m_264metadata,0,sizeof(h264metadata));
}

RtmpSmartPusher::~RtmpSmartPusher()
{

}

bool RtmpSmartPusher::Init(char* rtmpurl)
{
	pthread_mutex_init(&m_lck,NULL);
	pthread_mutex_init(&m_queuelck,NULL);
	pthread_cond_init(&cv,NULL);
	connectRtmpServer(rtmpurl);
}

bool RtmpSmartPusher::connectRtmpServer(char* rtmpurl)
{
		m_rtmp = RTMP_Alloc();
		if(!m_rtmp)
		{
				cout<<"Alloc rtmp error"<<endl;
				return false;
		}
		RTMP_Init(m_rtmp);
		if(RTMP_SetupURL(m_rtmp,rtmpurl) == FALSE)
		{
				cout<<"set rtmp url error"<<endl;
				return false;
		}
		RTMP_EnableWrite(m_rtmp);
		if(RTMP_Connect(m_rtmp,NULL) == FALSE)
		{
				cout<<"connect rtmp server error"<<endl;
				return false;
		}
		if(RTMP_ConnectStream(m_rtmp,0) == FALSE)
		{
				cout<<"Connect stream with RTMP error"<<endl;
				return false;
		}
		av_register_all();
		return true;
}

bool RtmpSmartPusher::startPush()
{
	int ret = 0;
	ret = pthread_create(&m_videoThread,NULL,do_audio_push_thread,this);
	if(ret != 0)
		return false;
	ret = pthread_create(&m_audioThread,NULL,do_video_push_thread,this);
	if(ret != 0)
		return false;
	ret = pthread_create(&m_pushThread,NULL,do_queue_push_thread,this);
	if(ret != 0)
		return false;
	int audioover = 0;
	int videoover = 0;
	void *pvideoover = &videoover;
	void *paudioover = &audioover;
	pthread_join(m_audioThread,&(paudioover));
	pthread_join(m_videoThread,&(pvideoover));
	pthread_join(m_pushThread,NULL);

}

void* RtmpSmartPusher::do_queue_push_thread(void* arg)
{
	RtmpSmartPusher* rtmppusher = (RtmpSmartPusher*)arg;
	rtmppusher->popQueueAndSendPacket();
}

void* RtmpSmartPusher::do_video_push_thread(void* arg)
{
	RtmpSmartPusher* rtmppusher = (RtmpSmartPusher*)arg;
	rtmppusher->StartPusherH264();
	rtmppusher->m_videoover = 1;
	return &(rtmppusher->m_videoover);
}

void* RtmpSmartPusher::do_audio_push_thread(void* arg)
{
	RtmpSmartPusher* rtmppusher = (RtmpSmartPusher*)arg;
	rtmppusher->StartPusherAAC();
	rtmppusher->m_audioover = 1;
	return &(rtmppusher->m_audioover);
}


bool RtmpSmartPusher::popQueueAndSendPacket()
{
	while(m_audioover == 0 || m_videoover == 0)
	{
		if(m_packetQueue.empty())
		{
			continue;
		}
		RTMPPacket* packet = m_packetQueue.front();
		if(packet)
		{
			if (RTMP_IsConnected(m_rtmp))
			{
				cout<<"start send packet"<<endl;
				int nRet = RTMP_SendPacket(m_rtmp,packet,TRUE); /*TRUE为放进发送队列,FALSE是不放进发送队列,直接发送*/
				cout<<"over send packet"<<endl;
			}
		}
		free(packet);
		m_packetQueue.pop_front();
	}
	cout<<"RTMP video and audio data send over"<<endl;
}

bool RtmpSmartPusher::StartPusherAAC()
{
	AVFormatContext *pFormatCtx = NULL;
	AVCodecContext *pCodecCtx = NULL;
	AVCodec *pCodec = NULL;

	pFormatCtx = avformat_alloc_context();
	if(pFormatCtx == NULL)
	{
		cout<<"alloc format error"<<endl;
		return false;
	}
	if(avformat_open_input(&pFormatCtx,"audio.aac",NULL,NULL) != 0)
	{
		cout<<"open input file error"<<endl;
		return false;
	}
	if(avformat_find_stream_info(pFormatCtx,NULL) < 0 )
	{
		cout<<"find stream info error"<<endl;
		return false;
	}
	int audioindex = -1;
	for(int i = 0; i < pFormatCtx->nb_streams;i++)
	{
		if(pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO)
		{
				audioindex = i;
				break;
		}
	}
	if(audioindex == -1)
	{
		cout<<"can't find any video"<<endl;
		return false;
	}
	pCodecCtx = pFormatCtx->streams[audioindex]->codec;
	pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
	if(pCodec == NULL)
	{
		cout<<"can't find codec "<<endl;
		return false;
	}
		
	if(avcodec_open2(pCodecCtx,pCodec,NULL) < 0)
	{
		cout<<"open codec error"<<endl;
		return false;
	}
	senAudioSpecificConfig();
	cout<<"send audio spec"<<endl;
	while(1)
	{
			AVPacket packet;
			if(av_read_frame(pFormatCtx,&packet) >= 0)
			{
					if(packet.stream_index == audioindex)
					{
						FILE *aacdata = NULL;
						aacdata = fopen("aacdata","wb");
						if(!aacdata)
								return false;
						fwrite(packet.data,1,packet.size,aacdata);
						fclose(aacdata);
						unsigned char* data = packet.data + 7;  //传送rawdata 跳过前面的7个adts字节
						unsigned int num = packet.size - 7;
						
						static int inc = 0;
						int timestamp = (inc++)*(1024*1000/44100);
						cout<<"start read audio frame"<<endl;
						pthread_mutex_lock(&m_lck);
						cout<<"start in audio mutex"<<endl;
						cout<<" audio thread m_audiotime is "<<m_audiotime<<endl;
						cout<<"audio thread m_videotime is "<<m_videotime<<endl;
						if(m_audiotime <= m_videotime)
						{
							m_audiotime = timestamp;
							SendAACPacket(data,num,timestamp);
							if(m_audiotime > m_videotime)
							{
								pthread_mutex_unlock(&m_lck);
								pthread_cond_signal(&cv);
								cout<<"unlock audio"<<endl;
							}
							pthread_mutex_unlock(&m_lck);						
						}
						else
						{
							cout<<"wait video arrive"<<endl;
							if(m_videoover == 0)
								pthread_cond_wait(&cv,&m_lck);
							m_audiotime = timestamp;
							SendAACPacket(data,num,timestamp);
							pthread_mutex_unlock(&m_lck);
						}
					}
			}
			else
			{
				cout<<"send AAC data end"<<endl;
				return true;
			}
	}
	avcodec_close(pCodecCtx);
	avformat_close_input(&pFormatCtx);
}

bool RtmpSmartPusher::SendAACPacket(unsigned char *data,unsigned int size,unsigned int nTimeStamp)
{
	unsigned char *body = new unsigned char[size + 2];
	memset(body,0,size+2);
	int num = 0;
	body[num] = 0xAF;
	num++;
	body[num] = 0x01;
	num++;
	memcpy(body+2,data,size);
	RTMPPacket* packet;
	/*分配包内存和初始化,len为包体长度*/
	packet = (RTMPPacket *)malloc(RTMP_HEAD_SIZE+size+2);
	memset(packet,0,RTMP_HEAD_SIZE);
	packet->m_body = (char *)packet + RTMP_HEAD_SIZE;
	//memcpy(packet->m_body,data,size);

	packet->m_packetType = RTMP_PACKET_TYPE_AUDIO;   
	packet->m_nChannel = 0x04;    
	packet->m_headerType = RTMP_PACKET_SIZE_LARGE;    
	packet->m_nTimeStamp = nTimeStamp;    
	packet->m_nInfoField2 = m_rtmp->m_stream_id;  
	packet->m_nBodySize = size+2;
	memcpy(packet->m_body,body,packet->m_nBodySize);
	//cout<<"start send AAC packet"<<endl;
	pthread_mutex_lock(&m_queuelck);
	m_packetQueue.push_back(packet);
	pthread_mutex_unlock(&m_queuelck);
}

bool RtmpSmartPusher::senAudioSpecificConfig()
{
	unsigned char *body = new unsigned char[1024];
	memset(body,0,1024);
	int num = 0;
	body[num++] = 0xAF;
	body[num++] = 0x00;
	body[num++] = 0x12;
	body[num++] = 0x10;
	sendPacket(RTMP_PACKET_TYPE_AUDIO,(unsigned char*)body,num,0);
	delete[] body;
}

bool RtmpSmartPusher::StartPusherH264()
{
		
		AVFormatContext *pFormatCtx = NULL;
		AVCodecContext *pCodecCtx = NULL;
		AVCodec *pCodec = NULL;

		pFormatCtx = avformat_alloc_context();
		if(pFormatCtx == NULL)
		{
				cout<<"alloc format error"<<endl;
				return false;
		}
		if(avformat_open_input(&pFormatCtx,"out.h264",NULL,NULL) != 0)
		{
				cout<<"open input file error"<<endl;
				return false;
		}
		if(avformat_find_stream_info(pFormatCtx,NULL) < 0 )
		{
				cout<<"find stream info error"<<endl;
				return false;
		}
		int videoindex = -1;
		for(int i = 0; i < pFormatCtx->nb_streams;i++)
		{
				if(pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
				{
						videoindex = i;
						break;
				}
		}
		if(videoindex == -1)
		{
				cout<<"can't find any video"<<endl;
				return false;
		}
		pCodecCtx = pFormatCtx->streams[videoindex]->codec;
		pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
		if(pCodec == NULL)
		{
				cout<<"can't find codec "<<endl;
				return false;
		}
		
		if(avcodec_open2(pCodecCtx,pCodec,NULL) < 0)
		{
				cout<<"open codec error"<<endl;
				return false;
		}
		FILE *dump = NULL;
		dump = fopen("dump.txt","wb");
		if(!dump)
				return false;
		int num = 0;
		num = fwrite(pCodecCtx->extradata,1,pCodecCtx->extradata_size,dump);
		if(num != pCodecCtx->extradata_size)
		{
				cout<<"write error"<<endl;
				return false;
		}
		fclose(dump);
		getSPSAndPPS(pCodecCtx->extradata,pCodecCtx->extradata_size);
		cout<<"sps size is "<<m_264metadata.sps_size<<endl<<"pps size is "<<m_264metadata.pps_size<<endl;
		bool iskey = false;
		while(1)
		{
				AVPacket packet;
				if(av_read_frame(pFormatCtx,&packet) >= 0)
				{
						if(packet.stream_index == videoindex)
						{	
							if((packet.data[4] & 0x07) == 0x07 || (packet.data[4] & 0x08) == 0x08)
							{
								
								packet.data = packet.data + pCodecCtx->extradata_size;
								packet.size = packet.size - pCodecCtx->extradata_size;
								
							}
							if(packet.flags & AV_PKT_FLAG_KEY)
							{
								iskey = true;
							}
							else{
								iskey = false;
							}
							int last_update=RTMP_GetTime();
							static int inc = 0;
							int timestamp = (inc++)*(1000/15);
							cout<<"start video frame read"<<endl;
							pthread_mutex_lock(&m_lck);
							cout<<"in video frame mutex"<<endl;
							cout<<"m_audiotime is "<<m_audiotime<<endl;
							cout<<"m_videotime is "<<m_videotime<<endl;
							if(m_videotime <= m_audiotime)
							{
								m_videotime = timestamp;
								sendRtmp264Packet(packet.data,packet.size,iskey,timestamp);
								if(m_videotime > m_audiotime)
								{
									cout<<"lock video"<<endl;
									pthread_mutex_unlock(&m_lck);
									pthread_cond_signal(&cv);
									cout<<"unlock video"<<endl;
								}
								pthread_mutex_unlock(&m_lck);
							}
							else
							{
								cout<<"m_video is > m_audio"<<endl;
								pthread_cond_wait(&cv,&m_lck);
								m_videotime = timestamp;
								sendRtmp264Packet(packet.data,packet.size,iskey,timestamp);
								if(m_videotime > m_audiotime)
								{
									
									pthread_mutex_unlock(&m_lck);
									pthread_cond_signal(&cv);
									continue;
								}
								pthread_mutex_unlock(&m_lck);
								cout<<"unlock video "<<endl;
							}
							int now=RTMP_GetTime();
						}
						
				}
				else
				{
					cout<<"read 264 frame end"<<endl;
					return true;
				}
		}
	avcodec_close(pCodecCtx);
	avformat_close_input(&pFormatCtx);
	return true;
}

bool RtmpSmartPusher::sendRtmp264Packet(unsigned char* data, int size , bool iskeyframe,int nTimeStamp)
{
	if(data == NULL && size<11)
	{  
		return false;  
	}
	unsigned char *body = (unsigned char*)malloc(size+9);  
	memset(body,0,size+9);
	int i = 0;
	if(iskeyframe == true)
	{
		body[i++] = 0x17;// 1:Iframe  7:AVC   
		body[i++] = 0x01;// AVC NALU   
		body[i++] = 0x00;  
		body[i++] = 0x00;  
		body[i++] = 0x00;  


		// NALU size   
		body[i++] = size>>24 &0xff;  
		body[i++] = size>>16 &0xff;  
		body[i++] = size>>8 &0xff;  
		body[i++] = size&0xff;
		// NALU data   
		memcpy(&body[i],data,size);
		sendRtmpspsppsPacket(m_264metadata.sps,m_264metadata.sps_size,m_264metadata.pps,m_264metadata.pps_size);
	}
	else
	{
		body[i++] = 0x27;// 2:Pframe  7:AVC   
		body[i++] = 0x01;// AVC NALU   
		body[i++] = 0x00;  
		body[i++] = 0x00;  
		body[i++] = 0x00;  

		// NALU size   
		body[i++] = size>>24 &0xff;  
		body[i++] = size>>16 &0xff;  
		body[i++] = size>>8 &0xff;  
		body[i++] = size&0xff;
		// NALU data   
		memcpy(&body[i],data,size);
	}
	sendPacket(RTMP_PACKET_TYPE_VIDEO,body,i+size,nTimeStamp);
	return true;
}

bool RtmpSmartPusher::sendPacket(unsigned int nPacketType,unsigned char *data,unsigned int size,unsigned int nTimestamp)
{
	RTMPPacket* packet;
	/*分配包内存和初始化,len为包体长度*/
	packet = (RTMPPacket *)malloc(RTMP_HEAD_SIZE+size);
	memset(packet,0,RTMP_HEAD_SIZE);
	packet->m_body = (char *)packet + RTMP_HEAD_SIZE;
	memcpy(packet->m_body,data,size);
	packet->m_nBodySize = size;
	packet->m_hasAbsTimestamp = 0;
	packet->m_packetType = nPacketType; /*此处为类型有两种一种是音频,一种是视频*/
	packet->m_nInfoField2 = m_rtmp->m_stream_id;
	packet->m_nChannel = 0x04;
	packet->m_headerType = RTMP_PACKET_SIZE_LARGE;
	packet->m_nTimeStamp = nTimestamp;
	
	pthread_mutex_lock(&m_queuelck);
	m_packetQueue.push_back(packet);
	pthread_mutex_unlock(&m_queuelck);
	return true;
}


bool RtmpSmartPusher::sendRtmpspsppsPacket(unsigned char* sps, int sps_size,unsigned char* pps , int pps_size)
{
	RTMPPacket *packet = NULL;
	packet = (RTMPPacket *)malloc(RTMP_HEAD_SIZE+1024);
	unsigned char * body=NULL;
	memset(packet,0,RTMP_HEAD_SIZE+1024);
	packet->m_body = (char *)packet + RTMP_HEAD_SIZE;
	body = (unsigned char *)packet->m_body;

	int i = 0;
	body[i++] = 0x17;
	body[i++] = 0x00;

	body[i++] = 0x00;
	body[i++] = 0x00;
	body[i++] = 0x00;

	/*AVCDecoderConfigurationRecord*/
	body[i++] = 0x01;
	body[i++] = sps[1];
	body[i++] = sps[2];
	body[i++] = sps[3];
	body[i++] = 0xff;

	/*sps*/
	body[i++]   = 0xe1;
	body[i++] = (sps_size >> 8) & 0xff;
	body[i++] = sps_size & 0xff;
	memcpy(&body[i],sps,sps_size);
	i +=  sps_size;

	/*pps*/
	body[i++]   = 0x01;
	body[i++] = (pps_size >> 8) & 0xff;
	body[i++] = (pps_size) & 0xff;
	memcpy(&body[i],pps,pps_size);
	i +=  pps_size;

	packet->m_packetType = RTMP_PACKET_TYPE_VIDEO;
	packet->m_nBodySize = i;
	packet->m_nChannel = 0x04;
	packet->m_nTimeStamp = 0;
	packet->m_hasAbsTimestamp = 0;
	packet->m_headerType = RTMP_PACKET_SIZE_LARGE;
	packet->m_nInfoField2 = m_rtmp->m_stream_id;
	pthread_mutex_lock(&m_queuelck);
	m_packetQueue.push_back(packet);
	pthread_mutex_unlock(&m_queuelck);
	//int nRet = RTMP_SendPacket(m_rtmp,packet,0);
	//packet->m_body = NULL;
	//delete packet;
	//free(packet);
	return true;
}

void RtmpSmartPusher::getSPSAndPPS(const unsigned char* sps_pps_data,int size)
{	
		int i = 0;
		for(;i<size-4;i++)
		{
				if(sps_pps_data[i] ==0x00 && sps_pps_data[i+1] == 0x00 && sps_pps_data[i+2] == 0x00 && sps_pps_data[i+3] == 0x01)
				{
						i = i+3+1;
						break;	
				}
		}	
		int sps_start = i;
		int sps_end = 0;
		for(; i<size-4;i++)
		{
			if(sps_pps_data[i] ==0x00 && sps_pps_data[i+1] == 0x00 && sps_pps_data[i+2] == 0x00 && sps_pps_data[i+3] == 0x01)
			{
				sps_end = i-1;
				i = i+3;
				break;	
			}
		}
		int pps_start = i+1;
		int ppssize = size - pps_start;
		int sps_size = sps_end-sps_start + 1;
		unsigned char* sps = new unsigned char[sps_size];
		if(!sps)
			return;
		memset(sps,0,sps_size);
		memcpy(sps,sps_pps_data+sps_start,sps_size);
		m_264metadata.sps = sps;
		m_264metadata.sps_size = sps_size;

		for(int j = 0; j< sps_size;j++)
		{
				printf("%x",sps[j]);
		}
		unsigned char* pps = new unsigned char[ppssize];
		if(!pps)
			return;
		memset(pps,0,ppssize);
		memcpy(pps,sps_pps_data+pps_start,ppssize);
		m_264metadata.pps = pps;
		m_264metadata.pps_size = ppssize;
}