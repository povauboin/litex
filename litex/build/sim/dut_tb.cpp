// This file is Copyright (c) 2015 Florent Kermarrec <florent@enjoy-digital.fr>
// License: BSD
#include "Vdut.h"
#include "verilated.h"
#include "verilated_vcd_c.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <sys/poll.h>

#include <linux/if.h>
#include <linux/if_tun.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

/* ios */

#ifdef SERIAL_SOURCE_VALID
#define WITH_SERIAL
#endif

#ifdef ETH_SOURCE_VALID
#define WITH_ETH
#endif

#ifdef VGA_DE
#define WITH_VGA
#endif

#define MAX(a,b) (((a)>(b))?(a):(b))
#define MIN(a,b) (((a)<(b))?(a):(b))

int trace = 0;

vluint64_t main_time = 0;
double sc_time_stamp()
{
	return main_time;
}

Vdut* dut;
VerilatedVcdC* tfp;

/* Sim struct */
struct sim {
	bool run;

	unsigned int tick;
	clock_t start;
	clock_t end;
	float speed;

#ifdef WITH_SERIAL_PTY
	char serial_dev[64];
	int serial_fd;
	unsigned char serial_rx_data;
	unsigned char serial_tx_data;
#endif
#ifdef WITH_ETH
	const char *eth_dev;
	const char *eth_tap;
	int eth_fd;
	unsigned char eth_txbuffer[2048];
	unsigned char eth_rxbuffer[2048];
	int eth_txbuffer_len;
	int eth_rxbuffer_len;
	int eth_rxbuffer_pos;
	int eth_last_source_valid;
#endif
};

struct sw {
	SDL_Rect box;
	unsigned int state;
	unsigned int mask;
};

/* Serial functions */
#if defined(WITH_SERIAL) && !defined(WITH_SERIAL_PTY)
struct termios orig_termios;

void reset_terminal_mode(void)
{
	tcsetattr(0, TCSANOW, &orig_termios);
}

void set_conio_terminal_mode(void)
{
	struct termios new_termios;

	/* take two copies - one for now, one for later */
	tcgetattr(0, &orig_termios);
	memcpy(&new_termios, &orig_termios, sizeof(new_termios));

	/* register cleanup handler, and set the new terminal mode */
	atexit(reset_terminal_mode);
	cfmakeraw(&new_termios);
	tcsetattr(0, TCSANOW, &new_termios);
}

int kbhit(void)
{
	struct timeval tv = { 0L, 0L };
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(0, &fds);
	return select(1, &fds, NULL, NULL, &tv);
}

int getch(void)
{
	int r;
	unsigned char c;
	if((r = read(0, &c, sizeof(c))) < 0) {
		return r;
	} else {
		return c;
	}
}
#endif

/* Ethernet functions */
/* create tap:
     openvpn --mktun --dev tap0
     ifconfig tap0 192.168.0.14 up
     mknod /dev/net/tap0 c 10 200
   delete tap:
     openvpn --rmtun --dev tap0 */
#ifdef WITH_ETH
void eth_init(struct sim *s, const char *dev, const char*tap)
{
	s->eth_txbuffer_len = 0;
	s->eth_rxbuffer_len = 0;
	s->eth_rxbuffer_pos = 0;
	s->eth_last_source_valid = 0;
	s->eth_dev = dev;
	s->eth_tap = tap;
}

void eth_open(struct sim *s)
{

	struct ifreq ifr;
	s->eth_fd = open (s->eth_dev, O_RDWR);
	if(s->eth_fd < 0) {
		fprintf(stderr, " Could not open dev %s\n", s->eth_dev);
		return;
	}

	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
	strncpy(ifr.ifr_name, s->eth_tap, IFNAMSIZ);

	if(ioctl(s->eth_fd, TUNSETIFF, (void *) &ifr) < 0) {
		fprintf(stderr, " Could not set %s\n", s->eth_tap);
		close(s->eth_fd);
	}
	return;
}

int eth_close(struct sim *s)
{
	if(s->eth_fd < 0)
		close(s->eth_fd);
}

void eth_write(struct sim *s, unsigned char *buf, int len)
{
	write(s->eth_fd, buf, len);
}

