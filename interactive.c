/*
 *   SlimProtoLib Copyright (c) 2004,2006 Richard Titmuss
 *
 *   This file is part of SlimProtoLib.
 *
 *   SlimProtoLib is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   SlimProtoLib is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with SlimProtoLib; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */


#ifdef INTERACTIVE
#include "interactive.h"
#include "squeezelite.h"

extern interactive_str interactive;

struct lirc_config *lircconfig;

/* For curses display */
SCREEN *term = NULL;
WINDOW *slimwin = NULL;
WINDOW *errwin = NULL;
WINDOW *msgwin = NULL;


/* For LCDd support */
char lcddserver_address[INET_FQDNSTRLEN] = "127.0.0.1";


void interactive_set_lcdd_ipaddress( char *ipaddy, int ipaddysize ) {
	strncpy(lcddserver_address, ipaddy, sizeof(lcddserver_address));
}

/* Close LCDd connection (if open) */
void close_lcd(void) {
   if (interactive.use_lcdd_menu) {
      free (interactive.lcd_addr);
      close (interactive.lcd_fd);
      interactive.use_lcdd_menu = false;
   }
}

/* Close lircd connection (if open) */
void close_lirc(void){
   if (interactive.using_lirc) {
      lirc_freeconfig(interactive.lircconfig);
      lirc_deinit();
      interactive.using_lirc = false;
   }
}

/* Set fd to non-blocking mode */
int setNonblocking(int fd) {
#ifdef __WIN32__
    int iretcode;
    unsigned long flags;

    iretcode = 0;
    flags = 1;

    if ( ioctlsocket( fd, FIONBIO, &flags ) == SOCKET_ERROR )
	    iretcode = -1;

    return (iretcode);
#else
    int flags;

    if (-1 == (flags = fcntl(fd, F_GETFL, 0)))
       flags = 0;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#endif
} 

/* Read response from LCDd and update our line length is one is supplied */
bool read_lcd(void) {
  char buf[1024];
  int res;
  long int num=interactive.linelen;
  char *pos;

  res = recv(interactive.lcd_fd, buf, 1024, MSG_DONTWAIT);
  if (res>9) {
     buf[res-1]=0; /* Null terminate before calling strstr */
     if ((pos= strstr(buf,"wid "))) {
        num=strtol(pos+3,NULL,10);
	if (num<100 && num>10)
	   interactive.linelen=num;
     }
  }
  return ((res > 0)?true:false);
}

/* Send data to lcdd */
void send_lcd(char* data, int len) {
  int sent = 0;
  int res;
  while(sent < len) {
     res = send(interactive.lcd_fd, data+sent, len-sent, 0);
     if (res == -1){
	fprintf(stderr,"LCD Send Failed\n");
	return;
     }
     sent += res;
   }
}

/* Try to open a connection to lircd if suppor is enabled
** if it fails, disable support, print a message and continue
*/
void init_lirc(void) {
   if (interactive.using_lirc) {
      interactive.using_lirc = false;
      if ((interactive.lirc_fd = lirc_init("squeezeslave",1)) > 0) {
         if (lirc_readconfig(interactive.lircrc, &interactive.lircconfig, NULL)==0){
            interactive.using_lirc = true;
            setNonblocking(interactive.lcd_fd);
         } else {
	    interactive.using_lirc = false;
            close_lirc();
	 }
      }
      if (!interactive.using_lirc ) fprintf(stderr, "Failed to init LIRC\n");
   }
}

