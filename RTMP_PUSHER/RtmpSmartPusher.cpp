#include "RtmpSmartPusher.h"
#include <arpa/inet.h>

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}
RtmpSmartPusher::RtmpSmartPusher():m_rtmp(NULL)
{

}

RtmpSmartPusher::~RtmpSmartPusher()
{

}

bool RtmpSmartPusher::Init(char* rtmpurl)
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
		SendH264();
		cout<<"send data over"<<endl;
		return true;
}

bool RtmpSmartPusher::SendH264()
{
		av_register_all();
		AVFormatContext *pFormatCtx = NULL;
		AVCodecContext *pCodecCtx = NULL;
		AVCodec *pCodec = NULL;

		pFormatCtx = avformat_alloc_context();
		if(pFormatCtx == NULL)
		{
				cout<<"alloc format error"<<endl;
				return false;
		}
		if(avformat_open_input(&pFormatCtx,"720p.h264",NULL,NULL) != 0)
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
		while(1)
		{
				AVPacket packet;
				if(av_read_frame(pFormatCtx,&packet) >= 0)
				{
						if(packet.stream_index == videoindex)
						{
								FILE *h264data = NULL;
								h264data = fopen("h264dara","wb");
								if(!h264data)
										return false;
								fwrite(packet.data,1,packet.size,h264data);
								fclose(h264data);
								int hasspspps = 0;
								if((packet.data[4] & 0x07) == 0x07 )
								{
										cout<<"exit sps"<<endl;
										hasspspps = 1;
								}
								bool iskey = false;
								if(packet.flags & AV_PKT_FLAG_KEY)
								{
									iskey = true;
								}
								else{
									iskey = false;
								}
								static int num = 0;
								if(num == 0)
								{
										num++;
										packet.data = packet.data + 22;
										packet.size -= 22;
								}
								else if(hasspspps == 1)
								{
										packet.data = packet.data + 22;
										packet.size -= 22;
								}
								static int timestamps = 0;
								if(!SendH264Packet(packet.data,packet.size,iskey,timestamps))
								{
										cout<<"send 264 data error"<<endl;
										return false;
								}
								timestamps += 40;
						}
				}
				else{
						cout<<"send h264 data end"<<endl;
						return true;
				}
		}
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
		for(int j = 0; j< sps_size;j++)
		{
				printf("%x",sps[j]);
		}
		unsigned char* pps = new unsigned char[ppssize];
		if(!pps)
			return;
		memset(pps,0,ppssize);
		memcpy(pps,sps_pps_data+pps_start,ppssize);

		

		//After Get sps pps need Packet send to RTMP server
		
		//int i = 0;
		i =0;  
		char body[1024];
		body[i++] = 0x17; // 1:keyframe  7:AVC  
		body[i++] = 0x00; // AVC sequence header  

		body[i++] = 0x00;  
		body[i++] = 0x00;  
		body[i++] = 0x00; // fill in 0;  

		body[i++] = 0x01; // configurationVersion  
		body[i++] = sps[0]; // AVCProfileIndication  
		body[i++] = sps[1]; // profile_compatibility  
		body[i++] = sps[2]; // AVCLevelIndication   
		body[i++] = 0xff; // lengthSizeMinusOne    
		// sps nums  
		body[i++] = 0xE1; //&0x1f  
		// sps data length
		body[i++] = sps_size >> 8; 

		body[i++] = sps_size & 0xff;
		// sps data  
		memcpy(&body[i],sps,sps_size);  
		i= i+sps_size;  

		// pps nums  
		body[i++] = 0x01; //&0x1f  
		// pps data length 
		body[i++] = ppssize >> 8;  
		body[i++] = ppssize & 0xff;
		// sps data  
		memcpy(&body[i],pps,ppssize);  
		i= i+ppssize;

		if(!SendPacket(RTMP_PACKET_TYPE_VIDEO,(unsigned char*)body,i,0))
		{
				cout<<"send packet error"<<endl;
				return ;
		}
}

int RtmpSmartPusher::SendPacket(unsigned int nPacketType,unsigned char *data,unsigned int size,unsigned int nTimestamp)  
{  

		RTMPPacket packet;  
		RTMPPacket_Reset(&packet);  
		RTMPPacket_Alloc(&packet,size);  

		packet.m_packetType = nPacketType;  
		packet.m_nChannel = 0x04;    
		packet.m_headerType = RTMP_PACKET_SIZE_LARGE;    
		packet.m_nTimeStamp = nTimestamp;    
		packet.m_nInfoField2 = m_rtmp->m_stream_id;  
		packet.m_nBodySize = size;
		memcpy(packet.m_body,data,size);  
		FILE* sendbody = NULL;
		sendbody = fopen("sendbody","wb");
		if(!sendbody)
				return 0;
		fwrite(packet.m_body,1,size,sendbody);
		fclose(sendbody);
		int nRet = RTMP_SendPacket(m_rtmp,&packet,0);  

		RTMPPacket_Free(&packet);  

		return nRet;  
}  

bool RtmpSmartPusher::SendH264Packet(unsigned char *data,unsigned int size,bool bIsKeyFrame,unsigned int nTimeStamp)  
{  
		if(data == NULL && size<11)  
		{  
				return false;  
		}  

		unsigned char *body = new unsigned char[size+9];  

		int i = 0;  
		if(bIsKeyFrame)  
		{  
				body[i++] = 0x17;// 1:Iframe  7:AVC  
		}  
		else  
		{  
				body[i++] = 0x27;// 2:Pframe  7:AVC  
		}  
		body[i++] = 0x01;// AVC NALU  
		body[i++] = 0x00;  
		body[i++] = 0x00;  
		body[i++] = 0x00;  

		// NALU size
		size -= 4;
		body[i++] = size>>24;
		body[i++] = size>>16;  
		body[i++] = size>>8;  
		body[i++] = size&0xff;;  
		FILE* framedata = NULL;
		framedata = fopen("framedata","wb");
		if(!framedata)
				return 0;
		fwrite(data+4,1,size,framedata);
		fclose(framedata);
		// NALU data  
		memcpy(&body[i],data+4,size);
		bool bRet = SendPacket(RTMP_PACKET_TYPE_VIDEO,body,i+size,nTimeStamp);

		delete[] body;  

		return bRet;  
}  

