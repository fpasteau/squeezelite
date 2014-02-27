#ifdef INTERACTIVE
#ifndef INTERACTIVE_H
#define INTERACTIVE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <signal.h>

#include <errno.h>
#include <ctype.h>
#include <netinet/in.h>
#ifndef __WIN32__
#include <netinet/tcp.h> 
#endif


#include <sys/types.h>
#include <sys/socket.h>

#include <sys/time.h>
#include <unistd.h>
#include <locale.h>
#include <ctype.h>
#include <curses.h>
#include <fcntl.h>
#include <lirc/lirc_client.h>
#define LINE_COUNT	2
typedef struct {
	struct lirc_config *lircconfig;
	int linelen;
	char *lircrc;
	bool using_lirc;
	int using_curses;
	bool use_lcdd_menu;
	bool lcdd_compat;
	struct sockaddr_in *lcd_addr;
	int lcd_fd;
	int lirc_fd;
	pthread_t interactive_t;
	bool finished;
} interactive_str;

void receive_display_data( unsigned short *data, int bytes_read);
void interactive_set_lcdd_ipaddress( char *ipaddy, int ipaddysize ) ;
void initcurses(void);
void close_lcd(void);
void close_lirc(void);
void init_lcd (void);
void init_lirc(void) ;
void exitcurses(void) ;
unsigned long getircode(int key);
int read_lirc(void);
bool read_lcd(void) ;
void init_interactive_str(interactive_str *i);
#ifndef INET_FQDNSTRLEN
#define INET_FQDNSTRLEN (256)
#endif

#endif
#endif
