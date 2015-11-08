#ifndef RTMP_SMART_PUSHER_CPP
#define RTMP_SMART_PUSHER_CPP
#include <rtmp_sys.h>
#include <rtmp.h>
#include <iostream>
using namespace std;
class RtmpSmartPusher
{
	public:
		RtmpSmartPusher();
		~RtmpSmartPusher();
		bool Init(char* rtmpurl);
	private:
		bool SendH264();	
		void getSPSAndPPS(const unsigned char* sps_pps_data,int size);
		int SendPacket(unsigned int nPacketType,unsigned char *data,unsigned int size,unsigned int nTimestamp);
		bool SendH264Packet(unsigned char *data,unsigned int size,bool bIsKeyFrame,unsigned int nTimeStamp);
	private:
		RTMP *m_rtmp;	
};

#endif
