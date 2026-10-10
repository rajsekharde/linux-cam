#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>

#define CAMERA_DEVICE "/dev/video0"
#define WIDTH 640
#define HEIGHT 480

// Wrapper for ioctl with error handling for interrupted system calls
static int xioctl(int fd, int request, void *arg)
{
    int r;
    do
    {
        r = ioctl(fd, request, arg);
    } while (-1 == r && EINTR == errno);
    return r;
}

int main()
{
    // 1. Open the video device file
    int fd = open(CAMERA_DEVICE, O_RDWR);
    if (fd == -1)
    {
        perror("Failed to open webcam");
        return 1;
    }

    // 2. Set the image format (Resolution and Pixel Format: YUYV)
    struct v4l2_format fmt = {0};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = WIDTH;
    fmt.fmt.pix.height = HEIGHT;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;

    if (xioctl(fd, VIDIOC_S_FMT, &fmt) == -1)
    {
        perror("Failed to set pixel format");
        close(fd);
        return 1;
    }

    // 3. Request memory-mapped buffers from the kernel (we just want 1 buffer here)
    struct v4l2_requestbuffers req = {0};
    req.count = 1;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (xioctl(fd, VIDIOC_REQBUFS, &req) == -1)
    {
        perror("Failed to request buffer");
        close(fd);
        return 1;
    }

    // 4. Query the buffer to get its memory offset length
    struct v4l2_buffer buf = {0};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;

    if (xioctl(fd, VIDIOC_QUERYBUF, &buf) == -1)
    {
        perror("Failed to query buffer");
        close(fd);
        return 1;
    }

    // 5. Map the kernel buffer into our application's address space (mmap)
    void *buffer_start = mmap(
        NULL,
        buf.length,
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        fd,
        buf.m.offset);

    if (MAP_FAILED == buffer_start)
    {
        perror("Failed to map memory");
        close(fd);
        return 1;
    }

    // 6. Queue the buffer so the driver knows it's ready to be filled
    if (xioctl(fd, VIDIOC_QBUF, &buf) == -1)
    {
        perror("Failed to queue buffer");
        munmap(buffer_start, buf.length);
        close(fd);
        return 1;
    }

    // 7. Start the video stream
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(fd, VIDIOC_STREAMON, &type) == -1)
    {
        perror("Failed to start stream");
        munmap(buffer_start, buf.length);
        close(fd);
        return 1;
    }

    // 8. Dequeue the filled buffer (this blocks until a frame is ready)
    if (xioctl(fd, VIDIOC_DQBUF, &buf) == -1)
    {
        perror("Failed to dequeue buffer");
    }
    else
    {
        printf("Successfully captured a frame!\n");
        printf("Frame size in bytes: %u\n", buf.bytesused);

        // At this point, 'buffer_start' points directly to your raw in-memory byte array!
        // You can read 'buf.bytesused' bytes from 'buffer_start'.
        // Example: Let's dump it to a raw file to test.
        FILE *fp = fopen("frame_output.raw", "wb");
        if (fp)
        {
            fwrite(buffer_start, 1, buf.bytesused, fp);
            fclose(fp);
            printf("Saved frame to frame_output.raw\n");
        }
    }

    // 9. Stop streaming and clean up resources
    xioctl(fd, VIDIOC_STREAMOFF, &type);
    munmap(buffer_start, buf.length);
    close(fd);

    return 0;
}