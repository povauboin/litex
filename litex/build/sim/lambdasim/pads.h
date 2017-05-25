#ifndef __PADS_H_
#define __PADS_H_

struct pad_s {
  char *name;
  size_t len;
  void *signal;
};

struct pad_list_s {
  char *name;
  struct pad_s *pads;
  int index;
  struct pad_list_s *next;
};

int lambdasim_pads_get_list(struct pad_list_s **plist);
int lambdasim_pads_find(struct pad_list_s *first, char *name, int index,  struct pad_list_s **found);
  
#ifdef __cplusplus
extern "C" int lambdasim_register_pads(struct pad_s *pads, char *interface_name, int index);
#else
int lambdasim_register_pads(struct pad_s *pads, char *interface_name, int index);
#endif

#endif
