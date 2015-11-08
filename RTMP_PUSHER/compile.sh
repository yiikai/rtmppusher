g++ -g -o hello  main.cpp RtmpSmartPusher.cpp -I ../rtmpdump/librtmp/ -L ../rtmpdump/librtmp/ -lrtmp -L ../ffmpeg/out/lib/ -lm -lz -lpthread -lavformat -lavcodec -lavutil -lswresample -lx264