/* Try to open a connection to LCDd if support is enabled
** If it succeeeds configure our screen
** If it fails, print a message, disable support and continue
*/
void init_lcd (void)
{
	if (!interactive.use_lcdd_menu) return;

	interactive.use_lcdd_menu = false;

	interactive.lcd_fd = socket(AF_INET, SOCK_STREAM, 0);

	if (interactive.lcd_fd > 0)
	{
		interactive.lcd_addr = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in *));
		interactive.lcd_addr->sin_family = AF_INET;

		if (inet_pton(AF_INET,lcddserver_address, (void *)(&(interactive.lcd_addr->sin_addr.s_addr))) >0)
		{
			interactive.lcd_addr->sin_port = htons(13666);
			if (connect(interactive.lcd_fd, (struct sockaddr *)interactive.lcd_addr, sizeof(struct sockaddr)) >= 0)
			{ 
				int flag = 1;
				if (setsockopt(interactive.lcd_fd, IPPROTO_TCP, TCP_NODELAY, (void*)&flag, sizeof(flag) ) == 0)
				{
					interactive.use_lcdd_menu = true;
					send_lcd("hello\n",6);

					while(!read_lcd());  /* wait for display info */

					send_lcd("client_set name {squeeze}\n",26);
					send_lcd("screen_add main\n",16);
					send_lcd("screen_set main name {main}\n",28);
					send_lcd("screen_set main heartbeat off\n",30);
					send_lcd("widget_add main one string\n",27);
					send_lcd("widget_add main two string\n",27);

					if ( interactive.lcdd_compat )		
						send_lcd("screen_set main -priority 256\n",30);
					else
						send_lcd("screen_set main -priority info\n",31);

					setNonblocking(interactive.lcd_fd);
				}
			}
		} 
	}

	/* If connect failed */
	if (!interactive.use_lcdd_menu)
	{
		interactive.use_lcdd_menu = true;
		close_lcd();
		fprintf(stderr,"Connect to LCDd Server on %s failed!\n", lcddserver_address);
	}
}

/* Called by our USR2 signal handler to toggle IR/LCD support on and off
*/
void toggle_handler(int signal_number) {
   if (interactive.use_lcdd_menu) {
      close_lcd();
      close_lirc();
   } else {
      interactive.use_lcdd_menu = true;
      interactive.using_lirc = true;
      init_lcd();
      init_lirc();
   }
}

/* Read a key code from lircd
*/
int read_lirc(void) {
  char *code;
  char *c;
  int key=0;

  if (lirc_nextcode(&code)==0) {
     if (code!=NULL) {
        while((lirc_code2char(interactive.lircconfig,code,&c)==0) && (c != NULL)) {
	   key = (unsigned char)c[0];
	}
	free(code);
	return key;
     }
     return -1;
  }
  return 0;
}

/* Set up a curses window for our display
*/
void initcurses(void) {
    if (!interactive.using_curses)
       return;
    term = newterm(NULL, stdout, stdin);
    if (term != NULL) {
	int screen_width, screen_height;
	int window_width = interactive.linelen+2;
	int window_height = LINE_COUNT+2;
	int org_x, org_y;

	interactive.using_curses = 1;

	cbreak();
	noecho();
	nonl();

	nodelay(stdscr, TRUE);
	intrflush(stdscr, FALSE);
	keypad(stdscr, TRUE);
	leaveok(stdscr, TRUE);
	curs_set(0);

	getmaxyx(stdscr, screen_height, screen_width);
	org_x = (screen_width - window_width) / 2;
        org_y = (screen_height - window_height - 1) / 2;

	slimwin = subwin(stdscr, window_height, window_width, org_y, org_x);
	box(slimwin, 0, 0);

	if (screen_height > LINE_COUNT+2) {
	    /* create one-line error message subwindow */
	    errwin = subwin(stdscr, 1, screen_width, org_y+window_height, 0);
	}

	wrefresh(curscr);
    } else {
	interactive.using_curses = 0;
    }
}

/* Shut down curses and put our terminal back to normal
*/
void exitcurses(void) {
    if (interactive.using_curses) {
       endwin();
       delscreen(term);
    }
}

