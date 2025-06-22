#include <errno.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <termios.h>

#include "doomgeneric.h"
#include "doomkeys.h"

int fb, fd;
void *fb_mem;
struct fb_var_screeninfo vinfo = {};
struct termios oldt, newt;

void DG_Init() {
    fb = open("/dev/fb0", O_RDWR);
    if (fb == -1) {
        perror("failed to open framebuffer");
        exit(EXIT_FAILURE);
    }
    if (ioctl(fb, FBIOGET_VSCREENINFO, &vinfo) < 0) {
        perror("ioctl");
    }
    fb_mem = mmap(NULL, vinfo.xres * vinfo.yres * vinfo.bits_per_pixel / 8, PROT_READ | PROT_WRITE, MAP_SHARED, fb, 0);
    if (!fb_mem) {
        perror("failed to map framebuffer");
        exit(EXIT_FAILURE);
    }

    fd = open("/dev/input/event0", O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        perror("open /dev/input/event0");
        exit(EXIT_FAILURE);
    }
}

void DG_DrawFrame() {
    for (int y = 0; y < DOOMGENERIC_RESY; y++) {
        memcpy(fb_mem + y * vinfo.xres * 4, DG_ScreenBuffer + y * DOOMGENERIC_RESX, DOOMGENERIC_RESX * 4);
    }
}

void DG_SleepMs(uint32_t ms) {
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts, &ts);
}

#define KEYQUEUE_SIZE 16

static unsigned short s_KeyQueue[KEYQUEUE_SIZE];
static unsigned int s_KeyQueueWriteIndex = 0;
static unsigned int s_KeyQueueReadIndex = 0;

int DG_GetKey(int* pressed, unsigned char* doomKey) {
	if (s_KeyQueueReadIndex == s_KeyQueueWriteIndex) {
		return 0;
	} else {
		unsigned short keyData = s_KeyQueue[s_KeyQueueReadIndex];
		s_KeyQueueReadIndex++;
		s_KeyQueueReadIndex %= KEYQUEUE_SIZE;

		*pressed = keyData >> 8;
		*doomKey = keyData & 0xFF;
		return 1;
	}
}

static void addRawKeyToQueue(int pressed, unsigned char key) {
    unsigned short keyData = (pressed << 8) | key;

	s_KeyQueue[s_KeyQueueWriteIndex] = keyData;
	s_KeyQueueWriteIndex++;
	s_KeyQueueWriteIndex %= KEYQUEUE_SIZE;
}

static unsigned char convertToDoomKey(unsigned int key) {
	switch (key) {
        case KEY_W:
            return KEY_UPARROW;
        case KEY_A:
            return KEY_LEFTARROW;
        case KEY_S:
            return KEY_DOWNARROW;
        case KEY_D:
            return KEY_RIGHTARROW;
        case KEY_Q:
            return KEY_FIRE;
        case 28:
            return KEY_ENTER;
        case KEY_SPACE:
            return KEY_USE;
        case KEY_ESC:
            return KEY_ESCAPE;
        default:
            return key + 100;
	}
}

static void addKeyToQueue(int pressed, unsigned int keyCode) {
	unsigned char key = convertToDoomKey(keyCode);

	unsigned short keyData = (pressed << 8) | key;

	s_KeyQueue[s_KeyQueueWriteIndex] = keyData;
	s_KeyQueueWriteIndex++;
	s_KeyQueueWriteIndex %= KEYQUEUE_SIZE;
}

uint32_t DG_GetTicksMs() {
    struct timeval  tp;
    struct timezone tzp;

    gettimeofday(&tp, &tzp);

    return (tp.tv_sec * 1000) + (tp.tv_usec / 1000);
}

void DG_SetWindowTitle(const char *title) {
    (void)title;
}

int main(int argc, char **argv)
{
    doomgeneric_Create(argc, argv);

    for (int i = 0; ; i++) {
        doomgeneric_Tick();

        struct input_event ev;
        ssize_t n = read(fd, &ev, sizeof(ev));
        if (n > 0) {
            addKeyToQueue(ev.value, ev.code);
        }
    }

    tcsetattr(fd, TCSANOW, &oldt);
    close(fd);
    return 0;
}