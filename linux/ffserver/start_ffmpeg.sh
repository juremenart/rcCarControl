
#ffmpeg -loglevel trace -f v4l2 -video_size 640x480 -r 15 -pixel_format yuyv422 -i /dev/video0 /tmp/bla.avi
ffmpeg -f v4l2 -video_size 640x480 -pixel_format yuyv422 -i /dev/video0 http://localhost:8090/feed1.ffm
