#include "RtmpSmartPusher.h"
int main()
{
		const char *data = "rtmp://127.0.0.1/hls/test1";
		RtmpSmartPusher pusher;
		pusher.Init((char*)data);
		return 0;
}
