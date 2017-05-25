#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "error.h"
#include <unistd.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <event2/event.h>
#include <termios.h>

#include "modules.h"

struct session_s {
  char *tx;
  char *tx_valid;
  char *tx_ready;
  char *rx;
  char *rx_valid;
  char *rx_ready;
  char *sys_clk;
  struct event *ev;
  char databuf[2048];
  int data_start;
  int datalen;
};

struct event_base *base;
static int lambdasim_module_pads_get( struct pad_s *pads, char *name, void **signal)
{
  int ret;
  void *sig=NULL;
  int i;

  if(!pads || !name || !signal)
  {
    ret=RC_INVARG;
    goto out;
  }
  i = 0;
  while(pads[i].name)
  {
    if(!strcmp(pads[i].name, name))
    {
      sig=(void*)pads[i].signal;
      break;
    }
    i++;
  }

out:
  *signal=sig;
  return ret;
}



void set_conio_terminal_mode(void)
{
  struct termios new_termios;

  tcgetattr(0, &new_termios);
  cfmakeraw(&new_termios);
  tcsetattr(0, TCSANOW, &new_termios);
}



static int serial2console_start(void *b)
{
  base =(struct event_base *)b;
  printf("Loaded %p!\n", base);
  // set_conio_terminal_mode();
  return RC_OK;
}

void read_handler(int fd, short event, void *arg)
{
  struct session_s *s= (struct session_s*)arg;
  char buffer[1024];
  ssize_t read_len;
  
  int i;
  read_len = read(fd, buffer, 1024);
  for(i = 0; i < read_len; i++)
  {
    s->databuf[(s->data_start +  s->datalen ) % 2048] = buffer[i];
    s->datalen++;
  }
}

static void event_handler(int fd, short event, void *arg)
{
  //printf("hit\n");
  if (event & EV_READ)
  {
    read_handler(fd, event, arg);
  }
}


static int serial2console_new(void **sess, char *args)
{
  int ret=RC_OK;
  struct timeval tv = {1, 0};
  struct session_s *s=NULL;

  if(!sess)
  {
    ret = RC_INVARG;
    goto out;
  }
  
  s=(struct session_s*)malloc(sizeof(struct session_s));
  if(!s)
  {
    ret=RC_NOENMEM;
    goto out;
  }
  memset(s, 0, sizeof(struct session_s));
  s->ev = event_new(base, fileno(stdin), EV_READ | EV_PERSIST , event_handler, s);
  event_add(s->ev, &tv);
  
out:
  *sess=(void*)s;
  return ret;
  
}

static int serial2console_add_pads(void *sess, struct pad_list_s *plist)
{
  int ret=RC_OK;
  struct session_s *s=(struct session_s*)sess;
  struct pad_s *pads;
  if(!sess || !plist)
  {
    ret = RC_INVARG;
    goto out;
  }
  pads = plist->pads;
  if(!strcmp(plist->name, "serial"))
  {
    lambdasim_module_pads_get(pads, "sink_data", (void**)&s->rx);
    lambdasim_module_pads_get(pads, "sink_valid", (void**)&s->rx_valid);
    lambdasim_module_pads_get(pads, "sink_ready", (void**)&s->rx_ready);
    lambdasim_module_pads_get(pads, "source_data", (void**)&s->tx);
    lambdasim_module_pads_get(pads, "source_valid", (void**)&s->tx_valid);
    lambdasim_module_pads_get(pads, "source_ready", (void**)&s->tx_ready);
  }
  
  if(!strcmp(plist->name, "sys_clk"))  
  {
    lambdasim_module_pads_get(pads, "sys_clk", (void**)&s->sys_clk);

  }

out:
  return ret;
  
}
static int serial2console_tick(void *sess)
{

  struct session_s *s=(struct session_s*)sess;
  if(*s->sys_clk == 0)
  {
    return RC_OK;
  }
  *s->tx_ready = 1;
  if(*s->tx_valid)
  {
    if(*s->tx == '\n')
    {
      printf("\r");
    }
    printf("%c", *s->tx);
    fflush(stdout);
  }



  *s->rx_valid=0;
  if(s->datalen)
  {
    
    *s->rx=s->databuf[s->data_start];
    s->data_start = (s->data_start + 1) % 2048;
    s->datalen--;
    *s->rx_valid=1;
  }
  
  return RC_OK;
}

static struct ext_module_s ext_mod = {
  "serial2console",
  serial2console_start,
  serial2console_new,
  serial2console_add_pads,
  NULL,
  serial2console_tick
};

int lambdasim_ext_module_init(int (*register_module)(struct ext_module_s *))
{
  int ret=RC_OK;
  ret = register_module(&ext_mod);
  return ret;
}