int eth_read(struct sim *s, unsigned char *buf)
{

	struct pollfd fds[1];
	int n;
	int len;

	fds[0].fd = s->eth_fd;
	fds[0].events = POLLIN;

	n = poll(fds, 1, 0);
	if((n > 0) && ((fds[0].revents & POLLIN) == POLLIN)) {
		len = read(s->eth_fd, buf, 1532);
	} else {
		len = 0;
	}
	return len;
}
#endif

/* VGA functions */
#ifdef WITH_VGA

SDL_Surface *screen;
SDL_Event event;

void vga_set_pixel(SDL_Surface *screen, int x, int y, char r, char g, char b)
{
	unsigned int *pixmem32;
	unsigned int color;

	color = SDL_MapRGB(screen->format, r, g, b);
	pixmem32 = (unsigned int*) screen->pixels  + y*640 + x;
	*pixmem32 = color;
}

int vga_init(struct sim *s) {
	if(SDL_Init(SDL_INIT_VIDEO) < 0) return 1;
	if(!(screen = SDL_SetVideoMode(640, 480+1, 32, SDL_HWSURFACE))) {
		SDL_Quit();
		return 1;
	}
	return 0;
}

int x;
int y;
int frame;
int hsync_wait_de = 1;
int vsync_wait_de = 1;

void vga_service(struct sim *s) {
	int i;
	if(VGA_HSYNC == 1 && hsync_wait_de == 0) {
		x = 0;
		y++;
		hsync_wait_de = 1;
	}
	if(VGA_VSYNC == 1 && vsync_wait_de == 0) {
		y = 0;
		vsync_wait_de = 1;
		for(i=0; i<frame; i++)
			vga_set_pixel(screen, i%640, 480, 255, 255, 255);
		frame++;
		if(SDL_MUSTLOCK(screen))
			SDL_UnlockSurface(screen);
		SDL_Flip(screen);
		if(SDL_MUSTLOCK(screen))
			SDL_LockSurface(screen);
	}
	if(VGA_DE == 1) {
		hsync_wait_de = 0;
		vsync_wait_de = 0;
		vga_set_pixel(screen, x, y, VGA_R, VGA_G, VGA_B);
		x++;
	}

	if(s->tick%1000 == 0) {
		while(SDL_PollEvent(&event)) {
			switch (event.type) {
				case SDL_QUIT:
					s->run = false;
					break;
				case SDL_KEYDOWN:
					s->run = false;
					break;
			}
		}
	}
}

int vga_close(struct sim *s) {
	SDL_Quit();
}

#endif


#if defined(WITH_SERIAL) && !defined(WITH_SERIAL_PTY)
int console_service(struct sim *s)
{
	/* fpga --> console */
	SERIAL_SOURCE_READY = 1;
	if(SERIAL_SOURCE_VALID == 1) {
		if(SERIAL_SOURCE_DATA == '\n')
			putchar('\r');
		putchar(SERIAL_SOURCE_DATA);
		fflush(stdout);
	}

	/* console --> fpga */
	SERIAL_SINK_VALID = 0;
	if(s->tick%(1000) == 0) {
		if(kbhit()) {
			char c = getch();
			if(c == 27 && !kbhit()) {
				printf("\r\n");
				return -1;
			} else {
				SERIAL_SINK_VALID = 1;
				SERIAL_SINK_DATA = c;
			}
		}
	}
	return 0;
}
#elif defined(WITH_SERIAL)
void console_init(struct sim *s)
{
	FILE *f;
	f = fopen("/tmp/simserial","r");
	fscanf(f, "%[^\n]", s->serial_dev);
	fclose(f);
	return;
}

int console_open(struct sim *s)
{
	int fdm;
	int rc;

	fdm = posix_openpt(O_RDWR);
	s->serial_fd = fdm;
	if (fdm < 0) {
		fprintf(stderr, "Error %d on posix_openpt()\n", errno);
		return 1;
	}

	rc = grantpt(fdm);
	if (rc != 0) {
		fprintf(stderr, "Error %d on grantpt()\n", errno);
		return 1;
	}

	rc = unlockpt(fdm);
	if (rc != 0) {
		fprintf(stderr, "Error %d on unlockpt()\n", errno);
		return 1;
	}

	printf("The slave side is named : %s\n", ptsname(fdm));
}

int console_close(struct sim *s)
{
	if(s->serial_fd < 0)
		close(s->serial_fd);
}

void console_write(struct sim *s, unsigned char *buf, int len)
{
	write(s->serial_fd, buf, len);
}