/* Translate keys and lirc inputs to squeezecener IR codes
*/
unsigned long getircode(int key) {
    unsigned long ir = 0;
    
    switch(key) {
    case '0': ir = 0x76899867; break;
    case '1': ir = 0x7689f00f; break;
    case '2': ir = 0x768908f7; break;
    case '3': ir = 0x76898877; break;
    case '4': ir = 0x768948b7; break;
    case '5': ir = 0x7689c837; break;
    case '6': ir = 0x768928d7; break;
    case '7': ir = 0x7689a857; break;
    case '8': ir = 0x76896897; break;
    case '9': ir = 0x7689e817; break;
    case KEY_IC: ir = 0x7689609f; break; /* add */
    case 'I': ir = 0x7689609f; break; /* add */
    case 'i': ir = 0x7689609f; break; /* add */
    case 0x01: ir = 0x7689609f; break; /* add IR */
    case KEY_DOWN: ir = 0x7689b04f; break; /* arrow_down */
    case 'B': ir = 0x7689b04f; break; /* arrow_down  OSX 10.6.3 bug */
    case 0x02: ir = 0x7689b04f; break; /* arrow_down IR */
    case KEY_LEFT: ir = 0x7689906f; break; /* arrow_left */
    case 'D': ir = 0x7689906f; break; /* arrow_left OSX 10.6.3 bug */
    case 0x03: ir = 0x7689906f; break; /* arrow_left IR */
    case KEY_RIGHT: ir = 0x7689d02f; break; /* arrow_right */
    case 'C': ir = 0x7689d02f; break; /* arrow_right OSX 10.6.3 bug */
    case 0x04: ir = 0x7689d02f; break; /* arrow_right IR */
    case KEY_UP: ir = 0x7689e01f; break; /* arrow_up */
    case 'A': ir = 0x7689e01f; break; /* arrow_up OSX 10.6.3 bug */
    case 0x05: ir = 0x7689e01f; break; /* arrow_up IR*/
    case '<': ir = 0x7689c03f; break; /* rew */
    case ',': ir = 0x7689c03f; break; /* rew */
    case '>': ir = 0x7689a05f; break; /* fwd */
    case '.': ir = 0x7689a05f; break; /* fwd */
    case KEY_HOME: ir = 0x768922DD; break; /* home */
    case 'H': ir = 0x768922DD; break; /* home */
    case 'h': ir = 0x768922DD; break; /* home */
    case 0x06: ir = 0x768922DD; break; /* home IR*/
    case KEY_END: ir = 0x76897887; break; /* now_playing */
    case 'N': ir = 0x76897887; break; /* now_playing */
    case 'n': ir = 0x76897887; break; /* now_playing */
    case 0x07: ir = 0x76897887; break; /* now_playing IR*/
    case ' ': ir = 0x768920df; break; /* pause */
    case 'p': ir = 0x768920df; break; /* pause */
    case 'P': ir = 0x768920df; break; /* pause */
    case '\r': ir = 0x768910ef; break; /* play */
    case 'r': ir = 0x768938c7; break; /* repeat */
    case 'R': ir = 0x768938c7; break; /* repeat */
    case 's': ir = 0x7689d827; break; /* shuffle */
    case 'S': ir = 0x7689d827; break; /* shuffle */
    case '?': ir = 0x768958a7; break; /* search */
    case '/': ir = 0x768958a7; break; /* search */
    case 'b': ir = 0x7689708f; break; /* browse */
    case 'f': ir = 0x768918e7; break; /* favourites */
    case 'F': ir = 0x768918e7; break; /* favourites */
    case '%': ir = 0x7689f807; break; /* size */
    case 'z': ir = 0x7689b847; break; /* sleep */
    case 'Z': ir = 0x7689b847; break; /* sleep */
    case '-': ir = 0x768900ff; break; /* voldown */
    case '+': ir = 0x7689807f; break; /* volup */
    case '=': ir = 0x7689807f; break; /* volup */
    /* non-IR key actions */
    case '\f': wrefresh(curscr); break; /* repaint screen */
    case 'q': ir=0x01 ;/* quit */
    case 'Q': ir=0x01 ;/* quit */
#if 0
#if ! defined(__APPLE__) && ! defined(__MACH__)
    case '\e': ir=0x01 ;/* quit */
#endif
#endif
  }

  return (unsigned long)ir;
}

/* Send line to lcdd
*/
void set_lcd_line(int lineid, char *text, int len) {
   int total = 27 + len;
   char cmd[total];
   
   if (lineid == 1)
      memcpy(cmd,"widget_set main one 1 1 {",25);
   else
      memcpy(cmd,"widget_set main two 1 2 {",25);
   memcpy (cmd+25,text,len);
   memcpy(cmd+total-2,"}\n",2);
   send_lcd(cmd,total);
}

