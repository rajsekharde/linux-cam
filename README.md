# Linux-Cam: Webcam utility app for Liux

## Webcam output pipeline in Linux

In linux, devices are abstracted as files under the /dev directory. Webcam typically appears as /dev/video0. These files cannot be queried using regular commands like read(), as they stream high bandwidth video data in real time. Copying frames back and forth between kernel space and user space using standard read() calls would cause massive CPU overhead and drop frames. Instead, Linux uses Video4Linux2 (V4L2) and Memory Mapping (mmap).

V4L2 lifecycle:
1. Open the device using open() and standard file flags. This returns a file descriptor.
2. Query capabilities using ioctl(): Ask the driver what the hardware can do (does it support streaming? is it a video capture device? etc)
3. Set Image format using ioctl(): Negotiate the resolution (e.g., 640x480), pixel format (e.g., V4L2_PIX_FMT_YUYV or V4L2_PIX_FMT_MJPEG), and fields with the driver.
4. Request Buffers (ioctl with VIDIOC_REQBUFS): Ask the kernel to allocate a ring buffer of memory slots (usually 3 to 4 buffers) to hold video frames as they arrive from the USB bus.
5. Memory Map the Buffers (mmap): The kernel allocates buffers in its space. Call mmap() to map those kernel buffers directly into your user-space application's virtual memory. Now, the program can read the raw pixel bytes directly from pointer addresses without extra copying.
6. Queue Buffers (ioctl with VIDIOC_QBUF): You hand the allocated buffers back to the driver so it can fill them with incoming frames.
7. Start Streaming (ioctl with VIDIOC_STREAMON): The camera sensor turns on and starts pumping frames into the queue.
8. The Capture Loop:
- Dequeue (VIDIOC_DQBUF): Wait for the driver to fill a buffer with a fresh frame.
- Process/Copy: Extract or copy those bytes into your application's thread-safe memory store.
- Re-queue (VIDIOC_QBUF): Send the buffer back to the kernel so it can be reused for the next frame.
9. Stop Streaming (ioctl with VIDIOC_STREAMOFF): Cleanly shut down the device stream before exiting.

Image formats: When you ask the webcam for its format, it will likely offer one of two things:
- YUYV (YUV 4:2:2): Uncompressed format. Every 4 bytes represent 2 pixels (sharing color data, keeping luminance separate). It's great because you don't need a heavy decoder library—you can parse the bytes with simple math. However, it takes up more bandwidth.
- MJPEG (Motion JPEG): Compressed JPEG frames packed consecutively. It uses less USB bandwidth (great for high resolutions), but you will need to decode each frame using a library like libjpeg-turbo to turn it into raw RGB bytes.