int console_read(struct sim *s, unsigned char *buf)
{
	struct pollfd fds[1];
	int n;
	int len;

	fds[0].fd = s->serial_fd;
	fds[0].events = POLLIN;

	n = poll(fds, 1, 0);
	if((n > 0) && ((fds[0].revents & POLLIN) == POLLIN)) {
		len = read(s->serial_fd, buf, 1);
	} else {
		len = 0;
	}
	return len;
}

int console_service(struct sim *s)
{
	/* fpga --> console */
	SERIAL_SOURCE_READY = 1;
	if(SERIAL_SOURCE_VALID == 1) {
		s->serial_tx_data = SERIAL_SOURCE_DATA;
		console_write(s, &(s->serial_tx_data), 1);
	}

	/* console --> fpga */
	SERIAL_SINK_VALID = 0;
	if(console_read(s, &(s->serial_rx_data)))
	{
		SERIAL_SINK_VALID = 1;
		SERIAL_SINK_DATA = s->serial_rx_data;
	}
	return 0;
}
#endif

#ifdef WITH_ETH
int ethernet_service(struct sim *s) {
	/* fpga --> tap */
	ETH_SOURCE_READY = 1;
	if(ETH_SOURCE_VALID == 1) {
		s->eth_txbuffer[s->eth_txbuffer_len] = ETH_SOURCE_DATA;
		s->eth_txbuffer_len++;
	} else {
		if(s->eth_last_source_valid) {
			eth_write(s, s->eth_txbuffer, s->eth_txbuffer_len);
			s->eth_txbuffer_len = 0;
		}
	}
	s->eth_last_source_valid = ETH_SOURCE_VALID;

	/* tap --> fpga */
	if(s->eth_rxbuffer_len == 0) {
		ETH_SINK_VALID = 0;
		s->eth_rxbuffer_pos = 0;
		s->eth_rxbuffer_len = eth_read(s, s->eth_rxbuffer);
	} else {
		if(s->eth_rxbuffer_pos < MAX(s->eth_rxbuffer_len, 60)) {
			ETH_SINK_VALID = 1;
			ETH_SINK_DATA = s->eth_rxbuffer[s->eth_rxbuffer_pos];
			s->eth_rxbuffer_pos++;
		} else {
			ETH_SINK_VALID = 0;
			s->eth_rxbuffer_len = 0;
			memset(s->eth_rxbuffer, 0, 1532);
		}
	}
}
#endif

void sim_tick(struct sim *s)
{
	SYS_CLK = s->tick%2;
	dut->eval();
	if(trace)
		tfp->dump(s->tick);
	s->tick++;
}

void sim_init(struct sim *s)
{
	int i;
	s->tick = 0;
#ifdef SYS_RST
	SYS_RST = 1;
	SYS_CLK = 0;
	for (i=0; i<8; i++)
		sim_tick(s);
	SYS_RST = 0;
#endif
	s->start = clock();
}

/* Swap int8 bit order */
unsigned char swap(unsigned char b) {
   b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
   b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
   b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
   return b;
}

