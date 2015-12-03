#ifndef RTMP_SMART_PUSHER_CPP
#define RTMP_SMART_PUSHER_CPP
#include <rtmp_sys.h>
#include <rtmp.h>
#include <iostream>
#include <pthread.h>
#include <list>
using namespace std;

typedef struct h264metadata
{
	unsigned char* sps;
	int sps_size;
	unsigned char* pps;
	int pps_size;
}h264metadata_st;

class RtmpSmartPusher
{
	public:
		RtmpSmartPusher();
		~RtmpSmartPusher();
		bool Init(char* rtmpurl);
		bool startPush();

	private:
		bool StartPusherH264();
		bool StartPusherAAC();
		bool senAudioSpecificConfig();
		bool SendAACPacket(unsigned char *data,unsigned int size,unsigned int nTimeStamp);
		bool connectRtmpServer(char* rtmpurl);
		void getSPSAndPPS(const unsigned char* sps_pps_data,int size);
		bool sendRtmp264Packet(unsigned char* data, int size , bool iskeyframe,int nTimeStamp);
		bool sendPacket(unsigned int nPacketType,unsigned char *data,unsigned int size,unsigned int nTimestamp);
		bool sendRtmpspsppsPacket(unsigned char* sps, int sps_size,unsigned char* pps , int pps_size);
		bool popQueueAndSendPacket();
		static void* do_video_push_thread(void* arg);
		static void* do_audio_push_thread(void* arg);
		static void* do_queue_push_thread(void* arg);
	private:
		RTMP *m_rtmp;
		h264metadata_st m_264metadata;

		pthread_t m_videoThread;
		pthread_t m_audioThread;
		pthread_t m_pushThread;
		pthread_mutex_t m_lck;
		pthread_mutex_t m_queuelck;
		pthread_cond_t cv;
		list<RTMPPacket*> m_packetQueue;
		int m_audiotime;
		int m_videotime;
		int m_audioover;
		int m_videoover;
};

#endif
