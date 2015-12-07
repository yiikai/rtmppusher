#include "RtmpSmartPusher.h"
int main()
{
		const char *data = "rtmp://127.0.0.1/myapp/test1";
		RtmpSmartPusher pusher;
		pusher.Init((char*)data);
		pusher.startPush();
		return 0;
}