int main(int argc, char **argv, char **env)
{
	float speed;

	SDL_Window* window = NULL;
	SDL_Renderer* renderer = NULL;
	SDL_Texture* img_board = NULL;
	SDL_Texture* img_digits = NULL;
	SDL_Texture* img_sw0 = NULL;
	SDL_Texture* img_sw1 = NULL;
	SDL_Event event;
	SDL_Rect rect;
	struct sw sws[8] = {
		{.box = {.x = 180, .y = 242, .w = 20, .h = 37}, .state = 0, .mask = 0x80},
		{.box = {.x = 200, .y = 242, .w = 20, .h = 37}, .state = 0, .mask = 0x40},
		{.box = {.x = 221, .y = 242, .w = 20, .h = 37}, .state = 0, .mask = 0x20},
		{.box = {.x = 242, .y = 242, .w = 20, .h = 37}, .state = 0, .mask = 0x10},
		{.box = {.x = 263, .y = 242, .w = 20, .h = 37}, .state = 0, .mask = 0x8},
		{.box = {.x = 284, .y = 242, .w = 20, .h = 37}, .state = 0, .mask = 0x4},
		{.box = {.x = 305, .y = 242, .w = 20, .h = 37}, .state = 0, .mask = 0x2},
		{.box = {.x = 326, .y = 242, .w = 20, .h = 37}, .state = 0, .mask = 0x1}
	};
	SDL_Rect segs[4] = {
		{.x = 216, .y = 193, .w = 32, .h = 42},
		{.x = 248, .y = 193, .w = 32, .h = 42},
		{.x = 280, .y = 193, .w = 32, .h = 42},
		{.x = 312, .y = 193, .w = 32, .h = 42}
	};
	SDL_Init( SDL_INIT_EVERYTHING );
	window = SDL_CreateWindow("Spartan3",
                          SDL_WINDOWPOS_UNDEFINED,
                          SDL_WINDOWPOS_UNDEFINED,
                          600, 426,
                          0);
	renderer = SDL_CreateRenderer(window, -1, 0);
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
	SDL_RenderSetLogicalSize(renderer, 400, 284);
	img_board = IMG_LoadTexture(renderer, "../spartan3.png" );
	img_digits = IMG_LoadTexture(renderer, "../spartan3_digits.png" );
	img_sw0 = IMG_LoadTexture(renderer, "../spartan3_sw0.png" );
	img_sw1 = IMG_LoadTexture(renderer, "../spartan3_sw1.png" );

#if defined(WITH_SERIAL) && !defined(WITH_SERIAL_PTY)
	set_conio_terminal_mode();
#endif

	Verilated::commandArgs(argc, argv);
	dut = new Vdut;

	Verilated::traceEverOn(true);
	tfp = new VerilatedVcdC;
	dut->trace(tfp, 99);
	tfp->open("dut.vcd");

	struct sim s;
	sim_init(&s);

#ifdef WITH_SERIAL_PTY
	// console_init(&s);
	console_open(&s);
#endif

#ifdef WITH_ETH
	eth_init(&s, "/dev/net/tap0", "tap0"); // XXX get this from /tmp/simethernet
	eth_open(&s);
#endif

#ifdef WITH_VGA
	if(vga_init(&s)) return 1;
#endif

	s.run = true;
	while(s.run) {
		sim_tick(&s);
		if(SYS_CLK) {
#ifdef WITH_SERIAL
			if(console_service(&s) != 0)
				s.run = false;
#endif
#ifdef WITH_ETH
			ethernet_service(&s);
#endif
#ifdef WITH_VGA
			vga_service(&s);
#endif
			if(s.tick%100000 == 0) {
				SDL_RenderClear(renderer);
				SDL_RenderCopy(renderer, img_board, NULL, NULL);
				while( SDL_PollEvent( &event ) ) {
					if( event.type == SDL_QUIT ) {
						s.run = false;
						break;
					}
					if( event.type == SDL_MOUSEBUTTONDOWN ) {
						if( event.button.button == SDL_BUTTON_LEFT ) {
							for	(int i=0; i<8; i++) {
								if( ( event.motion.x > sws[i].box.x ) && 
									( event.motion.x < sws[i].box.x + sws[i].box.w ) &&
									( event.motion.y > sws[i].box.y ) &&
									( event.motion.y < sws[i].box.y + sws[i].box.h ) ) {
										sws[i].state = !sws[i].state;
										SW = sws[i].state ? SW | sws[i].mask : SW & ~sws[i].mask;
								}
							}
						}
					}
				}
				for	(int i=0; i<8; i++) {
					SDL_RenderCopy(renderer, sws[i].state ? img_sw1 : img_sw0, NULL, &sws[i].box);
				}
				rect.x = ((swap(~SEGMENT) >> 1) & 0xf) * segs[0].w;
				rect.y = ((swap(~SEGMENT) >> 5) & 0xf) * segs[0].h;
				rect.w = segs[0].w;
				rect.h = segs[0].h;
				SDL_RenderCopy(renderer, img_digits, &rect, &segs[3]); // XXX
				SDL_RenderPresent( renderer );
			}
		}
	}
	s.end = clock();

	speed = (s.tick/2)/((s.end-s.start)/CLOCKS_PER_SEC);

	printf("average speed: %3.3f MHz\n\r", speed/1000000);

	tfp->close();

#ifdef WITH_SERIAL_PTY
	console_close(&s);
#endif
#ifdef WITH_ETH
	eth_close(&s);
#endif
#ifdef WITH_VGA
	vga_close(&s);
#endif

	SDL_Quit();

	exit(0);
}