/* Change special LCD chars to something more printable on screen
*/
unsigned char printable(unsigned char c) {
   switch (c) {
      case 11:		/* block */
         return '#';
	 break;;
      case 16:		/* rightarrow */
         return '>';
	 break;;
      case 22:		/* circle */
         return '@';
	 break;;
      case 224:	
      case 226:	
         return 'a';
	 break;;
      case 232:	
      case 233:
      case 234:
      case 235:	
         return 'e';
	 break;;
      case 231:	
         return 'c';
	 break;;
      case 238:	
      case 239:
         return 'i';
	 break;;
      case 244:	
         return 'o';
	 break;;
      case 249:
      case 251:
      case 252:	
         return 'e';
	 break;;
      case 255:	
         return 'y';
	 break;;
      case 145:		/* note */
	 return ' ';
	 break;;
      case 152:		/* bell */
         return 'o';
	 break;;
      default:
         return c;
      }
}

/* Replace unprintable symbols in line
*/
void makeprintable(unsigned char * line) {
    int n;

    for (n=0;n<interactive.linelen;n++) 
       line[n]=printable(line[n]);
}

/* Show the display
*/
void show_display_buffer(char *ddram) {
    char line1[interactive.linelen+1];
    char *line2;

    memset(line1, 0, interactive.linelen+1);
    strncpy(line1, ddram, interactive.linelen);   
    line2 = &(ddram[interactive.linelen]);
    line2[interactive.linelen] = '\0';

    if (interactive.use_lcdd_menu) {
      set_lcd_line(1,line1,interactive.linelen);
      set_lcd_line(2,line2,interactive.linelen);
    }

    if (interactive.using_curses) {
      /* Convert special LCD chars */
      makeprintable((unsigned char *)line1);
      makeprintable((unsigned char *)line2);
      mvwaddnstr(slimwin, 1, 1, line1, interactive.linelen);
      mvwaddnstr(slimwin, 2, 1, line2, interactive.linelen);
      wrefresh(slimwin);
    }
}

/* Check if char is printable, or a valid symbol
*/
bool charisok(unsigned char c) {

   switch (c) {
      case 11:		/* block */
      case 16:		/* rightarrow */
      case 22:		/* circle */
      case 145:		/* note */
      case 152:		/* bell */
      case 224:	
      case 226:	
      case 232:	
      case 233:
      case 234:
      case 235:	
      case 231:	
      case 238:	
      case 239:
      case 244:	
      case 249:
      case 251:
      case 252:	
      case 255:	
         return true;
	 break;;
      default:
         return isprint(c);
   }
}

/* Process display data
*/
void receive_display_data( unsigned short *data, int bytes_read) {
    unsigned short *display_data;
    char ddram[interactive.linelen * 2];
    int n;
    int addr = 0; /* counter */

    if (bytes_read % 2) bytes_read--; /* even number of bytes */
    display_data = &(data[5]); /* display data starts at byte 12 */

    memset(ddram, ' ', interactive.linelen * 2);
    for (n=0; n<(bytes_read/2); n++) {
        unsigned short d; /* data element */
        unsigned char t, c;

        d = ntohs(display_data[n]);
        t = (d & 0x00ff00) >> 8; /* type of display data */
        c = (d & 0x0000ff); /* character/command */
        switch (t) {
            case 0x03: /* character */
                if (!charisok(c)) c = ' ';
                if (addr <= interactive.linelen * 2) {
                    ddram[addr++] = c;
		}
                break;
            case 0x02: /* command */
                switch (c) {
                    case 0x06: /* display clear */
                        memset(ddram, ' ', interactive.linelen * 2);
			break;
                    case 0x02: /* cursor home */
                        addr = 0;
                        break;
                    case 0xc0: /* cursor home2 */
                        addr = interactive.linelen;
                        break;
                }
    	}
    }
    show_display_buffer(ddram);
}

void init_interactive_str(interactive_str *i){
	i->linelen = 40;
	i->using_lirc = false;
	i->using_curses = 0;
	i->use_lcdd_menu = false;
	i->lcdd_compat = false;
}


#endif

