.PHONY: capture

capture:
	gcc main.c -o app && ./app
	ffmpeg -f rawvideo -pixel_format yuyv422 -video_size 640x480 -i frame_output.raw frame.png