

#include <stdio.h>
#include <stdlib.h>

#include "../libopencore.hpp"
#include <stdarg.h>
#include <time.h>
#include  "ncurses.h"
#include <list>
#include <string>
#include <string.h>
using namespace std;

#define NORMAL "\033[0m"
#define BLINK "\033[5m"
#define BLUE "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN "\033[36m"
#define WHITE "\033[37m"
#define RED "\033[31m"
#define GREEN "\033[32m"
#define PINK "\033[35m\033[1m"
#define BROWN "\033[33m"
#define YELLOW "\033[33m\033[1m"
#define BOLDBLACK "\033[30m\033[1m"
#define BOLDGREEN "\033[32m\033[1m"
#define BOLDBLUE "\033[34m\033[1m"
#define BOLDMAGENTA "\033[35m\033[1m"
#define BOLDCYAN "\033[36m\033[1m"
#define BOLDWHITE "\033[37m\033[1m"
#define BOLDRED "\033[31m\033[1m"
#define BOLDPINK "\033[35m\033[1m"
#define BLINKBLUE "\033[34m\033[5m"
#define BLINKMAGENTA "\033[35m\033[5m"
#define BLINKCYAN "\033[36m\033[5m"
#define BLINKWHITE "\033[37m\033[5m"
#define BLINKRED "\033[31m\033[5m"
#define BLINKGREEN "\033[32m\033[5m"
#define BLINKBROWN "\033[33m\033[5m"
#define BLINKYELLOW "\033[33m\033[1m\033[5m"
#define BLINKBBLACK "\033[30m\033[1m\033[5m"
#define BLINKBGREEN "\033[32m\033[1m\033[5m"
#define BLINKBBLUE "\033[34m\033[1m\033[5m"
#define BLINKBMAGENTA "\033[35m\033[1m\033[5m"
#define BLINKBCYAN "\033[36m\033[1m\033[5m"
#define BLINKBWHITE "\033[37m\033[1m\033[5m"
#define BLINKBRED "\033[31m\033[1m\033[5m"
#define BGBLUE "\033[44m"
#define BGBLACK "\033[40m"
#define BGRED "\033[41m"
#define BGGREEN "\033[42m"
#define BGYELLOW "\033[43m"
#define BGMAGENTA "\033[45m"
#define BGCYAN "\033[46m"
#define BGWHITE "\033[47m"
#include <signal.h>

