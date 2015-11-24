#ifndef RTMP_SMART_PUSHER_CPP
#define RTMP_SMART_PUSHER_CPP
#include <rtmp_sys.h>
#include <rtmp.h>
#include <iostream>
#include <pthread.h>
#include <list>
using namespace std;
class RtmpSmartPusher
{
	public:
		RtmpSmartPusher();
		~RtmpSmartPusher();
		bool Init(char* rtmpurl);
	private:
		bool SendH264();
		bool SendAAC();
		void getSPSAndPPS(const unsigned char* sps_pps_data,int size);
		int SendPacket(unsigned int nPacketType,unsigned char *data,unsigned int size,unsigned int nTimestamp);
		bool SendH264Packet(unsigned char *data,unsigned int size,bool bIsKeyFrame,unsigned int nTimeStamp);
		bool SendAACPacket(unsigned char *data,unsigned int size,bool bIsKeyFrame,unsigned int nTimeStamp);
		bool SendAVPacketToServer(RTMPPacket& packet);
		static void* audio_packet(void *arg);
		static void* video_packet(void *arg);
		static void* push_packet(void* arg);
	private:
		RTMP *m_rtmp;
		pthread_t m_audiopacket_encode_thread;
		pthread_t m_videopacket_encode_thread;
		pthread_t m_sendAVPacket_thread;
		pthread_mutex_t bufferlck;
		
		list<RTMPPacket> m_Queue;
		//vector<RTMPPacket*> m_Queue;

		int m_currentAudioTime;
		int m_currentVideoTime;
		pthread_cond_t cv;
		pthread_mutex_t audio_video_put_mutex;

};

#endif