#ifdef __cplusplus
extern "C" {
#endif




//ncurses code adapted from ntalk.cpp http://freshmeat.net/projects/ntalk/
int term_resized=0;
int curr_h;
int curr_w;
void resizeHandler(int sig)
{
	term_resized =1; //poll this var in event_tick and resize and reset var
}


int term_quit_signaled=0;
void QuitHandler(int sig)
{
	term_quit_signaled=1; //poll this var in event_tick and stop bot if set
}




enum NC_COLOR
{
	NC_NONE=0,
	NC_NORMAL,
	NC_GREEN,
	NC_RED,
	NC_CYAN,
	NC_YELLOW,
	NC_MAGENTA
};


class NC_LINE_SEGMENT
{
friend class NC_LINE;
public:
	NC_LINE_SEGMENT()
	{	
		//do nothing
	}	
	NC_LINE_SEGMENT(NC_COLOR color, char msg[256])
	{
		strncpy(txt,msg,sizeof(txt));
		c= color;	
	}
	NC_LINE_SEGMENT& operator=(const NC_LINE_SEGMENT &rhs)
	{
		strncpy(txt,rhs.txt,sizeof(txt));
		c= rhs.c;
		return *this;
	}
	NC_LINE_SEGMENT(const NC_LINE_SEGMENT &rhs)
	{
		*this = rhs;

	}
private:
	NC_COLOR c;
	char txt[256];
};
	
class NC_LINE
{
public:	
	NC_LINE()
	{

	}

	void AddToLine(NC_COLOR color, char *fmt,...)
	{
		va_list(ap);
		char buff[256];
		va_start(ap, fmt);
		vsnprintf(buff, sizeof(buff), fmt, ap);
		va_end(ap);
		sl.push_back(NC_LINE_SEGMENT(color,buff));
	}
	
	void printw(WINDOW *w)
	{
		list<NC_LINE_SEGMENT>::iterator i =sl.begin();
		list<NC_LINE_SEGMENT>::iterator j = sl.end();
		while(i != j)
		{
			if(i->c != NC_NONE) 
				wattron(w, COLOR_PAIR((int)i->c));
			wprintw(w,"%s",i->txt); //this was a bug (before there was no %s)
			i++;
			
		}
		wattron(w, COLOR_PAIR((int)NC_NORMAL));
		wnoutrefresh(w);
    		doupdate();
	}
private:
	list<NC_LINE_SEGMENT> sl;
};

class NC_HISTORY
{
public:
	NC_HISTORY()
	{
		MAX_HIST=1024;
		
	}
	NC_HISTORY(unsigned int mh)
	{
		MAX_HIST = mh;
	}
	void printw(WINDOW *w)
	{
		list<NC_LINE>::iterator i = nl.begin();
		list<NC_LINE>::iterator j = nl.end();
		wclear(w);
		while(i != j)
		{
			i->printw(w);
			i++;

		}
	
	}
	void AddLine(WINDOW *w,NC_LINE &k)
	{
		if(nl.size() > MAX_HIST)
			nl.pop_front();
		k.printw(w);
		nl.push_back(k);
		
	}
private:
	unsigned int MAX_HIST;
	list<NC_LINE> nl; 
};


/*
 * Since modules are reentrant, global variables are accessible to
 * any thread.  user_data is a thread-specific storage area for
 * values that are global to a thread.
 *
 * This should be allocated in EVENT_START and freed in EVENT_STOP.
 */
typedef struct user_data USER_DATA;
struct user_data
{
	//int running;// controls loop in input thread;
	WINDOW *input_w, *main_w, *sts_w, *count_w, *user_w;
	
	NC_HISTORY hist;
	string input;
	
	char last_priv[25];
	FILE *log;
	
};

void GameEvent(CORE_DATA *cd);//forward declaration

LIB_DATA REGISTRATION_INFO = {
	"Chat",
	"The Junky",
	"1.32",
	__DATE__,
	__TIME__,
	"ChatBot ncurses chat client",
	CORE_VERSION,
	0,
	GameEvent
};


void update_all(USER_DATA *ud)
{
    wnoutrefresh(ud->main_w);
    wnoutrefresh(ud->input_w);
    wnoutrefresh(ud->sts_w);
    wnoutrefresh(ud->count_w);
    wclear(ud->input_w);
    doupdate();
}

void update_main(USER_DATA *ud)
{
    wnoutrefresh(ud->main_w);
    doupdate();
    wrefresh(ud->input_w);
}

void MakeWindows(USER_DATA *ud, int lines, int cols)
{
    ud->main_w = newwin(lines - 2, cols  - 21, 1, 0);
    ud->sts_w = newwin(1, cols  - 21, 0, 0);
    ud->count_w = newwin(1, 20, 0, cols  - 20);
    ud->input_w = newwin(1, cols, lines - 1, 0);
    ud->user_w = newwin(LINES - 2, 20, 1, cols  - 20);
    scrollok(ud->main_w, TRUE);
    keypad(ud->input_w, TRUE);
    scrollok(ud->input_w, TRUE);
    nodelay(ud->input_w, TRUE);
    cbreak();		
  }

void gui_init(CORE_DATA *cd,USER_DATA *ud)
{	
    initscr();
    nodelay(stdscr, TRUE);	
    start_color();   
    init_pair(NC_GREEN, COLOR_GREEN, COLOR_BLACK);
    init_pair(NC_RED, COLOR_RED, COLOR_BLACK);
    init_pair(NC_CYAN, COLOR_CYAN, COLOR_BLACK);
    init_pair(NC_YELLOW, COLOR_YELLOW, COLOR_BLACK);
    init_pair(NC_NORMAL, COLOR_WHITE, COLOR_BLACK);
    MakeWindows(ud,LINES,COLS);
    curr_h = LINES;
    curr_w = COLS;	
    echo();
    update_all(ud);

}

void RefreshStatusWindow(CORE_DATA *cd,USER_DATA *ud)
{

    wclear(ud->sts_w);
    wrefresh(ud->sts_w);	
    wattron(ud->sts_w, COLOR_PAIR((int)NC_CYAN));
    wprintw(ud->sts_w,"Logged in:");
    wattron(ud->sts_w, COLOR_PAIR((int)NC_RED));
    wprintw(ud->sts_w," Arena:[%s]",cd->bot_arena);
    wrefresh(ud->sts_w);
}


void RefreshUserList(CORE_DATA *cd,USER_DATA *ud)
{
    int n=0;	
    PLAYER *p = NULL;
    wclear(ud->user_w);
    wrefresh(ud->user_w);
    wattron(ud->user_w, COLOR_PAIR((int)NC_CYAN));
    for(int i=0; i < cd->phere; i++)
    {
	p = &(cd->parray[i]);
	if(p->in_arena)
	{
		wprintw(ud->user_w,"%s\n",p->name);
		n++;
	}
    }
    wrefresh(ud->user_w);
    wclear(ud->count_w);
    wprintw(ud->count_w,"PlayerCount: %i",n); 		
    wrefresh(ud->count_w);	
	
    
}




void wColoredOutput(USER_DATA *ud,NC_COLOR nc,char *name,NC_COLOR mc,char *msg,...)
{
	va_list(ap);
	va_start(ap, msg);
	char buf[256];
	vsnprintf(buf, 256, msg, ap);
	va_end(ap);
	time_t now = time(NULL);
	char time_str[32];
	NC_LINE nl;
	strftime(time_str,sizeof(time_str),"%m/%d %H:%M",localtime(&now));
	nl.AddToLine(NC_NONE,"[%s] ",time_str);
	if(name)
	{
		nl.AddToLine(nc,"%s> ",name);

	}
	nl.AddToLine(mc,"%s\n",buf);
	ud->hist.AddLine(ud->main_w,nl);//this will print to screen aswell
	update_main(ud);
}
void DelWindows(USER_DATA *ud)
{
	delwin(ud->main_w); ud->main_w=NULL;
    	delwin(ud->sts_w);ud->sts_w=NULL;
    	delwin(ud->count_w);ud->count_w = NULL;
    	delwin(ud->input_w); ud->input_w = NULL;
    	delwin(ud->user_w); ud->user_w = NULL;
}
void ResizeWindows(CORE_DATA *cd,USER_DATA *ud)
{
	int h,w;
	getmaxyx(stdscr,h,w);
	DelWindows(ud);
    	MakeWindows(ud,h,w);
	curr_h = h;
	curr_w = w;
	ud->hist.printw(ud->main_w);
	RefreshUserList(cd,ud);
	RefreshStatusWindow(cd,ud);
	update_main(ud);
    	update_all(ud);
}


const char *
NC_COLOR_TO_CONSOLE_COLOR_STR(NC_COLOR c)
{
	switch(c)
	{
	case NC_NONE: return NULL;
	case NC_NORMAL: return NORMAL;
	case NC_GREEN: return GREEN;
	case NC_RED: return RED;
	case NC_CYAN: return CYAN;
	case NC_YELLOW: return YELLOW;
	case NC_MAGENTA: return MAGENTA;
	}
	return NULL;
} 


void ColoredLog(USER_DATA *ud,NC_COLOR nc,char *name,NC_COLOR mc,char *msg,...)
{
	if(ud->log)
	{
		va_list(ap);
		va_start(ap, msg);
		char buf[256];
		vsnprintf(buf, 256, msg, ap);
		va_end(ap);
		const char *name_color = NC_COLOR_TO_CONSOLE_COLOR_STR(nc);
		const char *msg_color =  NC_COLOR_TO_CONSOLE_COLOR_STR(mc);
		time_t now = time(NULL);
		char time_str[32];
		strftime(time_str,sizeof(time_str),"%m/%d/%y %H:%M",localtime(&now));	
		fprintf(ud->log,"[%s] ",time_str);
		if(name)
		{
			fprintf(ud->log,"%s%s> ",name_color,name);

		}
		fprintf(ud->log,"%s%s%s%s\n",msg_color,buf,msg_color,NORMAL);
		fflush(ud->log);
	}



}
/*
void ColoredConsole(char *name_color,char *name,char *msg_color,char *msg,...)
{
	va_list(ap);
	va_start(ap, msg);
	char buf[256];
	vsnprintf(buf, 256, msg, ap);
	va_end(ap);
	time_t now = time(NULL);
	char time_str[32];
	strftime(time_str,sizeof(time_str),"%m/%d/%y %H:%M",localtime(&now));	
	printf("[%s] ",time_str);
	if(name)
	{
		printf("%s%s> ",name_color,name);

	}
	printf("%s%s%s%s\n",msg_color,buf,msg_color,NORMAL);

}
*/

void HandleMsgOutput(CORE_DATA *cd,char msg_name[25],char msg[256],MSG_TYPE msg_type,uint8_t msg_chatnum)
{
	char name[32];
	USER_DATA *ud = (USER_DATA *) cd->user_data;
	switch(msg_type)
	{	
	case MSG_PUBLIC: 
		wColoredOutput(ud,NC_CYAN,msg_name,NC_CYAN, "%s", msg);
		ColoredLog(ud,NC_CYAN,msg_name,NC_CYAN, "%s", msg);
		break;
	case MSG_PRIVATE: 
		wColoredOutput(ud,NC_GREEN,msg_name,NC_GREEN, "%s", msg);
		ColoredLog(ud,NC_GREEN,msg_name,NC_GREEN, "%s", msg);
		break;
	case MSG_REMOTE: 
		sprintf(name,"(%s)",msg_name);
		wColoredOutput(ud,NC_GREEN,name,NC_GREEN, "%s", msg);
		ColoredLog(ud,NC_GREEN,name,NC_GREEN, "%s", msg);
		break;
	case MSG_ARENA: 
		wColoredOutput(ud,NC_NONE,0,NC_GREEN,"%s", msg);
		ColoredLog(ud,NC_NONE,0,NC_GREEN,"%s", msg);
		break;
	case MSG_CHAT:
		sprintf(name,"%i:%s",msg_chatnum,msg_name);
		wColoredOutput(ud,NC_RED,name,NC_RED,"%s", msg);
		ColoredLog(ud,NC_RED,name,NC_RED,"%s", msg);
		break;
	case MSG_SYSOP:
		wColoredOutput(ud,NC_NONE,0,NC_RED,"%s", msg);
		ColoredLog(ud,NC_RED,0,NC_RED,"%s", msg);
		break;
	case MSG_TEAM:
		wColoredOutput(ud,NC_YELLOW,msg_name,NC_YELLOW,"%s", msg);
		ColoredLog(ud,NC_YELLOW,msg_name,NC_YELLOW,"%s", msg);
		break;
	case MSG_FREQ:
		wColoredOutput(ud,NC_GREEN,msg_name,NC_CYAN, "%s", msg);
		ColoredLog(ud,NC_GREEN,msg_name,NC_CYAN, "%s", msg);
		break;
	default:
		break;
	}
}


void ParseConsoleInput(CORE_DATA *cd, string input)
{
	USER_DATA *ud = (USER_DATA *) cd->user_data;
	char buff[256];
	strncpy(buff,input.c_str(),256);
	buff[255]=0;
	char buff2[64];
	char *tp,*tp2;
	 

	switch(buff[0])
   	{
	case '\n':
	case '\0':
		break;
	case ':'://priv/remote msg
		if(buff[1] == ':')
		{	
			tp = buff+2;
  			RmtMessage(ud->last_priv,tp);
			sprintf(buff2,"%s To %s",cd->bot_name,ud->last_priv);
			HandleMsgOutput(cd,buff2,tp,MSG_PRIVATE,0);
	
		}
		else
		{
			tp = buff+1;
			tp2 = strchr(tp,':');
			if(tp2)
			{
				*tp2=0;
				tp2++;
				if(strlen(tp) < 25 && strlen(tp2) < 256)
				{
					RmtMessage(tp,tp2);
					strcpy(ud->last_priv,tp);
					sprintf(buff2,"%s To %s",cd->bot_name,tp);
					HandleMsgOutput(cd,buff2,tp2,MSG_PRIVATE,0);
				}
			}
		}	
		break;
	case ';'://chat
		if((buff[1] >= '1' && buff[1] <= '9') && buff[2] == ';' )
		{		
			ChatMessage(ChatName(buff[1]-'0'),buff+3);
			HandleMsgOutput(cd,cd->bot_name,buff+3,MSG_CHAT,atoi(buff+1));
		}
		else if(!(buff[1] >= '1' && buff[1] <= '9'))
		{
			ChatMessage(ChatName(1),buff+1);
			HandleMsgOutput(cd,cd->bot_name,buff+1,MSG_CHAT,1);
		}
		break;
	case '/'://team
	if(buff[1] =='/')
	{
		TeamMessage(buff+2);
		HandleMsgOutput(cd,cd->bot_name,buff+2,MSG_TEAM,0);
	}	
		break;
	case '\''://team
		TeamMessage(buff+1);
		HandleMsgOutput(cd,cd->bot_name,buff+1,MSG_TEAM,0);
	
		break;
	//case '!'://internal command
	//	break;
	case '?':
		if (strncasecmp(buff, "?go ", 4) == 0) {
			Go(&buff[4]);
		} else if (strcasecmp(buff, "?quit") == 0) {
			StopBot("Exiting");
		} else if (strcasecmp(buff, "?mark") == 0 || strcasecmp(buff, "?m") == 0) {
			wColoredOutput(ud,NC_RED,NULL,NC_RED,"%s", "=== MARK ====================================================================");
			ColoredLog(ud,NC_RED,NULL,NC_RED,"%s", "=== MARK ====================================================================");
			// FIXME: this should probably fall through below to be consistent with other behavior,
			// and its kind of bad that HandleMsgOutput's functionality is repeated here (2 lines above).
			// however, that function doesnt account for magenta because it only takes in MSG_Xxx constants
			// that are defined in the opencore header, and we add a constant for magenta
			return;
		}
		/* FALLTHROUGH */
	default:
		PubMessage(buff);
		HandleMsgOutput(cd,cd->bot_name,buff,MSG_PUBLIC,0);
		break;
	}
}

void Handle_Input(CORE_DATA *cd)
{
    	USER_DATA *ud = (USER_DATA *)cd->user_data;
    	int ch= 0; 
	//while((ch= wgetch(ud->input_w))  != ERR)
	//{
		switch (ch = wgetch(ud->input_w)) 
		{
			case ERR:
			case KEY_DOWN://        The four arrow keys ...
			case KEY_UP:
			case KEY_LEFT:
			case KEY_RIGHT:
           		case KEY_HOME://        Home key 
	   		case KEY_END: 		
   		        //case KEY_F(n)        Function keys, for 0 <= n >= 63
           		case KEY_DC://          Delete character
           		case KEY_IC://          Insert char or enter insert mode
           		case KEY_ENTER://       Enter or send
			//no char waiting... ignore
				break;	
			case KEY_BACKSPACE://   Backspace
				if(ud->input.size())
				{
					ud->input.erase(ud->input.size() - 1, 1);
					wclear(ud->input_w);
					waddstr(ud->input_w,ud->input.c_str());
   					wrefresh(ud->input_w); 
				}
				break;
	
          		case '\n':
    				wclear(ud->input_w);
   				wrefresh(ud->input_w); 
   				ParseConsoleInput(cd,ud->input);
				ud->input.clear();	
				break;
        		  default:
				if(ud->input.size() < 254)
					ud->input.append(1,ch);
				//else ignore
				break;
     		}
	//}	
}

void GameEvent(CORE_DATA *cd)
{
	USER_DATA *ud = (USER_DATA *) cd->user_data;
	time_t now;
	char buff[256];

	switch (cd->event) {
	case EVENT_START:
		/* set the bot's thread-specific user_data */
		cd->user_data = ud = new USER_DATA;
		
		gui_init(cd,ud);
	    	signal(SIGWINCH, resizeHandler);
		signal(SIGINT, QuitHandler);
		signal(SIGQUIT, QuitHandler);
		signal(SIGABRT, QuitHandler);
	
		RefreshStatusWindow(cd,ud);
		now =time(NULL);
		strftime(buff,sizeof(buff),"./logs/chat-%m-%y.log",localtime(&now));
		ud->log = fopen(buff,"a+");
		break;

	case EVENT_MESSAGE:
		switch(cd->msg_type)
		{
		case MSG_PRIVATE:
		case MSG_REMOTE:
			strcpy(ud->last_priv,cd->msg_name);
		default:
			HandleMsgOutput(cd,cd->msg_name,cd->msg,cd->msg_type,cd->msg_chatid);
			break;
		}
		break;
	case EVENT_ARENA_LIST:
		for (int i = 0; i < cd->arena_list_count; ++i) {
			wColoredOutput(ud,NC_NONE,0,NC_GREEN,"%s: %d",
				cd->arena_list[i].name, cd->arena_list[i].count);
			ColoredLog(ud,NC_NONE,0,NC_GREEN,"%s: %d",
				cd->arena_list[i].name, cd->arena_list[i].count);
		}
		break;
	case EVENT_ALERT:
		wColoredOutput(ud,NC_NONE,0,NC_GREEN,"%s: (%s) (%s): %s",
			cd->alert_type,cd->alert_name,cd->alert_arena,cd->alert_msg);
		ColoredLog(ud,NC_NONE,0,NC_GREEN,"%s: (%s) (%s): %s",
			cd->alert_type,cd->alert_name,cd->alert_arena,cd->alert_msg);
		break;
	case EVENT_ENTER:
		RefreshUserList(cd,ud);
		break;
	case EVENT_LEAVE:
		RefreshUserList(cd,ud);
		break;
	case EVENT_TICK:
		Handle_Input(cd);
		if(term_resized)
		{
			term_resized = 0;
			ResizeWindows(cd,ud);
		}
		if(term_quit_signaled)
		{
			term_quit_signaled = 0;
			StopBot("Termination signal recieved from Console");
		}	
		break;
	case EVENT_LOGIN:
		RefreshStatusWindow(cd,ud);
		PubMessage("?chat");
		break;
	case EVENT_STOP:
		if(cd->user_data)
		{
			DelWindows(ud);
			endwin();
			if(ud->log) fclose(ud->log);		
			delete ud;	
		}
		break;
	}
}

#ifdef __cplusplus
}
#endif

