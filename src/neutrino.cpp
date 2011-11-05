/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
							 and some other guys
	Homepage: http://dbox.cyberphoria.org/

	Kommentar:

	Diese GUI wurde von Grund auf neu programmiert und sollte nun vom
	Aufbau und auch den Ausbaumoeglichkeiten gut aussehen. Neutrino basiert
	auf der Client-Server Idee, diese GUI ist also von der direkten DBox-
	Steuerung getrennt. Diese wird dann von Daemons uebernommen.


	License: GPL

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define NEUTRINO_CPP

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <dlfcn.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/statvfs.h>
#include <sys/vfs.h>

#include <sys/socket.h>

#include <iostream>
#include <fstream>
#include <string>


#include "global.h"
#include "neutrino.h"

#include <daemonc/remotecontrol.h>

#include <driver/encoding.h>
#include <driver/framebuffer.h>
#include <driver/fontrenderer.h>
#include <driver/rcinput.h>
#include <driver/stream2file.h>
#include <driver/vcrcontrol.h>
#include <driver/shutdown_count.h>

#include <gui/epgplus.h>
#include <gui/streaminfo2.h>

#include "gui/widget/colorchooser.h"
#include "gui/widget/menue.h"
#include "gui/widget/messagebox.h"
#include "gui/widget/hintbox.h"
#include "gui/widget/icons.h"
#include "gui/widget/lcdcontroler.h"
#include "gui/widget/vfdcontroler.h"
#include "gui/widget/rgbcsynccontroler.h"
#include "gui/widget/keychooser.h"
#include "gui/widget/stringinput.h"
#include "gui/widget/stringinput_ext.h"
#include "gui/widget/mountchooser.h"

#include "gui/color.h"
#include "gui/customcolor.h"

#include "gui/bedit/bouqueteditor_bouquets.h"
#include "gui/bouquetlist.h"
#include "gui/eventlist.h"
#include "gui/channellist.h"
#include "gui/screensetup.h"
#include "gui/pluginlist.h"
#include "gui/plugins.h"
#include "gui/infoviewer.h"
#include "gui/epgview.h"
#include "gui/epg_menu.h"
#include "gui/update.h"
#include "gui/scan.h"
#include "gui/favorites.h"
#include "gui/sleeptimer.h"
#include "gui/rc_lock.h"
#include "gui/timerlist.h"
#include "gui/alphasetup.h"
#include "gui/audioplayer.h"
#include "gui/imageinfo.h"
#include "gui/movieplayer.h"
#include "gui/nfs.h"
#include "gui/pictureviewer.h"
#include "gui/motorcontrol.h"
#include "gui/filebrowser.h"
#include "gui/scale.h"
#include "gui/cam_menu.h"
#include "gui/hdd_menu.h"
#include "gui/upnpbrowser.h"

#include <system/setting_helpers.h>
#include <system/settings.h>
#include <system/debug.h>
#include <system/flashtool.h>
#include <system/fsmounter.h>

#include <timerdclient/timerdmsg.h>
#include <zapit/satconfig.h>

#include <init_cs.h>
#include <video_cs.h>
#include <audio_cs.h>
#include <zapit/frontend_c.h>
#include <pwrmngr.h>

#include <string.h>
#include <linux/reboot.h>
#include <sys/reboot.h>

int old_b_id = -1;
CHintBox * reloadhintBox = 0;
uint32_t sec_timer;
int rf_dummy;
bool has_hdd;
#include "gui/infoclock.h"
#include "gui/dboxinfo.h"
CInfoClock      *InfoClock;
int allow_flash = 1;
extern int was_record;
extern char current_timezone[50];
Zapit_config zapitCfg;
char zapit_lat[20];
char zapit_long[20];
bool autoshift = false;
bool autoshift_delete = false;
uint32_t shift_timer;
uint32_t scrambled_timer;
char recDir[255];
char timeshiftDir[255];

extern int  tuxtxt_init();
extern void tuxtxt_start(int tpid);
extern int  tuxtxt_stop();
extern void tuxtxt_close();

int dvbsub_init();
int dvbsub_stop();
int dvbsub_close();
int dvbsub_start(int pid);
int dvbsub_pause();

//char current_volume;
extern int list_changed;

static CScale * g_volscale;
//NEW
static pthread_t timer_thread;
void * timerd_main_thread(void *data);

extern int streamts_stop;
void * streamts_main_thread(void *data);
static pthread_t stream_thread ;

extern int zapit_ready;
static pthread_t zapit_thread ;
void * zapit_main_thread(void *data);
extern t_channel_id live_channel_id; //zapit
void setZapitConfig(Zapit_config * Cfg);
void getZapitConfig(Zapit_config *Cfg);

void * nhttpd_main_thread(void *data);
static pthread_t nhttpd_thread ;

//#define DISABLE_SECTIONSD //FIXME
extern int sectionsd_stop;
static pthread_t sections_thread;
void * sectionsd_main_thread(void *data);
//extern int sectionsd_scanning; // sectionsd.cpp
extern bool timeset; // sectionsd

extern cVideo * videoDecoder;
extern cAudio * audioDecoder;
cPowerManager *powerManager;
cCpuFreqManager * cpuFreq;

int prev_video_mode;
int prev_asp_mode;

int xres = 72;
int yres = 72; // TODO: no globals for that.

void stop_daemons(bool stopall = true);
// uncomment if you want to have a "test" menue entry  (rasc)

#if HAVE_DVB_API_VERSION >= 3
//#define __EXPERIMENTAL_CODE__
#endif

#ifdef __EXPERIMENTAL_CODE__
#include "gui/ch_mosaic.h"
#endif

#include "gui/audio_select.h"

CAPIDChangeExec		* APIDChanger;
CAudioSetupNotifier	* audioSetupNotifier;
CBouquetList   * bouquetList; // current list

CBouquetList   * TVbouquetList;
CBouquetList   * TVsatList;
CBouquetList   * TVfavList;
CBouquetList   * TVallList;

CBouquetList   * RADIObouquetList;
CBouquetList   * RADIOsatList;
CBouquetList   * RADIOfavList;
CBouquetList   * RADIOallList;

CPlugins       * g_PluginList;
CRemoteControl * g_RemoteControl;
SMSKeyInput * c_SMSKeyInput;
CMoviePlayerGui* moviePlayerGui;
CAudioSelectMenuHandler  *audio_menu;
CPictureViewer * g_PicViewer;
CCAMMenuHandler * g_CamHandler;

//int g_ChannelMode = LIST_MODE_PROV;

// Globale Variablen - to use import global.h

// I don't like globals, I would have hidden them in classes,
// but if you wanna do it so... ;)
bool parentallocked = false;
//static bool waitforshutdown = false;
static char **global_argv;

//static CTimingSettingsNotifier timingsettingsnotifier;
CFontSizeNotifier fontsizenotifier;
CFanControlNotifier * funNotifier;

extern const char * locale_real_names[]; /* #include <system/locals_intern.h> */
// USERMENU
const char* usermenu_button_def[SNeutrinoSettings::BUTTON_MAX]={"red","green","yellow","blue"};

CZapitClient::SatelliteList satList;

CVCRControl::CDevice * recordingdevice = NULL;

#if 0
#define NEUTRINO_SETTINGS_FILE          CONFIGDIR "/neutrino.conf"

#define NEUTRINO_RECORDING_TIMER_SCRIPT CONFIGDIR "/recording.timer"
#define NEUTRINO_RECORDING_START_SCRIPT CONFIGDIR "/recording.start"
#define NEUTRINO_RECORDING_ENDED_SCRIPT CONFIGDIR "/recording.end"
#define NEUTRINO_ENTER_STANDBY_SCRIPT   CONFIGDIR "/standby.on"
#define NEUTRINO_LEAVE_STANDBY_SCRIPT   CONFIGDIR "/standby.off"

#define NEUTRINO_SCAN_SETTINGS_FILE     CONFIGDIR "/scan.conf"
#define NEUTRINO_PARENTALLOCKED_FILE    DATADIR   "/neutrino/.plocked"
#define NEUTRINO_SCAN_START_SCRIPT CONFIGDIR "/recording.start"
#endif

int safe_mkdir(char * path)
{
	struct statfs s;
	int ret = 0;
	if(!strncmp(path, "/hdd", 4)) {
		ret = statfs("/hdd", &s);
		if((ret != 0) || (s.f_type == 0x72b6)) 
			ret = -1;
		else 
			mkdir(path, 0755);
	} else
		mkdir(path, 0755);
	return ret;
}

static int check_dir(const char * newdir)
{
	if(strncmp(newdir, "/media/sda1/", 12) && strncmp(newdir, "/media/sdb1/", 12) && strncmp(newdir, "/mnt/", 5) && strncmp(newdir, "/tmp/", 5) && strncmp(newdir, "/media/", 7)) {
		return 1;
	}

	return 0;
}

static void initGlobals(void)
{
	g_fontRenderer  = NULL;

	g_RCInput       = NULL;
	//g_Controld      = new CControldClient;
	g_Timerd        = NULL;
	//g_Zapit         = new CZapitClient;
	g_RemoteControl = NULL;

	g_EpgData       = NULL;
	g_InfoViewer    = NULL;
	g_EventList     = NULL;

	g_Locale        = new CLocaleManager;
	g_PluginList    = NULL;
	InfoClock =  NULL;
	g_CamHandler = NULL;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
+          CNeutrinoApp - Constructor, initialize g_fontRenderer                      +
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
CNeutrinoApp::CNeutrinoApp()
: configfile('\t')
{
	standby_pressed_at.tv_sec = 0;

	frameBuffer = CFrameBuffer::getInstance();
	frameBuffer->setIconBasePath(DATADIR "/neutrino/icons/");

	SetupFrameBuffer();

	mode = mode_unknown;
	channelList       = NULL;
	TVchannelList       = NULL;
	RADIOchannelList       = NULL;
	nextRecordingInfo = NULL;
	skipShutdownTimer=false;
	current_muted = 0;
}

/*-------------------------------------------------------------------------------------
-           CNeutrinoApp - Destructor                                                 -
-------------------------------------------------------------------------------------*/
CNeutrinoApp::~CNeutrinoApp()
{
	if (channelList)
		delete channelList;
}

CNeutrinoApp* CNeutrinoApp::getInstance()
{
	static CNeutrinoApp* neutrinoApp = NULL;

	if(!neutrinoApp) {
		neutrinoApp = new CNeutrinoApp();
		dprintf(DEBUG_DEBUG, "NeutrinoApp Instance created\n");
	}
	return neutrinoApp;
}


/**************************************************************************************
*          CNeutrinoApp -  setup Color Sheme (Neutrino)                               *
**************************************************************************************/
void CNeutrinoApp::setupColors_neutrino()
{
	g_settings.menu_Head_alpha = 0x00;
	g_settings.menu_Head_red   = 0x00;
	g_settings.menu_Head_green = 0x0A;
	g_settings.menu_Head_blue  = 0x19;

	g_settings.menu_Head_Text_alpha = 0x00;
	g_settings.menu_Head_Text_red   = 0x5f;
	g_settings.menu_Head_Text_green = 0x46;
	g_settings.menu_Head_Text_blue  = 0x00;

	g_settings.menu_Content_alpha = 0x14;
	g_settings.menu_Content_red   = 0x00;
	g_settings.menu_Content_green = 0x0f;
	g_settings.menu_Content_blue  = 0x23;

	g_settings.menu_Content_Text_alpha = 0x00;
	g_settings.menu_Content_Text_red   = 0x64;
	g_settings.menu_Content_Text_green = 0x64;
	g_settings.menu_Content_Text_blue  = 0x64;

	g_settings.menu_Content_Selected_alpha = 0x14;
	g_settings.menu_Content_Selected_red   = 0x19;
	g_settings.menu_Content_Selected_green = 0x37;
	g_settings.menu_Content_Selected_blue  = 0x64;

	g_settings.menu_Content_Selected_Text_alpha  = 0x00;
	g_settings.menu_Content_Selected_Text_red    = 0x00;
	g_settings.menu_Content_Selected_Text_green  = 0x00;
	g_settings.menu_Content_Selected_Text_blue   = 0x00;

	g_settings.menu_Content_inactive_alpha = 0x14;
	g_settings.menu_Content_inactive_red   = 0x00;
	g_settings.menu_Content_inactive_green = 0x0f;
	g_settings.menu_Content_inactive_blue  = 0x23;

	g_settings.menu_Content_inactive_Text_alpha  = 0x00;
	g_settings.menu_Content_inactive_Text_red    = 55;
	g_settings.menu_Content_inactive_Text_green  = 70;
	g_settings.menu_Content_inactive_Text_blue   = 85;

	g_settings.infobar_alpha = 0x14;
	g_settings.infobar_red   = 0x00;
	g_settings.infobar_green = 0x0e;
	g_settings.infobar_blue  = 0x23;

	g_settings.infobar_Text_alpha = 0x00;
	g_settings.infobar_Text_red   = 0x64;
	g_settings.infobar_Text_green = 0x64;
	g_settings.infobar_Text_blue  = 0x64;
}

/**************************************************************************************
*                                                                                     *
*          CNeutrinoApp -  setup Color Sheme (classic)                                *
*                                                                                     *
**************************************************************************************/
void CNeutrinoApp::setupColors_classic()
{
	g_settings.menu_Head_alpha = 20;
	g_settings.menu_Head_red   =  5;
	g_settings.menu_Head_green = 10;
	g_settings.menu_Head_blue  = 60;

	g_settings.menu_Head_Text_alpha = 0;
	g_settings.menu_Head_Text_red   = 100;
	g_settings.menu_Head_Text_green = 100;
	g_settings.menu_Head_Text_blue  = 100;

	g_settings.menu_Content_alpha = 20;
	g_settings.menu_Content_red   = 50;
	g_settings.menu_Content_green = 50;
	g_settings.menu_Content_blue  = 50;

	g_settings.menu_Content_Text_alpha = 0;
	g_settings.menu_Content_Text_red   = 100;
	g_settings.menu_Content_Text_green = 100;
	g_settings.menu_Content_Text_blue  = 100;

	g_settings.menu_Content_Selected_alpha = 20;
	g_settings.menu_Content_Selected_red   = 5;
	g_settings.menu_Content_Selected_green = 10;
	g_settings.menu_Content_Selected_blue  = 60;

	g_settings.menu_Content_Selected_Text_alpha  = 0;
	g_settings.menu_Content_Selected_Text_red    = 100;
	g_settings.menu_Content_Selected_Text_green  = 100;
	g_settings.menu_Content_Selected_Text_blue   = 100;

	g_settings.menu_Content_inactive_alpha = 20;
	g_settings.menu_Content_inactive_red   = 50;
	g_settings.menu_Content_inactive_green = 50;
	g_settings.menu_Content_inactive_blue  = 50;

	g_settings.menu_Content_inactive_Text_alpha  = 0;
	g_settings.menu_Content_inactive_Text_red    = 80;
	g_settings.menu_Content_inactive_Text_green  = 80;
	g_settings.menu_Content_inactive_Text_blue   = 80;

	g_settings.infobar_alpha = 20;
	g_settings.infobar_red   = 5;
	g_settings.infobar_green = 10;
	g_settings.infobar_blue  = 60;

	g_settings.infobar_Text_alpha = 0;
	g_settings.infobar_Text_red   = 100;
	g_settings.infobar_Text_green = 100;
	g_settings.infobar_Text_blue  = 100;
}
void CNeutrinoApp::setupColors_ru()
{

	g_settings.infobar_Text_alpha=0;
	g_settings.infobar_Text_blue=100;
	g_settings.infobar_Text_green=100;
	g_settings.infobar_Text_red=100;

	g_settings.infobar_alpha=0;
	g_settings.infobar_blue=40;
	g_settings.infobar_green=29;
	g_settings.infobar_red=25;

	g_settings.menu_Content_Selected_Text_alpha=0;
	g_settings.menu_Content_Selected_Text_blue=0;
	g_settings.menu_Content_Selected_Text_green=0;
	g_settings.menu_Content_Selected_Text_red=0;

	g_settings.menu_Content_Selected_alpha=0;
	g_settings.menu_Content_Selected_blue=70;
	g_settings.menu_Content_Selected_green=65;
	g_settings.menu_Content_Selected_red=65;

	g_settings.menu_Content_Text_alpha=0;
	g_settings.menu_Content_Text_blue=100;
	g_settings.menu_Content_Text_green=100;
	g_settings.menu_Content_Text_red=100;

	g_settings.menu_Content_alpha=0;
	g_settings.menu_Content_blue=40;
	g_settings.menu_Content_green=30;
	g_settings.menu_Content_red=25;
;
	g_settings.menu_Content_inactive_Text_alpha=0;
	g_settings.menu_Content_inactive_Text_blue=100;
	g_settings.menu_Content_inactive_Text_green=100;
	g_settings.menu_Content_inactive_Text_red=100;

	g_settings.menu_Content_inactive_alpha=0;
	g_settings.menu_Content_inactive_blue=40;
	g_settings.menu_Content_inactive_green=30;
	g_settings.menu_Content_inactive_red=25;

	g_settings.menu_Head_Text_alpha=0;
	g_settings.menu_Head_Text_blue=0;
	g_settings.menu_Head_Text_green=70;
	g_settings.menu_Head_Text_red=95;

	g_settings.menu_Head_alpha=0;
	g_settings.menu_Head_blue=30;
	g_settings.menu_Head_green=20;
	g_settings.menu_Head_red=15;
}
void CNeutrinoApp::setupColors_red()
{
	g_settings.infobar_Text_alpha=0;
	g_settings.infobar_Text_blue=100;
	g_settings.infobar_Text_green=100;
	g_settings.infobar_Text_red=100;

	g_settings.infobar_alpha=20;
	g_settings.infobar_blue=0;
	g_settings.infobar_green=14;
	g_settings.infobar_red=40;

	g_settings.menu_Content_Selected_Text_alpha=0;
	g_settings.menu_Content_Selected_Text_blue=0;
	g_settings.menu_Content_Selected_Text_green=0;
	g_settings.menu_Content_Selected_Text_red=0;

	g_settings.menu_Content_Selected_alpha=20;
	g_settings.menu_Content_Selected_blue=100;
	g_settings.menu_Content_Selected_green=100;
	g_settings.menu_Content_Selected_red=100;

	g_settings.menu_Content_Text_alpha=0;
	g_settings.menu_Content_Text_blue=100;
	g_settings.menu_Content_Text_green=100;
	g_settings.menu_Content_Text_red=100;

	g_settings.menu_Content_inactive_Text_alpha=0;
	g_settings.menu_Content_inactive_Text_blue=35;
	g_settings.menu_Content_inactive_Text_green=85;
	g_settings.menu_Content_inactive_Text_red=85;

	g_settings.menu_Content_inactive_alpha=20;
	g_settings.menu_Content_inactive_blue=0;
	g_settings.menu_Content_inactive_green=15;
	g_settings.menu_Content_inactive_red=35;

	g_settings.menu_Content_alpha=20;
	g_settings.menu_Content_blue=0;
	g_settings.menu_Content_green=15;
	g_settings.menu_Content_red=40;

	g_settings.menu_Head_Text_alpha=0;
	g_settings.menu_Head_Text_blue=0;
	g_settings.menu_Head_Text_green=70;
	g_settings.menu_Head_Text_red=95;

	g_settings.menu_Head_alpha=0;
	g_settings.menu_Head_blue=0;
	g_settings.menu_Head_green=10;
	g_settings.menu_Head_red=40;
}

/**************************************************************************************
*                                                                                     *
*          CNeutrinoApp -  setup Color Sheme (darkblue)                               *
*                                                                                     *
**************************************************************************************/
void CNeutrinoApp::setupColors_dblue()
{
	g_settings.menu_Head_alpha = 0;
	g_settings.menu_Head_red   = 0;
	g_settings.menu_Head_green = 0;
	g_settings.menu_Head_blue  = 50;

	g_settings.menu_Head_Text_alpha = 0;
	g_settings.menu_Head_Text_red   = 95;
	g_settings.menu_Head_Text_green = 100;
	g_settings.menu_Head_Text_blue  = 100;

	g_settings.menu_Content_alpha = 20;
	g_settings.menu_Content_red   = 0;
	g_settings.menu_Content_green = 0;
	g_settings.menu_Content_blue  = 20;

	g_settings.menu_Content_Text_alpha = 0;
	g_settings.menu_Content_Text_red   = 100;
	g_settings.menu_Content_Text_green = 100;
	g_settings.menu_Content_Text_blue  = 100;

	g_settings.menu_Content_Selected_alpha = 15;
	g_settings.menu_Content_Selected_red   = 0;
	g_settings.menu_Content_Selected_green = 65;
	g_settings.menu_Content_Selected_blue  = 0;

	g_settings.menu_Content_Selected_Text_alpha  = 0;
	g_settings.menu_Content_Selected_Text_red    = 0;
	g_settings.menu_Content_Selected_Text_green  = 0;
	g_settings.menu_Content_Selected_Text_blue   = 0;

	g_settings.menu_Content_inactive_alpha = 20;
	g_settings.menu_Content_inactive_red   = 0;
	g_settings.menu_Content_inactive_green = 0;
	g_settings.menu_Content_inactive_blue  = 15;

	g_settings.menu_Content_inactive_Text_alpha  = 0;
	g_settings.menu_Content_inactive_Text_red    = 55;
	g_settings.menu_Content_inactive_Text_green  = 70;
	g_settings.menu_Content_inactive_Text_blue   = 85;

	g_settings.infobar_alpha = 20;
	g_settings.infobar_red   = 0;
	g_settings.infobar_green = 0;
	g_settings.infobar_blue  = 20;

	g_settings.infobar_Text_alpha = 0;
	g_settings.infobar_Text_red   = 100;
	g_settings.infobar_Text_green = 100;
	g_settings.infobar_Text_blue  = 100;
}

/**************************************************************************************
*                                                                                     *
*          CNeutrinoApp -  setup Color Sheme (dvb2000)                                *
*                                                                                     *
**************************************************************************************/
void CNeutrinoApp::setupColors_dvb2k()
{
	g_settings.menu_Head_alpha = 0;
	g_settings.menu_Head_red   = 25;
	g_settings.menu_Head_green = 25;
	g_settings.menu_Head_blue  = 25;

	g_settings.menu_Head_Text_alpha = 0;
	g_settings.menu_Head_Text_red   = 100;
	g_settings.menu_Head_Text_green = 100;
	g_settings.menu_Head_Text_blue  = 0;

	g_settings.menu_Content_alpha = 0;
	g_settings.menu_Content_red   = 0;
	g_settings.menu_Content_green = 20;
	g_settings.menu_Content_blue  = 0;

	g_settings.menu_Content_Text_alpha = 0;
	g_settings.menu_Content_Text_red   = 100;
	g_settings.menu_Content_Text_green = 100;
	g_settings.menu_Content_Text_blue  = 100;

	g_settings.menu_Content_Selected_alpha = 0;
	g_settings.menu_Content_Selected_red   = 100;
	g_settings.menu_Content_Selected_green = 100;
	g_settings.menu_Content_Selected_blue  = 100;

	g_settings.menu_Content_Selected_Text_alpha  = 0;
	g_settings.menu_Content_Selected_Text_red    = 0;
	g_settings.menu_Content_Selected_Text_green  = 0;
	g_settings.menu_Content_Selected_Text_blue   = 0;

	g_settings.menu_Content_inactive_alpha = 20;
	g_settings.menu_Content_inactive_red   = 0;
	g_settings.menu_Content_inactive_green = 25;
	g_settings.menu_Content_inactive_blue  = 0;

	g_settings.menu_Content_inactive_Text_alpha  = 0;
	g_settings.menu_Content_inactive_Text_red    = 100;
	g_settings.menu_Content_inactive_Text_green  = 100;
	g_settings.menu_Content_inactive_Text_blue   = 0;

	g_settings.infobar_alpha = 5;
	g_settings.infobar_red   = 0;
	g_settings.infobar_green = 19;
	g_settings.infobar_blue  = 0;

	g_settings.infobar_Text_alpha = 0;
	g_settings.infobar_Text_red   = 100;
	g_settings.infobar_Text_green = 100;
	g_settings.infobar_Text_blue  = 100;
}

#define FONT_STYLE_REGULAR 0
#define FONT_STYLE_BOLD    1
#define FONT_STYLE_ITALIC  2

extern font_sizes_groups_struct font_sizes_groups[];
extern font_sizes_struct neutrino_font[];

const font_sizes_struct signal_font = {LOCALE_FONTSIZE_INFOBAR_SMALL      ,  14, FONT_STYLE_REGULAR, 1};

typedef struct lcd_setting_t
{
	const char * const name;
	const unsigned int default_value;
} lcd_setting_struct_t;

const lcd_setting_struct_t lcd_setting[LCD_SETTING_COUNT] =
{
	{"lcd_brightness"       , DEFAULT_VFD_BRIGHTNESS       },
	{"lcd_standbybrightness", DEFAULT_VFD_STANDBYBRIGHTNESS},
	{"lcd_contrast"         , DEFAULT_LCD_CONTRAST         },
	{"lcd_power"            , DEFAULT_LCD_POWER            },
	{"lcd_inverse"          , DEFAULT_LCD_INVERSE          },
	{"lcd_show_volume"      , DEFAULT_LCD_SHOW_VOLUME      },
	{"lcd_autodimm"         , DEFAULT_LCD_AUTODIMM         }
};

/**************************************************************************************
*          CNeutrinoApp -  loadSetup, load the application-settings                   *
**************************************************************************************/
int CNeutrinoApp::loadSetup(const char * fname)
{
	char cfg_key[81];
	int erg = 0;

	configfile.clear();
	//settings laden - und dabei Defaults setzen!
	if(!configfile.loadConfig(fname)) {
		//file existiert nicht
		erg = 1;
	}
        std::ifstream checkParentallocked(NEUTRINO_PARENTALLOCKED_FILE);
	if(checkParentallocked) {
	        parentallocked = true;
	        checkParentallocked.close();
	}
	//video
	g_settings.video_Mode = configfile.getInt32("video_Mode", 2); // default 720p/50HZ
	prev_video_mode = g_settings.video_Mode;
	prev_asp_mode = g_settings.video_Format;
	g_settings.analog_mode1 = configfile.getInt32("analog_mode1", 0); // default RGB
	g_settings.analog_mode2 = configfile.getInt32("analog_mode2", 0); // default RGB

	g_settings.video_Format = configfile.getInt32("video_Format", 1);
	g_settings.video_43mode = configfile.getInt32("video_43mode", 0);
	g_settings.current_volume = configfile.getInt32("current_volume", 100);
	g_settings.channel_mode = configfile.getInt32("channel_mode", LIST_MODE_PROV);
	g_settings.video_csync = configfile.getInt32( "video_csync", 0 );

	g_settings.fan_speed = configfile.getInt32( "fan_speed", 1);
	if(g_settings.fan_speed < 1) g_settings.fan_speed = 1;//FIXME disable OFF

	g_settings.srs_enable = configfile.getInt32( "srs_enable", 0);
	g_settings.srs_algo = configfile.getInt32( "srs_algo", 1);
	g_settings.srs_ref_volume = configfile.getInt32( "srs_ref_volume", 40);//FIXME
	g_settings.srs_nmgr_enable = configfile.getInt32( "srs_nmgr_enable", 0);
	g_settings.hdmi_dd = configfile.getInt32( "hdmi_dd", 0);
	g_settings.spdif_dd = configfile.getInt32( "spdif_dd", 1);
	g_settings.audio_spdif_dd = configfile.getInt32( "audio_spdif_dd", 0);
	g_settings.audio_spdif_dts = configfile.getInt32( "audio_spdif_dts", 0);
	g_settings.audio_spdif_aac = configfile.getInt32( "audio_spdif_aac", 0);
	g_settings.avsync = configfile.getInt32( "avsync", 1);
	g_settings.clockrec = configfile.getInt32( "clockrec", 1);
	g_settings.video_dbdr = configfile.getInt32("video_dbdr", 2);

	for(int i = 0; i < VIDEOMENU_VIDEOMODE_OPTION_COUNT; i++) {
		sprintf(cfg_key, "enabled_video_mode_%d", i);
		g_settings.enabled_video_modes[i] = configfile.getInt32(cfg_key, 1);
	}
	g_settings.cpufreq = configfile.getInt32("cpufreq", 0);
	g_settings.standby_cpufreq = configfile.getInt32("standby_cpufreq", 100);
	g_settings.rounded_corners = 1; //FIXME
//FIXME
	g_settings.cpufreq = 0;
	g_settings.standby_cpufreq = 50;

	g_settings.make_hd_list = configfile.getInt32("make_hd_list", 1);
	//fb-alphawerte f�r gtx
	g_settings.gtx_alpha1 = configfile.getInt32( "gtx_alpha1", 255);
	g_settings.gtx_alpha2 = configfile.getInt32( "gtx_alpha2", 1);

	//misc
	g_settings.power_standby = configfile.getInt32( "power_standby", 0);
	g_settings.rotor_swap = configfile.getInt32( "rotor_swap", 0);
	g_settings.emlog = configfile.getInt32( "emlog", 0);

	g_settings.hdd_fs = configfile.getInt32( "hdd_fs", 0);
	g_settings.hdd_sleep = configfile.getInt32( "hdd_sleep", 120);
	g_settings.hdd_noise = configfile.getInt32( "hdd_noise", 254);
	g_settings.shutdown_real         = configfile.getBool("shutdown_real"        , false );
	g_settings.shutdown_real_rcdelay = configfile.getBool("shutdown_real_rcdelay", false );
        strcpy(g_settings.shutdown_count, configfile.getString("shutdown_count","0").c_str());
	g_settings.infobar_sat_display   = configfile.getBool("infobar_sat_display"  , true );
	g_settings.infobar_subchan_disp_pos = configfile.getInt32("infobar_subchan_disp_pos"  , 0 );

	//audio
	g_settings.audio_AnalogMode = configfile.getInt32( "audio_AnalogMode", 0 );
	g_settings.audio_DolbyDigital    = configfile.getBool("audio_DolbyDigital"   , false);

	g_settings.audio_avs_Control = false;
	g_settings.audio_english = configfile.getInt32( "audio_english", 0 );
	g_settings.zap_cycle = configfile.getInt32( "zap_cycle", 1 );
	g_settings.sms_channel = configfile.getInt32( "sms_channel", 0 );
	strcpy( g_settings.audio_PCMOffset, configfile.getString( "audio_PCMOffset", "0" ).c_str() );

	//vcr
	g_settings.vcr_AutoSwitch        = configfile.getBool("vcr_AutoSwitch"       , true );

	//language
	strcpy(g_settings.language, configfile.getString("language", "").c_str());
	strcpy(g_settings.timezone, configfile.getString("timezone", "(GMT+01:00) Amsterdam, Berlin, Bern, Rome, Vienna").c_str());
	//epg dir
        g_settings.epg_cache            = configfile.getString("epg_cache_time", "14");
        g_settings.epg_extendedcache    = configfile.getString("epg_extendedcache_time", "360");
        g_settings.epg_old_events       = configfile.getString("epg_old_events", "1");
        g_settings.epg_max_events       = configfile.getString("epg_max_events", "30000");
        g_settings.epg_dir              = configfile.getString("epg_dir", "/media/sda1/epg");
        // NTP-Server for sectionsd
        g_settings.network_ntpserver    = configfile.getString("network_ntpserver", "time.fu-berlin.de");
        g_settings.network_ntprefresh   = configfile.getString("network_ntprefresh", "30" );
        g_settings.network_ntpenable    = configfile.getBool("network_ntpenable", false);

	g_settings.epg_save = configfile.getBool("epg_save", false);

	//widget settings
	//FIXME not work yet g_settings.widget_fade           = configfile.getBool("widget_fade"          , false );
	g_settings.widget_fade = false;

	//colors (neutrino defaultcolors)
	g_settings.menu_Head_alpha = configfile.getInt32( "menu_Head_alpha", 0x00 );
	g_settings.menu_Head_red = configfile.getInt32( "menu_Head_red", 0x00 );
	g_settings.menu_Head_green = configfile.getInt32( "menu_Head_green", 0x0A );
	g_settings.menu_Head_blue = configfile.getInt32( "menu_Head_blue", 0x19 );

	g_settings.menu_Head_Text_alpha = configfile.getInt32( "menu_Head_Text_alpha", 0x00 );
	g_settings.menu_Head_Text_red = configfile.getInt32( "menu_Head_Text_red", 0x5f );
	g_settings.menu_Head_Text_green = configfile.getInt32( "menu_Head_Text_green", 0x46 );
	g_settings.menu_Head_Text_blue = configfile.getInt32( "menu_Head_Text_blue", 0x00 );

	g_settings.menu_Content_alpha = configfile.getInt32( "menu_Content_alpha", 0x14 );
	g_settings.menu_Content_red = configfile.getInt32( "menu_Content_red", 0x00 );
	g_settings.menu_Content_green = configfile.getInt32( "menu_Content_green", 0x0f );
	g_settings.menu_Content_blue = configfile.getInt32( "menu_Content_blue", 0x23 );
	g_settings.menu_Content_Text_alpha = configfile.getInt32( "menu_Content_Text_alpha", 0x00 );
	g_settings.menu_Content_Text_red = configfile.getInt32( "menu_Content_Text_red", 0x64 );
	g_settings.menu_Content_Text_green = configfile.getInt32( "menu_Content_Text_green", 0x64 );
	g_settings.menu_Content_Text_blue = configfile.getInt32( "menu_Content_Text_blue", 0x64 );

	g_settings.menu_Content_Selected_alpha = configfile.getInt32( "menu_Content_Selected_alpha", 0x14 );
	g_settings.menu_Content_Selected_red = configfile.getInt32( "menu_Content_Selected_red", 0x19 );
	g_settings.menu_Content_Selected_green = configfile.getInt32( "menu_Content_Selected_green", 0x37 );
	g_settings.menu_Content_Selected_blue = configfile.getInt32( "menu_Content_Selected_blue", 0x64 );

	g_settings.menu_Content_Selected_Text_alpha = configfile.getInt32( "menu_Content_Selected_Text_alpha", 0x00 );
	g_settings.menu_Content_Selected_Text_red = configfile.getInt32( "menu_Content_Selected_Text_red", 0x00 );
	g_settings.menu_Content_Selected_Text_green = configfile.getInt32( "menu_Content_Selected_Text_green", 0x00 );
	g_settings.menu_Content_Selected_Text_blue = configfile.getInt32( "menu_Content_Selected_Text_blue", 0x00 );

	g_settings.menu_Content_inactive_alpha = configfile.getInt32( "menu_Content_inactive_alpha", 0x14 );
	g_settings.menu_Content_inactive_red = configfile.getInt32( "menu_Content_inactive_red", 0x00 );
	g_settings.menu_Content_inactive_green = configfile.getInt32( "menu_Content_inactive_green", 0x0f );
	g_settings.menu_Content_inactive_blue = configfile.getInt32( "menu_Content_inactive_blue", 0x23 );

	g_settings.menu_Content_inactive_Text_alpha = configfile.getInt32( "menu_Content_inactive_Text_alpha", 0x00 );
	g_settings.menu_Content_inactive_Text_red = configfile.getInt32( "menu_Content_inactive_Text_red", 55 );
	g_settings.menu_Content_inactive_Text_green = configfile.getInt32( "menu_Content_inactive_Text_green", 70 );
	g_settings.menu_Content_inactive_Text_blue = configfile.getInt32( "menu_Content_inactive_Text_blue", 85 );

	g_settings.infobar_alpha = configfile.getInt32( "infobar_alpha", 0x14 );
	g_settings.infobar_red = configfile.getInt32( "infobar_red", 0x00 );
	g_settings.infobar_green = configfile.getInt32( "infobar_green", 0x0e );
	g_settings.infobar_blue = configfile.getInt32( "infobar_blue", 0x23 );

	g_settings.infobar_Text_alpha = configfile.getInt32( "infobar_Text_alpha", 0x00 );
	g_settings.infobar_Text_red = configfile.getInt32( "infobar_Text_red", 0x64 );
	g_settings.infobar_Text_green = configfile.getInt32( "infobar_Text_green", 0x64 );
	g_settings.infobar_Text_blue = configfile.getInt32( "infobar_Text_blue", 0x64 );

	//network
	for(int i=0 ; i < NETWORK_NFS_NR_OF_ENTRIES ; i++) {
		sprintf(cfg_key, "network_nfs_ip_%d", i);
		g_settings.network_nfs_ip[i] = configfile.getString(cfg_key, "");
		sprintf(cfg_key, "network_nfs_dir_%d", i);
		strcpy( g_settings.network_nfs_dir[i], configfile.getString( cfg_key, "" ).c_str() );
		sprintf(cfg_key, "network_nfs_local_dir_%d", i);
		strcpy( g_settings.network_nfs_local_dir[i], configfile.getString( cfg_key, "" ).c_str() );
		sprintf(cfg_key, "network_nfs_automount_%d", i);
		g_settings.network_nfs_automount[i] = configfile.getInt32( cfg_key, 0);
		sprintf(cfg_key, "network_nfs_type_%d", i);
		g_settings.network_nfs_type[i] = configfile.getInt32( cfg_key, 0);
		sprintf(cfg_key,"network_nfs_username_%d", i);
		strcpy( g_settings.network_nfs_username[i], configfile.getString( cfg_key, "" ).c_str() );
		sprintf(cfg_key, "network_nfs_password_%d", i);
		strcpy( g_settings.network_nfs_password[i], configfile.getString( cfg_key, "" ).c_str() );
		sprintf(cfg_key, "network_nfs_mount_options1_%d", i);
		strcpy( g_settings.network_nfs_mount_options1[i], configfile.getString( cfg_key, "ro,soft,udp" ).c_str() );
		sprintf(cfg_key, "network_nfs_mount_options2_%d", i);
		strcpy( g_settings.network_nfs_mount_options2[i], configfile.getString( cfg_key, "nolock,rsize=8192,wsize=8192" ).c_str() );
		sprintf(cfg_key, "network_nfs_mac_%d", i);
		strcpy( g_settings.network_nfs_mac[i], configfile.getString( cfg_key, "11:22:33:44:55:66").c_str() );
	}
	strcpy( g_settings.network_nfs_audioplayerdir, configfile.getString( "network_nfs_audioplayerdir", "/media/sda1/music" ).c_str() );
	strcpy( g_settings.network_nfs_picturedir, configfile.getString( "network_nfs_picturedir", "/media/sda1/pictures" ).c_str() );
	strcpy( g_settings.network_nfs_moviedir, configfile.getString( "network_nfs_moviedir", "/media/sda1/movies" ).c_str() );
	strcpy( g_settings.network_nfs_recordingdir, configfile.getString( "network_nfs_recordingdir", "/media/sda1/movies" ).c_str() );
	strcpy( g_settings.timeshiftdir, configfile.getString( "timeshiftdir", "" ).c_str() );

	g_settings.temp_timeshift = configfile.getInt32( "temp_timeshift", 0 );
	g_settings.auto_timeshift = configfile.getInt32( "auto_timeshift", 0 );
	g_settings.auto_delete = configfile.getInt32( "auto_delete", 1 );

	if(strlen(g_settings.timeshiftdir) == 0) {
		sprintf(timeshiftDir, "%s/.timeshift", g_settings.network_nfs_recordingdir);
		safe_mkdir(timeshiftDir);
	} else {
		if(strcmp(g_settings.timeshiftdir, g_settings.network_nfs_recordingdir))
			strncpy(timeshiftDir, g_settings.timeshiftdir, sizeof(timeshiftDir));
		else
			sprintf(timeshiftDir, "%s/.timeshift", g_settings.network_nfs_recordingdir);
	}
printf("***************************** rec dir %s timeshift dir %s\n", g_settings.network_nfs_recordingdir, timeshiftDir);

	if(g_settings.auto_delete) {
		if(strcmp(g_settings.timeshiftdir, g_settings.network_nfs_recordingdir)) {
			char buf[512];
			sprintf(buf, "rm -f %s/*_temp.ts %s/*_temp.xml &", timeshiftDir, timeshiftDir);
			system(buf);
		}
	}
	g_settings.record_hours = configfile.getInt32( "record_hours", 4 );
	g_settings.filesystem_is_utf8              = configfile.getBool("filesystem_is_utf8"                 , true );

	//recording (server + vcr)
	g_settings.recording_type = configfile.getInt32("recording_type", RECORDING_FILE);
	g_settings.recording_stopplayback          = configfile.getBool("recording_stopplayback"             , false);
	g_settings.recording_stopsectionsd         = configfile.getBool("recording_stopsectionsd"            , true );
	g_settings.recording_server_ip = configfile.getString("recording_server_ip", "127.0.0.1");
	strcpy( g_settings.recording_server_port, configfile.getString( "recording_server_port", "4000").c_str() );
	g_settings.recording_server_wakeup = configfile.getInt32( "recording_server_wakeup", 0 );
	strcpy( g_settings.recording_server_mac, configfile.getString( "recording_server_mac", "11:22:33:44:55:66").c_str() );
	g_settings.recording_vcr_no_scart = configfile.getInt32( "recording_vcr_no_scart", false);
	strcpy( g_settings.recording_splitsize, configfile.getString( "recording_splitsize", "2048").c_str() );
	g_settings.recording_use_o_sync            = configfile.getBool("recordingmenu.use_o_sync"           , false);
	g_settings.recording_use_fdatasync         = configfile.getBool("recordingmenu.use_fdatasync"        , false);
	g_settings.recording_audio_pids_default    = configfile.getInt32("recording_audio_pids_default", TIMERD_APIDS_STD | TIMERD_APIDS_AC3);
	g_settings.recording_zap_on_announce       = configfile.getBool("recording_zap_on_announce"      , false);

	g_settings.recording_stream_vtxt_pid       = configfile.getBool("recordingmenu.stream_vtxt_pid"      , false);
	g_settings.recording_stream_pmt_pid        = configfile.getBool("recordingmenu.stream_pmt_pid"      , false);
	strcpy( g_settings.recording_ringbuffers, configfile.getString( "recordingmenu.ringbuffers", "20").c_str() );
	g_settings.recording_choose_direct_rec_dir = configfile.getInt32( "recording_choose_direct_rec_dir", 0 );
	g_settings.recording_epg_for_filename      = configfile.getBool("recording_epg_for_filename"         , true);
	g_settings.recording_save_in_channeldir      = configfile.getBool("recording_save_in_channeldir"         , false);
	g_settings.recording_in_spts_mode          = true;
	//streaming (server)
	g_settings.streaming_type = configfile.getInt32( "streaming_type", 0 );
	g_settings.streaming_server_ip = configfile.getString("streaming_server_ip", "10.10.10.10");
	strcpy( g_settings.streaming_server_port, configfile.getString( "streaming_server_port", "8080").c_str() );
	strcpy( g_settings.streaming_server_cddrive, configfile.getString("streaming_server_cddrive", "D:").c_str() );
	strcpy( g_settings.streaming_videorate,  configfile.getString("streaming_videorate", "1000").c_str() );
	strcpy( g_settings.streaming_audiorate, configfile.getString("streaming_audiorate", "192").c_str() );
	strcpy( g_settings.streaming_server_startdir, configfile.getString("streaming_server_startdir", "C:/Movies").c_str() );
	g_settings.streaming_transcode_audio = configfile.getInt32( "streaming_transcode_audio", 0 );
	g_settings.streaming_force_transcode_video = configfile.getInt32( "streaming_force_transcode_video", 0 );
	g_settings.streaming_transcode_video_codec = configfile.getInt32( "streaming_transcode_video_codec", 0 );
	g_settings.streaming_force_avi_rawaudio = configfile.getInt32( "streaming_force_avi_rawaudio", 0 );
	g_settings.streaming_resolution = configfile.getInt32( "streaming_resolution", 0 );

	// default plugin for movieplayer
	g_settings.movieplayer_plugin = configfile.getString( "movieplayer_plugin", "Teletext" );
	g_settings.onekey_plugin = configfile.getString( "onekey_plugin", "noplugin" );

	//rc-key configuration
	g_settings.key_tvradio_mode = configfile.getInt32( "key_tvradio_mode", CRCInput::RC_nokey );

	g_settings.key_channelList_pageup = configfile.getInt32( "key_channelList_pageup",  CRCInput::RC_page_up );
	g_settings.key_channelList_pagedown = configfile.getInt32( "key_channelList_pagedown", CRCInput::RC_page_down );
	g_settings.key_channelList_cancel = configfile.getInt32( "key_channelList_cancel",  CRCInput::RC_home );
	g_settings.key_channelList_sort = configfile.getInt32( "key_channelList_sort",  CRCInput::RC_blue );
	g_settings.key_channelList_addrecord = configfile.getInt32( "key_channelList_addrecord",  CRCInput::RC_nokey );
	g_settings.key_channelList_addremind = configfile.getInt32( "key_channelList_addremind",  CRCInput::RC_nokey );

	g_settings.key_list_start = configfile.getInt32( "key_list_start", CRCInput::RC_nokey );
	g_settings.key_list_end = configfile.getInt32( "key_list_end", CRCInput::RC_nokey );
	g_settings.menu_left_exit = configfile.getInt32( "menu_left_exit", 0 );

	g_settings.audio_run_player = configfile.getInt32( "audio_run_player", 1 );
	g_settings.key_click = configfile.getInt32( "key_click", 1 );
	g_settings.timeshift_pause = configfile.getInt32( "timeshift_pause", 1 );
	g_settings.mpkey_rewind = configfile.getInt32( "mpkey.rewind", CRCInput::RC_rewind );
	g_settings.mpkey_forward = configfile.getInt32( "mpkey.forward", CRCInput::RC_forward );
	g_settings.mpkey_pause = configfile.getInt32( "mpkey.pause", CRCInput::RC_pause );
	//g_settings.mpkey_stop = configfile.getInt32( "mpkey.stop", CRCInput::RC_tv );
	g_settings.mpkey_stop = configfile.getInt32( "mpkey.stop", CRCInput::RC_stop );
	g_settings.mpkey_play = configfile.getInt32( "mpkey.play", CRCInput::RC_play );
	g_settings.mpkey_audio = configfile.getInt32( "mpkey.audio", CRCInput::RC_audio );
	g_settings.mpkey_subt = configfile.getInt32( "mpkey.subt", CRCInput::RC_video );
	g_settings.mpkey_time = configfile.getInt32( "mpkey.time", CRCInput::RC_ok );
	g_settings.mpkey_bookmark = configfile.getInt32( "mpkey.bookmark", CRCInput::RC_yellow );
	g_settings.mpkey_plugin = configfile.getInt32( "mpkey.plugin", CRCInput::RC_blue );
	g_settings.key_timeshift = configfile.getInt32( "key_timeshift", CRCInput::RC_pause );
	g_settings.key_plugin = configfile.getInt32( "key_plugin", CRCInput::RC_nokey );
	g_settings.key_unlock = configfile.getInt32( "key_unlock", CRCInput::RC_setup );
//printf("get: key_unlock =============== %d\n", g_settings.key_unlock);

//rfmod
	g_settings.rf_subcarrier = configfile.getInt32( "rf_subcarrier", 1);
	g_settings.rf_soundenable = configfile.getInt32( "rf_soundenable", 0);
	g_settings.rf_channel = configfile.getInt32( "rf_channel", 36);
	g_settings.rf_finetune = configfile.getInt32( "rf_finetune", 0);
	g_settings.rf_standby = configfile.getInt32( "rf_standby", 0);

	g_settings.key_quickzap_up = configfile.getInt32( "key_quickzap_up",  CRCInput::RC_up );
	g_settings.key_quickzap_down = configfile.getInt32( "key_quickzap_down",  CRCInput::RC_down );
	g_settings.key_subchannel_up = configfile.getInt32( "key_subchannel_up",  CRCInput::RC_right );
	g_settings.key_subchannel_down = configfile.getInt32( "key_subchannel_down",  CRCInput::RC_left );
	g_settings.key_zaphistory = configfile.getInt32( "key_zaphistory",  CRCInput::RC_home );
	g_settings.key_lastchannel = configfile.getInt32( "key_lastchannel",  CRCInput::RC_0 );
	g_settings.cacheTXT = configfile.getInt32( "cacheTXT",  0);
	g_settings.minimode = configfile.getInt32( "minimode",  0);
	g_settings.mode_clock = configfile.getInt32( "mode_clock",  0);
	g_settings.virtual_zap_mode         = configfile.getBool("virtual_zap_mode"          , false);
	g_settings.spectrum         = configfile.getBool("spectrum"          , false);
	g_settings.channellist_epgtext_align_right	= configfile.getBool("channellist_epgtext_align_right"          , false);
	g_settings.channellist_extended		= configfile.getBool("channellist_extended"          , true);
	//screen configuration
	g_settings.screen_StartX = configfile.getInt32( "screen_StartX", 37 );
	g_settings.screen_StartY = configfile.getInt32( "screen_StartY", 23 );
	g_settings.screen_EndX = configfile.getInt32( "screen_EndX", 668 );
	g_settings.screen_EndY = configfile.getInt32( "screen_EndY", 555 );
	g_settings.screen_width = configfile.getInt32("screen_width", 0);
	g_settings.screen_height = configfile.getInt32("screen_height", 0);

	g_settings.bigFonts = configfile.getInt32("bigFonts", 0);

	strcpy(g_settings.repeat_blocker, configfile.getString("repeat_blocker", "150").c_str());
	strcpy(g_settings.repeat_genericblocker, configfile.getString("repeat_genericblocker", "100").c_str());
	g_settings.key_bouquet_up = configfile.getInt32( "key_bouquet_up",  CRCInput::RC_right);
	g_settings.key_bouquet_down = configfile.getInt32( "key_bouquet_down",  CRCInput::RC_left);
	g_settings.audiochannel_up_down_enable = configfile.getBool("audiochannel_up_down_enable", false);

	//Software-update
	g_settings.softupdate_mode = configfile.getInt32( "softupdate_mode", 1 );

	strcpy(g_settings.softupdate_url_file, configfile.getString("softupdate_url_file", "/var/etc/update.urls").c_str());
	strcpy(g_settings.softupdate_proxyserver, configfile.getString("softupdate_proxyserver", "" ).c_str());
	strcpy(g_settings.softupdate_proxyusername, configfile.getString("softupdate_proxyusername", "" ).c_str());
	strcpy(g_settings.softupdate_proxypassword, configfile.getString("softupdate_proxypassword", "" ).c_str());
//
	strcpy( g_settings.font_file, configfile.getString( "font_file", FONTDIR"/neutrino.ttf" ).c_str() );
	strcpy( g_settings.update_dir, configfile.getString( "update_dir", "/tmp" ).c_str() );
	//BouquetHandling
	g_settings.bouquetlist_mode = configfile.getInt32( "bouquetlist_mode", 0 );

	// parentallock
	if (!parentallocked) {
	  	g_settings.parentallock_prompt = configfile.getInt32( "parentallock_prompt", 0 );
		g_settings.parentallock_lockage = configfile.getInt32( "parentallock_lockage", 12 );
	} else {
	        g_settings.parentallock_prompt = 3;
	        g_settings.parentallock_lockage = 18;
	}
	strcpy( g_settings.parentallock_pincode, configfile.getString( "parentallock_pincode", "0000" ).c_str() );

	for (int i = 0; i < TIMING_SETTING_COUNT; i++)
		g_settings.timing[i] = configfile.getInt32(locale_real_names[timing_setting_name[i]], default_timing[i]);

	for (int i = 0; i < LCD_SETTING_COUNT; i++)
		g_settings.lcd_setting[i] = configfile.getInt32(lcd_setting[i].name, lcd_setting[i].default_value);
	strcpy(g_settings.lcd_setting_dim_time, configfile.getString("lcd_dim_time","0").c_str());
	strcpy(g_settings.lcd_setting_dim_brightness, configfile.getString("lcd_dim_brightness","0").c_str());

	//Picture-Viewer
	strcpy( g_settings.picviewer_slide_time, configfile.getString( "picviewer_slide_time", "10" ).c_str() );
	g_settings.picviewer_scaling = configfile.getInt32("picviewer_scaling", 1 /*(int)CPictureViewer::SIMPLE*/);
	g_settings.picviewer_decode_server_ip = configfile.getString("picviewer_decode_server_ip", "");
	
	//Audio-Player
	g_settings.audioplayer_display = configfile.getInt32("audioplayer_display",(int)CAudioPlayerGui::ARTIST_TITLE);
	g_settings.audioplayer_follow  = configfile.getInt32("audioplayer_follow",0);
	strcpy( g_settings.audioplayer_screensaver, configfile.getString( "audioplayer_screensaver", "1" ).c_str() );
	g_settings.audioplayer_highprio  = configfile.getInt32("audioplayer_highprio",0);
	g_settings.audioplayer_select_title_by_name = configfile.getInt32("audioplayer_select_title_by_name",0);
	g_settings.audioplayer_repeat_on = configfile.getInt32("audioplayer_repeat_on",0);
	g_settings.audioplayer_show_playlist = configfile.getInt32("audioplayer_show_playlist",1);
	g_settings.audioplayer_enable_sc_metadata = configfile.getInt32("audioplayer_enable_sc_metadata",1);

	//Filebrowser
	g_settings.filebrowser_showrights =  configfile.getInt32("filebrowser_showrights", 1);
	g_settings.filebrowser_sortmethod = configfile.getInt32("filebrowser_sortmethod", 0);
	if ((g_settings.filebrowser_sortmethod < 0) || (g_settings.filebrowser_sortmethod >= FILEBROWSER_NUMBER_OF_SORT_VARIANTS))
		g_settings.filebrowser_sortmethod = 0;
	g_settings.filebrowser_denydirectoryleave = configfile.getBool("filebrowser_denydirectoryleave", false);

        // USERMENU -> in system/settings.h
        //-------------------------------------------
        // this is as the current neutrino usermen
        const char* usermenu_default[SNeutrinoSettings::BUTTON_MAX]={
                "2,3,4,13",                     // RED
                "6",                            // GREEN
                "7",                            // YELLOW
                "12,10,11,14,15"    // BLUE
        };
        char txt1[81];
        std::string txt2;
        const char* txt2ptr;
        for(int button = 0; button < SNeutrinoSettings::BUTTON_MAX; button++)
        {
                snprintf(txt1,80,"usermenu_tv_%s_text",usermenu_button_def[button]);
                txt1[80] = 0; // terminate for sure
                g_settings.usermenu_text[button] = configfile.getString(txt1, "" );

                snprintf(txt1,80,"usermenu_tv_%s",usermenu_button_def[button]);
                txt2 = configfile.getString(txt1,usermenu_default[button]);
                txt2ptr = txt2.c_str();
                for( int pos = 0; pos < SNeutrinoSettings::ITEM_MAX; pos++)
                {
                        // find next comma or end of string - if it's not the first round
                        if(pos != 0)
                        {
                                while(*txt2ptr != 0 && *txt2ptr != ',')
                                        txt2ptr++;
                                if(*txt2ptr != 0)
                                        txt2ptr++;
                        }
                        if(*txt2ptr != 0)
                        {
                                g_settings.usermenu[button][pos] = atoi(txt2ptr);  // there is still a string
                                if(g_settings.usermenu[button][pos] >= SNeutrinoSettings::ITEM_MAX)
                                        g_settings.usermenu[button][pos] = 0;
                        }
                        else
                                g_settings.usermenu[button][pos] = 0;     // string empty, fill up with 0

                }
        }

	if(configfile.getUnknownKeyQueryedFlag() && (erg==0)) {
		erg = 2;
	}

#if 0
	// uboot config file
	if(fromflash) {
		g_settings.uboot_console	= 0;
		g_settings.uboot_lcd_inverse	= -1;
		g_settings.uboot_lcd_contrast	= -1;

		FILE* fd = fopen("/var/tuxbox/boot/boot.conf", "r");
		if(fd) {
			char buffer[100];

			while(fgets(buffer, 99, fd) != NULL) {
				if(strncmp(buffer,"console=",8) == 0) {
					if(strncmp(&buffer[8], "null", 4)==0)
						g_settings.uboot_console = 0;
					else if(strncmp(&buffer[8], "ttyS0", 5)==0)
						g_settings.uboot_console = 1;
					else if(strncmp(&buffer[8], "ttyCPM0", 5)==0)
						g_settings.uboot_console = 1;
					else if(strncmp(&buffer[8], "tty", 3)==0)
						g_settings.uboot_console = 2;
				}
				else if(strncmp(buffer,"lcd_inverse=", 12) == 0) {
					g_settings.uboot_lcd_inverse = atoi(&buffer[12]);
				}
				else if(strncmp(buffer,"lcd_contrast=", 13) == 0) {
					g_settings.uboot_lcd_contrast = atoi(&buffer[13]);
				}
				else
					printf("unknown entry found in boot.conf\n");
			}

			fclose(fd);
		}
		g_settings.uboot_console_bak = g_settings.uboot_console;
	}
#endif
#define DEFAULT_X_OFF 85
#define DEFAULT_Y_OFF 34


	if((g_settings.screen_width != (int) frameBuffer->getScreenWidth(true)) 
		|| (g_settings.screen_height != (int) frameBuffer->getScreenHeight(true))) {
		g_settings.screen_StartX = DEFAULT_X_OFF;
		g_settings.screen_StartY = DEFAULT_Y_OFF;
		g_settings.screen_EndX = frameBuffer->getScreenWidth(true) - DEFAULT_X_OFF;
		g_settings.screen_EndY = frameBuffer->getScreenHeight(true) - DEFAULT_Y_OFF;
		g_settings.screen_width = frameBuffer->getScreenWidth(true);
		g_settings.screen_height = frameBuffer->getScreenHeight(true);
	}
	if(erg)
		configfile.setModifiedFlag(true);
	return erg;
}

/**************************************************************************************
*          CNeutrinoApp -  saveSetup, save the application-settings                   *
**************************************************************************************/
void CNeutrinoApp::saveSetup(const char * fname)
{
	char cfg_key[81];
	//uboot; write config only on changes
#if 0
	if (fromflash &&
	    ((g_settings.uboot_console_bak != g_settings.uboot_console) ||
	     (g_settings.uboot_lcd_inverse  != g_settings.lcd_setting[SNeutrinoSettings::LCD_INVERSE]) ||
	     (g_settings.uboot_lcd_contrast != g_settings.lcd_setting[SNeutrinoSettings::LCD_CONTRAST])))
	{
		bool newkernel = 0;
		FILE* fd = fopen("/proc/version", "r");
		if(fd != NULL) {
			char buf[128];
			fgets(buf, 127, fd);
			fclose(fd);
			if(strstr(buf, "version 2.6"))
				newkernel = 1;
//printf("new: %d kernel:: %s\n", newkernel, buf);
		}
		fd = fopen("/var/tuxbox/boot/boot.conf", "w");

		if(fd != NULL) {
			const char * buffer;
			g_settings.uboot_console_bak    = g_settings.uboot_console;
			g_settings.uboot_lcd_inverse	= g_settings.lcd_setting[SNeutrinoSettings::LCD_INVERSE];
			g_settings.uboot_lcd_contrast	= g_settings.lcd_setting[SNeutrinoSettings::LCD_CONTRAST];

			switch(g_settings.uboot_console) {
			case 1:
				buffer = newkernel ? "ttyCPM0" : "ttyS0";
				break;
			case 2:
				buffer = "tty";
				break;
			default:
				buffer = "null";
				break;
			}
			fprintf(fd, "console=%s\n" "lcd_inverse=%d\n" "lcd_contrast=%d\n", buffer, g_settings.uboot_lcd_inverse, g_settings.uboot_lcd_contrast);
			fclose(fd);
		} else {
			dprintf(DEBUG_NORMAL, "unable to write file /var/tuxbox/boot/boot.conf\n");
		}
	}
#endif
	//scan settings
	if(!scanSettings.saveSettings(NEUTRINO_SCAN_SETTINGS_FILE)) {
		dprintf(DEBUG_NORMAL, "error while saving scan-settings!\n");
	}

	//video
	configfile.setInt32( "video_Mode", g_settings.video_Mode );
	configfile.setInt32( "analog_mode1", g_settings.analog_mode1 );
	configfile.setInt32( "analog_mode2", g_settings.analog_mode2 );
	configfile.setInt32( "video_Format", g_settings.video_Format );
	configfile.setInt32( "video_43mode", g_settings.video_43mode );
	configfile.setInt32( "current_volume", g_settings.current_volume );
	configfile.setInt32( "channel_mode", g_settings.channel_mode );
	configfile.setInt32( "video_csync", g_settings.video_csync );

	configfile.setInt32( "fan_speed", g_settings.fan_speed);

	configfile.setInt32( "srs_enable", g_settings.srs_enable);
	configfile.setInt32( "srs_algo", g_settings.srs_algo);
	configfile.setInt32( "srs_ref_volume", g_settings.srs_ref_volume);
	configfile.setInt32( "srs_nmgr_enable", g_settings.srs_nmgr_enable);
	configfile.setInt32( "hdmi_dd", g_settings.hdmi_dd);
	configfile.setInt32( "spdif_dd", g_settings.spdif_dd);
	configfile.setInt32( "audio_spdif_dd", g_settings.audio_spdif_dd);
	configfile.setInt32( "audio_spdif_dts", g_settings.audio_spdif_dts);
	configfile.setInt32( "audio_spdif_aac", g_settings.audio_spdif_aac);
	configfile.setInt32( "avsync", g_settings.avsync);
	configfile.setInt32( "clockrec", g_settings.clockrec);
	configfile.setInt32( "video_dbdr", g_settings.video_dbdr);
	for(int i = 0; i < VIDEOMENU_VIDEOMODE_OPTION_COUNT; i++) {
		sprintf(cfg_key, "enabled_video_mode_%d", i);
		configfile.setInt32(cfg_key, g_settings.enabled_video_modes[i]);
	}
	configfile.setInt32( "cpufreq", g_settings.cpufreq);
	configfile.setInt32( "standby_cpufreq", g_settings.standby_cpufreq);

	configfile.setInt32( "make_hd_list", g_settings.make_hd_list);
	//fb-alphawerte f�r gtx
	configfile.setInt32( "gtx_alpha1", g_settings.gtx_alpha1 );
	configfile.setInt32( "gtx_alpha2", g_settings.gtx_alpha2 );

	//misc
	configfile.setInt32( "power_standby", g_settings.power_standby);
	configfile.setInt32( "rotor_swap", g_settings.rotor_swap);
	configfile.setInt32( "emlog", g_settings.emlog);
	configfile.setInt32( "zap_cycle", g_settings.zap_cycle );
	configfile.setInt32( "sms_channel", g_settings.sms_channel );
	configfile.setInt32( "hdd_fs", g_settings.hdd_fs);
	configfile.setInt32( "hdd_sleep", g_settings.hdd_sleep);
	configfile.setInt32( "hdd_noise", g_settings.hdd_noise);
	configfile.setBool("shutdown_real"        , g_settings.shutdown_real        );
	configfile.setBool("shutdown_real_rcdelay", g_settings.shutdown_real_rcdelay);
	configfile.setString("shutdown_count"           , g_settings.shutdown_count);
	configfile.setBool("infobar_sat_display"  , g_settings.infobar_sat_display  );
	configfile.setInt32("infobar_subchan_disp_pos"  , g_settings.infobar_subchan_disp_pos  );

	//audio
	configfile.setInt32( "audio_AnalogMode", g_settings.audio_AnalogMode );
	configfile.setBool("audio_DolbyDigital"   , g_settings.audio_DolbyDigital   );
	configfile.setInt32( "audio_avs_Control", g_settings.audio_avs_Control );
	configfile.setInt32( "audio_english", g_settings.audio_english );
	configfile.setString( "audio_PCMOffset", g_settings.audio_PCMOffset );

	//vcr
	configfile.setBool("vcr_AutoSwitch"       , g_settings.vcr_AutoSwitch       );

	//language
	configfile.setString("language", g_settings.language);
	configfile.setString("timezone", g_settings.timezone);
	// epg
	configfile.setBool("epg_save", g_settings.epg_save);
        configfile.setString("epg_cache_time"           ,g_settings.epg_cache );
        configfile.setString("epg_extendedcache_time"   ,g_settings.epg_extendedcache);
        configfile.setString("epg_old_events"           ,g_settings.epg_old_events );
        configfile.setString("epg_max_events"           ,g_settings.epg_max_events );
        configfile.setString("epg_dir"                  ,g_settings.epg_dir);

        // NTP-Server for sectionsd
        configfile.setString( "network_ntpserver", g_settings.network_ntpserver);
        configfile.setString( "network_ntprefresh", g_settings.network_ntprefresh);
        configfile.setBool( "network_ntpenable", g_settings.network_ntpenable);

	//widget settings
	configfile.setBool("widget_fade"          , g_settings.widget_fade          );

	//colors
	configfile.setInt32( "menu_Head_alpha", g_settings.menu_Head_alpha );
	configfile.setInt32( "menu_Head_red", g_settings.menu_Head_red );
	configfile.setInt32( "menu_Head_green", g_settings.menu_Head_green );
	configfile.setInt32( "menu_Head_blue", g_settings.menu_Head_blue );

	configfile.setInt32( "menu_Head_Text_alpha", g_settings.menu_Head_Text_alpha );
	configfile.setInt32( "menu_Head_Text_red", g_settings.menu_Head_Text_red );
	configfile.setInt32( "menu_Head_Text_green", g_settings.menu_Head_Text_green );
	configfile.setInt32( "menu_Head_Text_blue", g_settings.menu_Head_Text_blue );

	configfile.setInt32( "menu_Content_alpha", g_settings.menu_Content_alpha );
	configfile.setInt32( "menu_Content_red", g_settings.menu_Content_red );
	configfile.setInt32( "menu_Content_green", g_settings.menu_Content_green );
	configfile.setInt32( "menu_Content_blue", g_settings.menu_Content_blue );

	configfile.setInt32( "menu_Content_Text_alpha", g_settings.menu_Content_Text_alpha );
	configfile.setInt32( "menu_Content_Text_red", g_settings.menu_Content_Text_red );
	configfile.setInt32( "menu_Content_Text_green", g_settings.menu_Content_Text_green );
	configfile.setInt32( "menu_Content_Text_blue", g_settings.menu_Content_Text_blue );

	configfile.setInt32( "menu_Content_Selected_alpha", g_settings.menu_Content_Selected_alpha );
	configfile.setInt32( "menu_Content_Selected_red", g_settings.menu_Content_Selected_red );
	configfile.setInt32( "menu_Content_Selected_green", g_settings.menu_Content_Selected_green );
	configfile.setInt32( "menu_Content_Selected_blue", g_settings.menu_Content_Selected_blue );

	configfile.setInt32( "menu_Content_Selected_Text_alpha", g_settings.menu_Content_Selected_Text_alpha );
	configfile.setInt32( "menu_Content_Selected_Text_red", g_settings.menu_Content_Selected_Text_red );
	configfile.setInt32( "menu_Content_Selected_Text_green", g_settings.menu_Content_Selected_Text_green );
	configfile.setInt32( "menu_Content_Selected_Text_blue", g_settings.menu_Content_Selected_Text_blue );

	configfile.setInt32( "menu_Content_inactive_alpha", g_settings.menu_Content_inactive_alpha );
	configfile.setInt32( "menu_Content_inactive_red", g_settings.menu_Content_inactive_red );
	configfile.setInt32( "menu_Content_inactive_green", g_settings.menu_Content_inactive_green );
	configfile.setInt32( "menu_Content_inactive_blue", g_settings.menu_Content_inactive_blue );

	configfile.setInt32( "menu_Content_inactive_Text_alpha", g_settings.menu_Content_inactive_Text_alpha );
	configfile.setInt32( "menu_Content_inactive_Text_red", g_settings.menu_Content_inactive_Text_red );
	configfile.setInt32( "menu_Content_inactive_Text_green", g_settings.menu_Content_inactive_Text_green );
	configfile.setInt32( "menu_Content_inactive_Text_blue", g_settings.menu_Content_inactive_Text_blue );

	configfile.setInt32( "infobar_alpha", g_settings.infobar_alpha );
	configfile.setInt32( "infobar_red", g_settings.infobar_red );
	configfile.setInt32( "infobar_green", g_settings.infobar_green );
	configfile.setInt32( "infobar_blue", g_settings.infobar_blue );

	configfile.setInt32( "infobar_Text_alpha", g_settings.infobar_Text_alpha );
	configfile.setInt32( "infobar_Text_red", g_settings.infobar_Text_red );
	configfile.setInt32( "infobar_Text_green", g_settings.infobar_Text_green );
	configfile.setInt32( "infobar_Text_blue", g_settings.infobar_Text_blue );

	//network
	for(int i=0 ; i < NETWORK_NFS_NR_OF_ENTRIES ; i++) {
		sprintf(cfg_key, "network_nfs_ip_%d", i);
		configfile.setString( cfg_key, g_settings.network_nfs_ip[i] );
		sprintf(cfg_key, "network_nfs_dir_%d", i);
		configfile.setString( cfg_key, g_settings.network_nfs_dir[i] );
		sprintf(cfg_key, "network_nfs_local_dir_%d", i);
		configfile.setString( cfg_key, g_settings.network_nfs_local_dir[i] );
		sprintf(cfg_key, "network_nfs_automount_%d", i);
		configfile.setInt32( cfg_key, g_settings.network_nfs_automount[i]);
		sprintf(cfg_key, "network_nfs_type_%d", i);
		configfile.setInt32( cfg_key, g_settings.network_nfs_type[i]);
		sprintf(cfg_key,"network_nfs_username_%d", i);
		configfile.setString( cfg_key, g_settings.network_nfs_username[i] );
		sprintf(cfg_key, "network_nfs_password_%d", i);
		configfile.setString( cfg_key, g_settings.network_nfs_password[i] );
		sprintf(cfg_key, "network_nfs_mount_options1_%d", i);
		configfile.setString( cfg_key, g_settings.network_nfs_mount_options1[i]);
		sprintf(cfg_key, "network_nfs_mount_options2_%d", i);
		configfile.setString( cfg_key, g_settings.network_nfs_mount_options2[i]);
		sprintf(cfg_key, "network_nfs_mac_%d", i);
		configfile.setString( cfg_key, g_settings.network_nfs_mac[i]);
	}
	configfile.setString( "network_nfs_audioplayerdir", g_settings.network_nfs_audioplayerdir);
	configfile.setString( "network_nfs_picturedir", g_settings.network_nfs_picturedir);
	configfile.setString( "network_nfs_moviedir", g_settings.network_nfs_moviedir);
	configfile.setString( "network_nfs_recordingdir", g_settings.network_nfs_recordingdir);
	configfile.setString( "timeshiftdir", g_settings.timeshiftdir);
	configfile.setBool  ("filesystem_is_utf8"                 , g_settings.filesystem_is_utf8             );

	//recording (server + vcr)
	configfile.setInt32 ("recording_type",                      g_settings.recording_type);
	configfile.setBool  ("recording_stopplayback"             , g_settings.recording_stopplayback         );
	configfile.setBool  ("recording_stopsectionsd"            , g_settings.recording_stopsectionsd        );
	configfile.setString("recording_server_ip",                 g_settings.recording_server_ip);
	configfile.setString("recording_server_port",               g_settings.recording_server_port);
	configfile.setInt32 ("recording_server_wakeup",             g_settings.recording_server_wakeup);
	configfile.setString("recording_server_mac",                g_settings.recording_server_mac);
	configfile.setInt32 ("recording_vcr_no_scart",              g_settings.recording_vcr_no_scart);
	configfile.setString("recording_splitsize",                 g_settings.recording_splitsize);
	configfile.setBool  ("recordingmenu.use_o_sync"           , g_settings.recording_use_o_sync           );
	configfile.setBool  ("recordingmenu.use_fdatasync"        , g_settings.recording_use_fdatasync        );

	configfile.setInt32 ("recording_audio_pids_default"       , g_settings.recording_audio_pids_default);
	configfile.setBool  ("recording_zap_on_announce"          , g_settings.recording_zap_on_announce      );

	configfile.setBool  ("recordingmenu.stream_vtxt_pid"      , g_settings.recording_stream_vtxt_pid      );
	configfile.setBool  ("recordingmenu.stream_pmt_pid"       , g_settings.recording_stream_pmt_pid      );
	configfile.setString("recordingmenu.ringbuffers"          , g_settings.recording_ringbuffers);
	configfile.setInt32 ("recording_choose_direct_rec_dir"    , g_settings.recording_choose_direct_rec_dir);
	configfile.setBool  ("recording_epg_for_filename"         , g_settings.recording_epg_for_filename     );
	configfile.setBool  ("recording_save_in_channeldir"       , g_settings.recording_save_in_channeldir     );
	configfile.setBool  ("recording_in_spts_mode"             , g_settings.recording_in_spts_mode         );

	//streaming
	configfile.setInt32 ( "streaming_type", g_settings.streaming_type );
	configfile.setString( "streaming_server_ip", g_settings.streaming_server_ip );
	configfile.setString( "streaming_server_port", g_settings.streaming_server_port );
	configfile.setString( "streaming_server_cddrive", g_settings.streaming_server_cddrive );
	configfile.setString ( "streaming_videorate", g_settings.streaming_videorate );
	configfile.setString ( "streaming_audiorate", g_settings.streaming_audiorate );
	configfile.setString( "streaming_server_startdir", g_settings.streaming_server_startdir );
	configfile.setInt32 ( "streaming_transcode_audio", g_settings.streaming_transcode_audio );
	configfile.setInt32 ( "streaming_force_avi_rawaudio", g_settings.streaming_force_avi_rawaudio );
	configfile.setInt32 ( "streaming_force_transcode_video", g_settings.streaming_force_transcode_video );
	configfile.setInt32 ( "streaming_transcode_video_codec", g_settings.streaming_transcode_video_codec );
	configfile.setInt32 ( "streaming_resolution", g_settings.streaming_resolution );

	// default plugin for movieplayer
	configfile.setString ( "movieplayer_plugin", g_settings.movieplayer_plugin );
	configfile.setString ( "onekey_plugin", g_settings.onekey_plugin );

	configfile.setInt32 ( "streaming_resolution", g_settings.streaming_resolution );

	configfile.setInt32( "rf_subcarrier", g_settings.rf_subcarrier);
	configfile.setInt32( "rf_soundenable", g_settings.rf_soundenable);
	configfile.setInt32( "rf_channel", g_settings.rf_channel);
	configfile.setInt32( "rf_finetune", g_settings.rf_finetune);
	configfile.setInt32( "rf_standby", g_settings.rf_standby);

	//rc-key configuration
	configfile.setInt32( "key_tvradio_mode", g_settings.key_tvradio_mode );

	configfile.setInt32( "key_channelList_pageup", g_settings.key_channelList_pageup );
	configfile.setInt32( "key_channelList_pagedown", g_settings.key_channelList_pagedown );
	configfile.setInt32( "key_channelList_cancel", g_settings.key_channelList_cancel );
	configfile.setInt32( "key_channelList_sort", g_settings.key_channelList_sort );
	configfile.setInt32( "key_channelList_addrecord", g_settings.key_channelList_addrecord );
	configfile.setInt32( "key_channelList_addremind", g_settings.key_channelList_addremind );

	configfile.setInt32( "key_quickzap_up", g_settings.key_quickzap_up );
	configfile.setInt32( "key_quickzap_down", g_settings.key_quickzap_down );
	configfile.setInt32( "key_bouquet_up", g_settings.key_bouquet_up );
	configfile.setInt32( "key_bouquet_down", g_settings.key_bouquet_down );
	configfile.setInt32( "key_subchannel_up", g_settings.key_subchannel_up );
	configfile.setInt32( "key_subchannel_down", g_settings.key_subchannel_down );
	configfile.setInt32( "key_zaphistory", g_settings.key_zaphistory );
	configfile.setInt32( "key_lastchannel", g_settings.key_lastchannel );

	configfile.setInt32( "key_list_start", g_settings.key_list_start );
	configfile.setInt32( "key_list_end", g_settings.key_list_end );
	configfile.setInt32( "menu_left_exit", g_settings.menu_left_exit );
	configfile.setInt32( "audio_run_player", g_settings.audio_run_player );
	configfile.setInt32( "key_click", g_settings.key_click );
	configfile.setInt32( "timeshift_pause", g_settings.timeshift_pause );
	configfile.setInt32( "temp_timeshift", g_settings.temp_timeshift );
	configfile.setInt32( "auto_timeshift", g_settings.auto_timeshift );
	configfile.setInt32( "auto_delete", g_settings.auto_delete );
	configfile.setInt32( "record_hours", g_settings.record_hours );
	configfile.setInt32( "mpkey.rewind", g_settings.mpkey_rewind );
	configfile.setInt32( "mpkey.forward", g_settings.mpkey_forward );
	configfile.setInt32( "mpkey.pause", g_settings.mpkey_pause );
	configfile.setInt32( "mpkey.stop", g_settings.mpkey_stop );
	configfile.setInt32( "mpkey.play", g_settings.mpkey_play );
	configfile.setInt32( "mpkey.audio", g_settings.mpkey_audio );
	configfile.setInt32( "mpkey.subt", g_settings.mpkey_subt );
	configfile.setInt32( "mpkey.time", g_settings.mpkey_time );
	configfile.setInt32( "mpkey.bookmark", g_settings.mpkey_bookmark );
	configfile.setInt32( "mpkey.plugin", g_settings.mpkey_plugin );
	configfile.setInt32( "key_timeshift", g_settings.key_timeshift );
	configfile.setInt32( "key_plugin", g_settings.key_plugin );
	configfile.setInt32( "key_unlock", g_settings.key_unlock );
//printf("set: key_unlock =============== %d\n", g_settings.key_unlock);
	configfile.setInt32( "cacheTXT", g_settings.cacheTXT );
	configfile.setInt32( "minimode", g_settings.minimode );
	configfile.setInt32( "mode_clock", g_settings.mode_clock );
	configfile.setBool("virtual_zap_mode", g_settings.virtual_zap_mode);
	configfile.setBool("spectrum", g_settings.spectrum);
	configfile.setBool("channellist_epgtext_align_right", g_settings.channellist_epgtext_align_right);
	configfile.setBool("channellist_extended"                 , g_settings.channellist_extended);
	configfile.setString( "repeat_blocker", g_settings.repeat_blocker );
	configfile.setString( "repeat_genericblocker", g_settings.repeat_genericblocker );
	configfile.setBool  ( "audiochannel_up_down_enable", g_settings.audiochannel_up_down_enable );

	//screen configuration
	configfile.setInt32( "screen_StartX", g_settings.screen_StartX );
	configfile.setInt32( "screen_StartY", g_settings.screen_StartY );
	configfile.setInt32( "screen_EndX", g_settings.screen_EndX );
	configfile.setInt32( "screen_EndY", g_settings.screen_EndY );
	configfile.setInt32( "screen_width", g_settings.screen_width);
	configfile.setInt32( "screen_height", g_settings.screen_height);

	//Software-update
	configfile.setInt32 ("softupdate_mode"          , g_settings.softupdate_mode          );
	configfile.setString("softupdate_url_file"      , g_settings.softupdate_url_file      );
#if 1 //FIXME why was commented ??
	configfile.setString("softupdate_proxyserver"   , g_settings.softupdate_proxyserver   );
	configfile.setString("softupdate_proxyusername" , g_settings.softupdate_proxyusername );
	configfile.setString("softupdate_proxypassword" , g_settings.softupdate_proxypassword );
#endif
	configfile.setString("update_dir", g_settings.update_dir);
	configfile.setString("font_file", g_settings.font_file);
	//BouquetHandling
	configfile.setInt32( "bouquetlist_mode", g_settings.bouquetlist_mode );

	//parentallock
	configfile.setInt32( "parentallock_prompt", g_settings.parentallock_prompt );
	configfile.setInt32( "parentallock_lockage", g_settings.parentallock_lockage );
	configfile.setString( "parentallock_pincode", g_settings.parentallock_pincode );

	//timing
	for (int i = 0; i < TIMING_SETTING_COUNT; i++)
		configfile.setInt32(locale_real_names[timing_setting_name[i]], g_settings.timing[i]);

	for (int i = 0; i < LCD_SETTING_COUNT; i++)
		configfile.setInt32(lcd_setting[i].name, g_settings.lcd_setting[i]);
	configfile.setString("lcd_dim_time", g_settings.lcd_setting_dim_time);
	configfile.setString("lcd_dim_brightness", g_settings.lcd_setting_dim_brightness);

	//Picture-Viewer
	configfile.setString( "picviewer_slide_time", g_settings.picviewer_slide_time );
	configfile.setInt32( "picviewer_scaling", g_settings.picviewer_scaling );
	configfile.setString( "picviewer_decode_server_ip", g_settings.picviewer_decode_server_ip );
	configfile.setString( "picviewer_decode_server_port", g_settings.picviewer_decode_server_port);

	//Audio-Player
	configfile.setInt32( "audioplayer_display", g_settings.audioplayer_display );
	configfile.setInt32( "audioplayer_follow", g_settings.audioplayer_follow );
	configfile.setString( "audioplayer_screensaver", g_settings.audioplayer_screensaver );
	configfile.setInt32( "audioplayer_highprio", g_settings.audioplayer_highprio );
	configfile.setInt32( "audioplayer_select_title_by_name", g_settings.audioplayer_select_title_by_name );
	configfile.setInt32( "audioplayer_repeat_on", g_settings.audioplayer_repeat_on );
	configfile.setInt32( "audioplayer_show_playlist", g_settings.audioplayer_show_playlist );
	configfile.setInt32( "audioplayer_enable_sc_metadata", g_settings.audioplayer_enable_sc_metadata );

	//Filebrowser
	configfile.setInt32("filebrowser_showrights", g_settings.filebrowser_showrights);
	configfile.setInt32("filebrowser_sortmethod", g_settings.filebrowser_sortmethod);
	configfile.setBool("filebrowser_denydirectoryleave", g_settings.filebrowser_denydirectoryleave);

        // USERMENU
        //---------------------------------------
        char txt1[81];
        char txt2[81];
        for(int button = 0; button < SNeutrinoSettings::BUTTON_MAX; button++) {
                snprintf(txt1,80,"usermenu_tv_%s_text",usermenu_button_def[button]);
                txt1[80] = 0; // terminate for sure
                configfile.setString(txt1,g_settings.usermenu_text[button]);

                char* txt2ptr = txt2;
                snprintf(txt1,80,"usermenu_tv_%s",usermenu_button_def[button]);
                for(int pos = 0; pos < SNeutrinoSettings::ITEM_MAX; pos++) {
                        if( g_settings.usermenu[button][pos] != 0) {
                                if(pos != 0)
                                        *txt2ptr++ = ',';
                                txt2ptr += snprintf(txt2ptr,80,"%d",g_settings.usermenu[button][pos]);
                        }
                }
                configfile.setString(txt1,txt2);
        }

	configfile.setInt32("bigFonts", g_settings.bigFonts);
#if 0
	configfile.setInt32("pip_x", g_settings.pip_x);
	configfile.setInt32("pip_y", g_settings.pip_y);
	configfile.setInt32("pip_width", g_settings.pip_width);
	configfile.setInt32("pip_height", g_settings.pip_height);
#endif
	if(strcmp(fname, NEUTRINO_SETTINGS_FILE))
		configfile.saveConfig(fname);

	else if (configfile.getModifiedFlag())
	{
		configfile.saveConfig(fname);
		configfile.setModifiedFlag(false);
	}
}

/**************************************************************************************
*          CNeutrinoApp -  firstChannel, get the initial channel                      *
**************************************************************************************/
void CNeutrinoApp::firstChannel()
{
	g_Zapit->getLastChannel(firstchannel.channelNumber, firstchannel.mode);
}

/**************************************************************************************
*          CNeutrinoApp -  channelsInit, get the Channellist from daemon              *
**************************************************************************************/
#include <zapit/channel.h>
#include <zapit/bouquets.h>

#define TIMER_START()                                                                   \
        struct timeval tv, tv2;                                                         \
        unsigned int msec;                                                              \
        gettimeofday(&tv, NULL)

#define TIMER_STOP(label)                                                               \
        gettimeofday(&tv2, NULL);                                                       \
        msec = ((tv2.tv_sec - tv.tv_sec) * 1000) + ((tv2.tv_usec - tv.tv_usec) / 1000); \
        printf("%s: %u msec\n", label, msec)

extern tallchans allchans;
extern CBouquetManager *g_bouquetManager;

void CNeutrinoApp::channelsInit(bool bOnly)
{
	printf("[neutrino] Creating channels lists...\n");
	TIMER_START();

#if 1
	if(!reloadhintBox)
		reloadhintBox = new CHintBox(LOCALE_MESSAGEBOX_INFO, g_Locale->getText(LOCALE_SERVICEMENU_RELOAD_HINT));
	reloadhintBox->paint();
#endif

	const char * fav_bouquetname = g_Locale->getText(LOCALE_FAVORITES_BOUQUETNAME);
	if(g_bouquetManager->existsUBouquet(fav_bouquetname, true) == -1)
		g_bouquetManager->addBouquet(fav_bouquetname, true, true);

	if(TVallList) delete TVallList;
	if(RADIOallList) delete RADIOallList;
	if(TVbouquetList) delete TVbouquetList;
	if(TVsatList) delete TVsatList;
	if(TVfavList) delete TVfavList;
	if(RADIObouquetList) delete RADIObouquetList;
	if(RADIOsatList) delete RADIOsatList;
	if(RADIOfavList) delete RADIOfavList;

	if(TVchannelList) delete TVchannelList;
	if(RADIOchannelList) delete RADIOchannelList;

	TVchannelList = new CChannelList(g_Locale->getText(LOCALE_CHANNELLIST_HEAD));
	RADIOchannelList = new CChannelList(g_Locale->getText(LOCALE_CHANNELLIST_HEAD));

	TVbouquetList = new CBouquetList(g_Locale->getText(LOCALE_CHANNELLIST_PROVS));
	TVbouquetList->orgChannelList = TVchannelList;
	TVsatList = new CBouquetList(g_Locale->getText(LOCALE_CHANNELLIST_SATS));
	TVsatList->orgChannelList = TVchannelList;
	TVfavList = new CBouquetList(g_Locale->getText(LOCALE_CHANNELLIST_FAVS));
	TVfavList->orgChannelList = TVchannelList;

	RADIObouquetList = new CBouquetList(g_Locale->getText(LOCALE_CHANNELLIST_PROVS));
	RADIObouquetList->orgChannelList = RADIOchannelList;
	RADIOsatList = new CBouquetList(g_Locale->getText(LOCALE_CHANNELLIST_SATS));
	RADIOsatList->orgChannelList = RADIOchannelList;
	RADIOfavList = new CBouquetList(g_Locale->getText(LOCALE_CHANNELLIST_FAVS));
	RADIOfavList->orgChannelList = RADIOchannelList;

	uint32_t i;
	i = 1;

	//CBouquet* tmp = TVfavList->addBouquet("HD");//FIXME locale
	CBouquet* hdBouquet;
	if(g_settings.make_hd_list)
		hdBouquet = new CBouquet(0, (char *) "HD", 0);

	int tvi = 0, ri = 0, hi = 0;
	for (tallchans_iterator it = allchans.begin(); it != allchans.end(); it++) {
		if (it->second.getServiceType() == ST_DIGITAL_TELEVISION_SERVICE) {
			TVchannelList->putChannel(&(it->second));
			tvi++;
#if 1
			if(g_settings.make_hd_list)
				if(it->second.isHD()) {
					//printf("HD channel: %d %s sid %x\n", it->second.getSatellitePosition(), it->second.getName().c_str(), it->second.getServiceId());
					hdBouquet->channelList->addChannel(&(it->second));
					hi++;
				}
#endif
		}
		else if (it->second.getServiceType() == ST_DIGITAL_RADIO_SOUND_SERVICE) {
			RADIOchannelList->putChannel(&(it->second));
			ri++;
		}
	}
	if(hi)
		hdBouquet->channelList->SortSat();

	printf("[neutrino] got %d TV (%d is HD) and %d RADIO channels\n", tvi, hi, ri); fflush(stdout);

	CBouquet* tmp;

	TVallList = new CBouquetList(g_Locale->getText(LOCALE_CHANNELLIST_HEAD));
	tmp = TVallList->addBouquet(g_Locale->getText(LOCALE_CHANNELLIST_HEAD));
	*(tmp->channelList) = *TVchannelList;
	tmp->channelList->SortAlpha();
	TVallList->orgChannelList = TVchannelList;

	RADIOallList = new CBouquetList(g_Locale->getText(LOCALE_CHANNELLIST_HEAD));
	tmp = RADIOallList->addBouquet(g_Locale->getText(LOCALE_CHANNELLIST_HEAD));
	*(tmp->channelList) = *RADIOchannelList;
	tmp->channelList->SortAlpha();
	RADIOallList->orgChannelList = RADIOchannelList;

	int bnum;
	sat_iterator_t sit;
	for(sit = satellitePositions.begin(); sit != satellitePositions.end(); sit++) {
		tvi = 0, ri = 0;
		CBouquet* tmp1 = TVsatList->addBouquet(sit->second.name.c_str());
		CBouquet* tmp2 = RADIOsatList->addBouquet(sit->second.name.c_str());

		for (tallchans_iterator it = allchans.begin(); it != allchans.end(); it++) {
			if(it->second.getSatellitePosition() == sit->first) {
				if (it->second.getServiceType() == ST_DIGITAL_TELEVISION_SERVICE) {
					tmp1->channelList->addChannel(&(it->second));
					tvi++;
				}
				else if (it->second.getServiceType() == ST_DIGITAL_RADIO_SOUND_SERVICE) {
					tmp2->channelList->addChannel(&(it->second));
					ri++;
				}
			}
		}
		if(tvi)
			tmp1->channelList->SortAlpha();
		else
			TVsatList->deleteBouquet(tmp1);
		if(ri)
			tmp2->channelList->SortAlpha();
		else
			RADIOsatList->deleteBouquet(tmp2);
		if(tvi || ri)
			printf("[neutrino] created %s bouquet with %d TV and %d RADIO channels\n", sit->second.name.c_str(), tvi, ri);
	}

	bnum = 0;
	for (i = 0; i < g_bouquetManager->Bouquets.size(); i++) {
		//if (!g_bouquetManager->Bouquets[i]->bHidden && (g_bouquetManager->Bouquets[i]->bUser || !g_bouquetManager->Bouquets[i]->tvChannels.empty() ))
		if (!g_bouquetManager->Bouquets[i]->bHidden && !g_bouquetManager->Bouquets[i]->tvChannels.empty())
		{
			CBouquet* tmp;
			if(g_bouquetManager->Bouquets[i]->bUser) 
				tmp = TVfavList->addBouquet(g_bouquetManager->Bouquets[i]);
			else
				tmp = TVbouquetList->addBouquet(g_bouquetManager->Bouquets[i]);

			ZapitChannelList* channels = &(g_bouquetManager->Bouquets[i]->tvChannels);
			tmp->channelList->setSize(channels->size());
			for(int j = 0; j < (int) channels->size(); j++) {
				tmp->channelList->addChannel((*channels)[j]);
			}
			bnum++;
		}
	}
	printf("[neutrino] got %d TV bouquets\n", bnum); fflush(stdout);

	if(g_settings.make_hd_list)
		TVfavList->Bouquets.push_back(hdBouquet);

	bnum = 0;
	for (i = 0; i < g_bouquetManager->Bouquets.size(); i++) {
		//if (!g_bouquetManager->Bouquets[i]->bHidden && (g_bouquetManager->Bouquets[i]->bUser || !g_bouquetManager->Bouquets[i]->radioChannels.empty() ))
		if (!g_bouquetManager->Bouquets[i]->bHidden && !g_bouquetManager->Bouquets[i]->radioChannels.empty() )
		{
			CBouquet* tmp;
			if(g_bouquetManager->Bouquets[i]->bUser) 
				tmp = RADIOfavList->addBouquet(g_bouquetManager->Bouquets[i]->Name.c_str(), i, g_bouquetManager->Bouquets[i]->bLocked);
			else
				tmp = RADIObouquetList->addBouquet(g_bouquetManager->Bouquets[i]->Name.c_str(), i, g_bouquetManager->Bouquets[i]->bLocked);

			ZapitChannelList* channels = &(g_bouquetManager->Bouquets[i]->radioChannels);
			tmp->channelList->setSize(channels->size());
			for(int j = 0; j < (int) channels->size(); j++) {
				tmp->channelList->addChannel((*channels)[j]);
			}
			bnum++;
		}
	}
	printf("[neutrino] got %d RADIO bouquets\n", bnum); fflush(stdout);
	TIMER_STOP("[neutrino] took");

	SetChannelMode(g_settings.channel_mode);

	dprintf(DEBUG_DEBUG, "\nAll bouquets-channels received\n");
#ifdef DEBUG
	struct mallinfo myinfo = mallinfo();
	printf("[neutrino] total memory allocated by malloc, in bytes: %d (%dkb), chunks %d\n",
			myinfo.arena, myinfo.arena / 1024, myinfo.uordblks);
#endif

	reloadhintBox->hide();
}

void CNeutrinoApp::SetChannelMode(int newmode)
{
printf("CNeutrinoApp::SetChannelMode %d\n", newmode);
	if(mode == mode_radio)
		channelList = RADIOchannelList;
	else
		channelList = TVchannelList;

	switch(newmode) {
		case LIST_MODE_FAV:
			if(mode == mode_radio) {
				bouquetList = RADIOfavList;
			} else {
				bouquetList = TVfavList;
			}
			break;
		case LIST_MODE_SAT:
			if(mode == mode_radio) {
				bouquetList = RADIOsatList;
			} else {
				bouquetList = TVsatList;
			}
			break;
		case LIST_MODE_ALL:
			if(mode == mode_radio) {
				bouquetList = RADIOallList;
			} else {
				bouquetList = TVallList;
			}
			break;
		default:
		case LIST_MODE_PROV:
			if(mode == mode_radio) {
				bouquetList = RADIObouquetList;
			} else {
				bouquetList = TVbouquetList;
			}
			break;
	}
	g_settings.channel_mode = newmode;
}

/**************************************************************************************
*          CNeutrinoApp -  run, the main runloop                                      *
**************************************************************************************/
extern int cnxt_debug;
extern int sections_debug;
extern int zapit_debug;

void CNeutrinoApp::CmdParser(int argc, char **argv)
{
        global_argv = new char *[argc+1];
        for (int i = 0; i < argc; i++)
                global_argv[i] = argv[i];
        global_argv[argc] = NULL;

	softupdate = false;
	fromflash = false;

	font.name = NULL;

	for(int x=1; x<argc; x++) {
		if ((!strcmp(argv[x], "-u")) || (!strcmp(argv[x], "--enable-update"))) {
			dprintf(DEBUG_NORMAL, "Software update enabled\n");
			softupdate = true;
			allow_flash = 1;
		}
		else if ((!strcmp(argv[x], "-f")) || (!strcmp(argv[x], "--enable-flash"))) {
			dprintf(DEBUG_NORMAL, "enable flash\n");
			fromflash = true;
		}
		else if ((!strcmp(argv[x], "-v")) || (!strcmp(argv[x], "--verbose"))) {
			int dl = atoi(argv[x+ 1]);
			dprintf(DEBUG_NORMAL, "set debuglevel: %d\n", dl);
			setDebugLevel(dl);
			x++;
		}
		else if ((!strcmp(argv[x], "-xd"))) {
			cnxt_debug = 1;
			x++;
		}
		else if ((!strcmp(argv[x], "-sd"))) {
			sections_debug = 1;
			x++;
		}
		else if ((!strcmp(argv[x], "-zd"))) {
			zapit_debug = 1;
			x++;
		}
		else if (!strcmp(argv[x], "-r")) {
			if (x < argc)
				xres = atoi(argv[x++]);
			if (x < argc)
				yres = atoi(argv[x++]);
		} else {
			dprintf(DEBUG_NORMAL, "Usage: neutrino [-u | --enable-update] [-f | --enable-flash] [-v | --verbose 0..3]\n");
			exit(1);
		}
	}
}

/**************************************************************************************
*          CNeutrinoApp -  setup the framebuffer                                      *
**************************************************************************************/
void CNeutrinoApp::SetupFrameBuffer()
{
	frameBuffer->init();

#ifndef AZBOX_GEN_1
	if(frameBuffer->setMode(720, 576, 8 * sizeof(fb_pixel_t))) {
		dprintf(DEBUG_NORMAL, "Error while setting framebuffer mode\n");
		exit(-1);
	}
#endif

#ifdef AZBOX_GEN_1
		frameBuffer->resize(g_settings.video_Mode);
#endif

	//make 1..8 transparent for dummy painting
	//for(int count =0;count<256;count++)
	for(int count =0;count<8;count++)
		frameBuffer->paletteSetColor(count, 0x000000, 0xffff);
	frameBuffer->paletteSet();
	frameBuffer->ClearFrameBuffer();
}

/**************************************************************************************
*          CNeutrinoApp -  setup fonts                                                *
**************************************************************************************/
#if 0
const neutrino_font_descr_struct predefined_font[2] =
{
	{"Micron"            , {FONTDIR "/micron.ttf"        , FONTDIR "/micron_bold.ttf", FONTDIR "/micron_italic.ttf"}, 0},
	{"MD King KhammuRabi", {FONTDIR "/md_khmurabi_10.ttf", NULL                      , NULL                        }, 0}
};
const char* predefined_lcd_font[2][6] =
{
	{FONTDIR "/12.pcf.gz", "Fix12", FONTDIR "/14B.pcf.gz", "Fix14", FONTDIR "/15B.pcf.gz", "Fix15"},
	{FONTDIR "/md_khmurabi_10.ttf", "MD King KhammuRabi", NULL, NULL,  NULL, NULL}
};
#endif

void CNeutrinoApp::SetupFonts()
{
	const char * style[3];

	if (g_fontRenderer != NULL)
		delete g_fontRenderer;

	g_fontRenderer = new FBFontRenderClass(xres, yres);

	if(font.filename != NULL)
		free((void *)font.filename);

	printf("[neutrino] settings font file %s\n", g_settings.font_file);

	if(access(g_settings.font_file, F_OK)) {
		font.filename = strdup(FONTDIR"/neutrino.ttf");
		strcpy(g_settings.font_file, font.filename);
	}
	else
		font.filename = strdup(g_settings.font_file);

	style[0] = g_fontRenderer->AddFont(font.filename);

	if(font.name != NULL)
		free((void *)font.name);

	font.name = strdup(g_fontRenderer->getFamily(font.filename).c_str());

	printf("[neutrino] font family %s\n", font.name);

	style[1] = "Bold Regular";

	g_fontRenderer->AddFont(font.filename, true);  // make italics
	style[2] = "Italic";

	for (int i = 0; i < FONT_TYPE_COUNT; i++)
	{
		if(g_Font[i]) delete g_Font[i];
		g_Font[i] = g_fontRenderer->getFont(font.name, style[neutrino_font[i].style], configfile.getInt32(locale_real_names[neutrino_font[i].name], neutrino_font[i].defaultsize) + neutrino_font[i].size_offset * font.size_offset);
	}
	g_SignalFont = g_fontRenderer->getFont(font.name, style[signal_font.style], signal_font.defaultsize + signal_font.size_offset * font.size_offset);
}

/**************************************************************************************
*          CNeutrinoApp -  setup the menu timouts                                     *
**************************************************************************************/
void CNeutrinoApp::SetupTiming()
{
	for (int i = 0; i < TIMING_SETTING_COUNT; i++)
		sprintf(g_settings.timing_string[i], "%d", g_settings.timing[i]);
}

CAudioPlayerGui * audioPlayer;

bool sectionsd_getActualEPGServiceKey(const t_channel_id uniqueServiceKey, CEPGData * epgdata);
bool sectionsd_getEPGid(const event_id_t epgID, const time_t startzeit, CEPGData * epgdata);

int startAutoRecord(bool addTimer)
{
	CTimerd::RecordingInfo eventinfo;
	if(CNeutrinoApp::getInstance()->recordingstatus || !CVCRControl::getInstance()->isDeviceRegistered() || (g_settings.recording_type != RECORDING_FILE))
		return 0;

	CZapitClient::CCurrentServiceInfo si = g_Zapit->getCurrentServiceInfo();

	//eventinfo.channel_id = g_Zapit->getCurrentServiceID();
	eventinfo.channel_id = live_channel_id;
	CEPGData		epgData;
	//if (g_Sectionsd->getActualEPGServiceKey(g_RemoteControl->current_channel_id&0xFFFFFFFFFFFFULL, &epgData ))
	if (sectionsd_getActualEPGServiceKey(live_channel_id&0xFFFFFFFFFFFFULL, &epgData ))
	{
		eventinfo.epgID = epgData.eventID;
		eventinfo.epg_starttime = epgData.epg_times.startzeit;
		strncpy(eventinfo.epgTitle, epgData.title.c_str(), EPG_TITLE_MAXLEN-1);
		eventinfo.epgTitle[EPG_TITLE_MAXLEN-1]=0;
	}
	else {
		eventinfo.epgID = 0;
		eventinfo.epg_starttime = 0;
		strcpy(eventinfo.epgTitle, "");
	}
	eventinfo.apids = TIMERD_APIDS_CONF;
printf("*********************************** startAutoRecord: dir %s\n", timeshiftDir);
	(static_cast<CVCRControl::CFileDevice*>(recordingdevice))->Directory = timeshiftDir;

	autoshift = 1;
	CNeutrinoApp::getInstance()->recordingstatus = 1;
	if(CVCRControl::getInstance()->Record(&eventinfo)==false) {
		CNeutrinoApp::getInstance()->recordingstatus = 0;
		autoshift = 0;
	}
	else if (addTimer) {
		time_t now = time(NULL);
		CNeutrinoApp::getInstance()->recording_id = g_Timerd->addImmediateRecordTimerEvent(eventinfo.channel_id, now, now+g_settings.record_hours*60*60, eventinfo.epgID, eventinfo.epg_starttime, eventinfo.apids);
	}
//printf("startAutoRecord: recording_id = %d\n", CNeutrinoApp::getInstance()->recording_id);
	return 0;
}
void stopAutoRecord()
{
	if(autoshift && CNeutrinoApp::getInstance()->recordingstatus) {
printf("stopAutoRecord: autoshift, recordingstatus %d, stopping ...\n", CNeutrinoApp::getInstance()->recordingstatus);
		CVCRControl::getInstance()->Stop();
		autoshift = false;
		if(CNeutrinoApp::getInstance()->recording_id) {
//printf("stopAutoRecord: recording_id = %d\n", CNeutrinoApp::getInstance()->recording_id);
			g_Timerd->stopTimerEvent(CNeutrinoApp::getInstance()->recording_id);
			CNeutrinoApp::getInstance()->recording_id = 0;
		}
	} else if(shift_timer)  {
		g_RCInput->killTimer (shift_timer);
		shift_timer = 0;
	}
}

bool CNeutrinoApp::doGuiRecord(char * preselectedDir, bool addTimer)
{
	CTimerd::RecordingInfo eventinfo;
	bool refreshGui = false;
	if(CVCRControl::getInstance()->isDeviceRegistered()) {
		if(autoshift) {
			stopAutoRecord();
		}
		if(recordingstatus == 1) {
			puts("[neutrino.cpp] executing " NEUTRINO_RECORDING_START_SCRIPT ".");
			if (system(NEUTRINO_RECORDING_START_SCRIPT) != 0)
				perror(NEUTRINO_RECORDING_START_SCRIPT " failed");

			CZapitClient::CCurrentServiceInfo si = g_Zapit->getCurrentServiceInfo();

			//eventinfo.channel_id = g_Zapit->getCurrentServiceID();
			eventinfo.channel_id = live_channel_id;
			CEPGData		epgData;
			//if (g_Sectionsd->getActualEPGServiceKey(g_RemoteControl->current_channel_id&0xFFFFFFFFFFFFULL, &epgData ))
			if (sectionsd_getActualEPGServiceKey(live_channel_id&0xFFFFFFFFFFFFULL, &epgData ))
			{
				eventinfo.epgID = epgData.eventID;
				eventinfo.epg_starttime = epgData.epg_times.startzeit;
				strncpy(eventinfo.epgTitle, epgData.title.c_str(), EPG_TITLE_MAXLEN-1);
				eventinfo.epgTitle[EPG_TITLE_MAXLEN-1]=0;
			}
			else {
				eventinfo.epgID = 0;
				eventinfo.epg_starttime = 0;
				strcpy(eventinfo.epgTitle, "");
			}
			eventinfo.apids = TIMERD_APIDS_CONF;
			bool doRecord = true;
			if (g_settings.recording_type == RECORDING_FILE) {
				strcpy(recDir, (preselectedDir != NULL) ? preselectedDir : g_settings.network_nfs_recordingdir);
				// data == NULL -> called after stream problem so do not show a dialog again
				// but which dir has been chosen?
				if(preselectedDir == NULL && (g_settings.recording_choose_direct_rec_dir == 2)) {
					CFileBrowser b;
					b.Dir_Mode=true;
					refreshGui = true;
					if (b.exec(g_settings.network_nfs_recordingdir)) {
						strcpy(recDir, b.getSelectedFile()->Name.c_str());
					}
					else doRecord = false;
				}
				else if(preselectedDir == NULL && (g_settings.recording_choose_direct_rec_dir == 1)) {
					int userDecision = -1;

					CMountChooser recDirs(LOCALE_TIMERLIST_RECORDING_DIR,NEUTRINO_ICON_SETTINGS,&userDecision,NULL,g_settings.network_nfs_recordingdir);
					if (recDirs.hasItem()) {
						recDirs.exec(NULL,"");
						refreshGui = true;
						if (userDecision != -1) {
							//recDir = g_settings.network_nfs_local_dir[userDecision];
							strcpy(recDir, g_settings.network_nfs_local_dir[userDecision]);
							if (!CFSMounter::isMounted(g_settings.network_nfs_local_dir[userDecision]))
							{
								CFSMounter::MountRes mres =
									CFSMounter::mount(g_settings.network_nfs_ip[userDecision].c_str(),
											  g_settings.network_nfs_dir[userDecision],
											  g_settings.network_nfs_local_dir[userDecision],
											  (CFSMounter::FSType) g_settings.network_nfs_type[userDecision],
											  g_settings.network_nfs_username[userDecision],
											  g_settings.network_nfs_password[userDecision],
											  g_settings.network_nfs_mount_options1[userDecision],
											  g_settings.network_nfs_mount_options2[userDecision]);
								if (mres != CFSMounter::MRES_OK) {
									//recDir = g_settings.network_nfs_local_dir[userDecision];
									//strcpy(recDir, g_settings.network_nfs_local_dir[userDecision]);
									doRecord = false;
									const char * merr = mntRes2Str(mres);
									int msglen = strlen(merr) + strlen(g_settings.network_nfs_local_dir[userDecision]) + 7;
									char msg[msglen];
									strcpy(msg,merr);
									strcat(msg,"\nDir: ");
									strcat(msg,g_settings.network_nfs_local_dir[userDecision]);

									ShowMsgUTF(LOCALE_MESSAGEBOX_ERROR, msg,
										   CMessageBox::mbrBack, CMessageBox::mbBack,NEUTRINO_ICON_ERROR, 450, 10); // UTF-8
								}
							}
						} else {
							doRecord = false;
						}
					} else {
						printf("[neutrino.cpp] no network devices available\n");
						doRecord = false;
					}
				}
				(static_cast<CVCRControl::CFileDevice*>(recordingdevice))->Directory = recDir;
printf("CNeutrinoApp::doGuiRecord: start to dir %s\n", recDir);
			}
			if(!doRecord || (CVCRControl::getInstance()->Record(&eventinfo)==false)) {
				recordingstatus=0;
				if(doRecord)
					return true;// try to refresh gui if record was not ok ?
				return refreshGui;
			}
			else if (addTimer) {
				time_t now = time(NULL);
				recording_id = g_Timerd->addImmediateRecordTimerEvent(eventinfo.channel_id, now, now+g_settings.record_hours*60*60, eventinfo.epgID, eventinfo.epg_starttime, eventinfo.apids);
			}
		} else {
			g_Timerd->stopTimerEvent(recording_id);
			startNextRecording();
		}
		return refreshGui;
	}
	else
		puts("[neutrino.cpp] no recording devices");
	return false;
}

#define LCD_UPDATE_TIME_RADIO_MODE (6 * 1000 * 1000)
#define LCD_UPDATE_TIME_TV_MODE (60 * 1000 * 1000)

void CNeutrinoApp::SendSectionsdConfig(void)
{
        CSectionsdClient::epg_config config;
        config.scanMode                 = scanSettings.scanSectionsd;
        config.epg_cache                = atoi(g_settings.epg_cache.c_str());
        config.epg_old_events           = atoi(g_settings.epg_old_events.c_str());
        config.epg_max_events           = atoi(g_settings.epg_max_events.c_str());
        config.epg_extendedcache        = atoi(g_settings.epg_extendedcache.c_str());
        config.epg_dir                  = g_settings.epg_dir;
        config.network_ntpserver        = g_settings.network_ntpserver;
        config.network_ntprefresh       = atoi(g_settings.network_ntprefresh.c_str());
        config.network_ntpenable        = g_settings.network_ntpenable;
        g_Sectionsd->setConfig(config);
}

void CNeutrinoApp::InitZapper()
{
 	struct stat my_stat;    

	g_InfoViewer->start();
	SendSectionsdConfig();
	if (g_settings.epg_save){
		if(stat(g_settings.epg_dir.c_str(), &my_stat) == 0)
			g_Sectionsd->readSIfromXML(g_settings.epg_dir.c_str());
	}
	firstChannel();
	channelsInit();

	if(firstchannel.mode == 't') {
		tvMode(false);
	} else {
		g_RCInput->killTimer(g_InfoViewer->lcdUpdateTimer);
		g_InfoViewer->lcdUpdateTimer = g_RCInput->addTimer( LCD_UPDATE_TIME_RADIO_MODE, false );
		radioMode(false);
	}
	if(g_settings.cacheTXT)
		tuxtxt_init();
	if(channelList->getSize() && live_channel_id) {
		channelList->adjustToChannelID(live_channel_id);
		CVFD::getInstance ()->showServicename(channelList->getActiveChannelName());
		g_Sectionsd->setPauseScanning(false);
		g_Sectionsd->setServiceChanged(live_channel_id&0xFFFFFFFFFFFFULL, true );
		g_Zapit->getPIDS(g_RemoteControl->current_PIDs);
		if(g_settings.cacheTXT)
			if(g_RemoteControl->current_PIDs.PIDs.vtxtpid != 0)
				tuxtxt_start(g_RemoteControl->current_PIDs.PIDs.vtxtpid);
		g_RCInput->postMsg(NeutrinoMessages::SHOW_INFOBAR, 0);
	}
}

void CNeutrinoApp::setupRecordingDevice(void)
{
	if (g_settings.recording_type == RECORDING_SERVER)
	{
		unsigned int port;
		sscanf(g_settings.recording_server_port, "%u", &port);

		recordingdevice = new CVCRControl::CServerDevice(g_settings.recording_stopplayback, g_settings.recording_stopsectionsd, g_settings.recording_server_ip.c_str(), port);
		CVCRControl::getInstance()->registerDevice(recordingdevice);
	}
	else if (g_settings.recording_type == RECORDING_FILE)
	{
		unsigned int splitsize, ringbuffers;
		sscanf(g_settings.recording_splitsize, "%u", &splitsize);
		sscanf(g_settings.recording_ringbuffers, "%u", &ringbuffers);

		recordingdevice = new CVCRControl::CFileDevice(g_settings.recording_stopplayback, g_settings.recording_stopsectionsd, g_settings.network_nfs_recordingdir, splitsize, g_settings.recording_use_o_sync, g_settings.recording_use_fdatasync, g_settings.recording_stream_vtxt_pid, g_settings.recording_stream_pmt_pid, ringbuffers);

		CVCRControl::getInstance()->registerDevice(recordingdevice);
	}
	else if(g_settings.recording_type == RECORDING_VCR)
	{
		recordingdevice = new CVCRControl::CVCRDevice((g_settings.recording_vcr_no_scart == 0));

		CVCRControl::getInstance()->registerDevice(recordingdevice);
	}
	else
	{
		if (CVCRControl::getInstance()->isDeviceRegistered())
			CVCRControl::getInstance()->unregisterDevice();
	}
}

//CMenuWidget    moviePlayer         (LOCALE_MOVIEPLAYER_HEAD              , "streaming.raw"       );
CMenuWidget * gmoviePlayer;
#if 0
CPipSetup * g_Pip0;
#endif
#include "videosettings.h"
extern CVideoSettings * videoSettings;
extern CMenuOptionStringChooser* tzSelect;

void CISendMessage(uint32_t msg, uint32_t data)
{
	g_RCInput->postMsg(msg, data);
}

int CNeutrinoApp::run(int argc, char **argv)
{
	CmdParser(argc, argv);

	CHintBox * hintBox;

	int loadSettingsErg = loadSetup(NEUTRINO_SETTINGS_FILE);

	bool display_language_selection;
	CLocaleManager::loadLocale_ret_t loadLocale_ret = g_Locale->loadLocale(g_settings.language);
	if (loadLocale_ret == CLocaleManager::NO_SUCH_LOCALE)
	{
		strcpy(g_settings.language, "english");
		loadLocale_ret = g_Locale->loadLocale(g_settings.language);
		display_language_selection = true;
	}
	else
		display_language_selection = false;

	SetupFonts();
	SetupTiming();
	colorSetupNotifier        = new CColorSetupNotifier;
	colorSetupNotifier->changeNotify(NONEXISTANT_LOCALE, NULL);
	hintBox = new CHintBox(LOCALE_MESSAGEBOX_INFO, g_Locale->getText(LOCALE_NEUTRINO_STARTING));
	hintBox->paint();
	CVFD::getInstance()->init(font.filename, font.name);

	CVFD::getInstance()->Clear();
	CVFD::getInstance()->ShowText((char *) g_Locale->getText(LOCALE_NEUTRINO_STARTING));

	init_cs_api();

	pthread_create (&zapit_thread, NULL, zapit_main_thread, (void *) g_settings.video_Mode);
	audioSetupNotifier        = new CAudioSetupNotifier;

	while(!zapit_ready)
		usleep(0);
	printf("zapit ready\n\n");

	audioDecoder->SetSRS(g_settings.srs_enable, g_settings.srs_nmgr_enable, g_settings.srs_algo, g_settings.srs_ref_volume);
	audioDecoder->setVolume(g_settings.current_volume, g_settings.current_volume);
	audioDecoder->SetHdmiDD(g_settings.hdmi_dd ? true : false);
	//audioDecoder->SetSpdifDD(g_settings.spdif_dd ? true : false);
	audioDecoder->SetSpdifDD(g_settings.audio_spdif_dd, g_settings.audio_spdif_dts, g_settings.audio_spdif_aac);
	videoDecoder->SetDBDR(g_settings.video_dbdr);
	audioSetupNotifier->changeNotify(LOCALE_AUDIOMENU_AVSYNC, NULL);

	if(display_language_selection)
		videoDecoder->ShowPicture(DATADIR "/neutrino/icons/start.mvi");

	powerManager = new cPowerManager;

	if (powerManager) {
		if (!powerManager->Open())
			printf("opening powermanager failed\n");
	}

	cpuFreq = new cCpuFreqManager();
	cpuFreq->SetCpuFreq(g_settings.cpufreq * 1000 * 1000);


	dvbsub_init();

	pthread_create (&timer_thread, NULL, timerd_main_thread, (void *) NULL);
	pthread_create (&nhttpd_thread, NULL, nhttpd_main_thread, (void *) NULL);

	pthread_create (&stream_thread, NULL, streamts_main_thread, (void *) NULL);

	hintBox->hide(); //FIXME
	hintBox->paint();

#ifndef DISABLE_SECTIONSD
	pthread_create (&sections_thread, NULL, sectionsd_main_thread, (void *) NULL);
#endif
	g_Zapit         = new CZapitClient;

	if (!scanSettings.loadSettings(NEUTRINO_SCAN_SETTINGS_FILE, (g_info.delivery_system = g_Zapit->getDeliverySystem()))) {
		dprintf(DEBUG_NORMAL, "Loading of scan settings failed. Using defaults.\n");
	}


	CVFD::getInstance()->showVolume(g_settings.current_volume);
	CVFD::getInstance()->setMuted(current_muted);

	InfoClock = new CInfoClock();
	if(g_settings.mode_clock)
		InfoClock->StartClock();

	g_RCInput = new CRCInput;

	g_Sectionsd = new CSectionsdClient;
	//g_Sectionsd->setServiceChanged(live_channel_id &0xFFFFFFFFFFFFULL, false);
	g_Timerd = new CTimerdClient;

	g_RemoteControl = new CRemoteControl;
	g_EpgData = new CEpgData;
	g_InfoViewer = new CInfoViewer;
	g_EventList = new EventList;
	g_volscale = new CScale(200, 15, 50, 100, 80, true);
	g_CamHandler = new CCAMMenuHandler();
	g_CamHandler->init();

	audio_menu = new CAudioSelectMenuHandler;

	g_PluginList = new CPlugins;
	g_PluginList->setPluginDir(PLUGINDIR);
	g_PicViewer = new CPictureViewer();
	// mount shares before scanning for plugins
	CFSMounter::automount();
	//load Pluginlist before main menu (only show script menu if at least one script is available
	g_PluginList->loadPlugins();

	APIDChanger               = new CAPIDChangeExec;
	NVODChanger               = new CNVODChangeExec;
	StreamFeaturesChanger     = new CStreamFeaturesChangeExec;
	MoviePluginChanger        = new CMoviePluginChangeExec;
	OnekeyPluginChanger       = new COnekeyPluginChangeExec;
	MyIPChanger               = new CIPChangeNotifier;
	ConsoleDestinationChanger = new CConsoleDestChangeNotifier;
	rcLock                    = new CRCLock();
	//USERMENU
	Timerlist                 = new CTimerList;

	// setup recording device
	if (g_settings.recording_type != RECORDING_OFF)
		setupRecordingDevice();

	dprintf( DEBUG_NORMAL, "menue setup\n");

	c_SMSKeyInput = new SMSKeyInput();
	//Main settings
	CMenuWidget    mainMenu            (LOCALE_MAINMENU_HEAD                 , "mainmenue.raw"       );
	CMenuWidget    mainSettings        (LOCALE_MAINSETTINGS_HEAD             , NEUTRINO_ICON_SETTINGS);
	CMenuWidget    languageSettings    (LOCALE_LANGUAGESETUP_HEAD            , "language.raw"        );
	CMenuWidget    audioSettings       (LOCALE_AUDIOMENU_HEAD                , "audio.raw"           );
	CMenuWidget    parentallockSettings(LOCALE_PARENTALLOCK_PARENTALLOCK     , "lock.raw"            , 500);
	CMenuWidget    networkSettings     (LOCALE_NETWORKMENU_HEAD              , "network.raw"         );
	CMenuWidget    recordingSettings   (LOCALE_RECORDINGMENU_HEAD            , "recording.raw"       );
	CMenuWidget    streamingSettings   (LOCALE_STREAMINGMENU_HEAD            , "streaming.raw"       );
	//CMenuWidget    colorSettings       (LOCALE_COLORMENU_HEAD                , "colors.raw"          );
	CMenuWidget    colorSettings       (LOCALE_MAINSETTINGS_OSD                , "colors.raw"          );
	CMenuWidget    fontSettings        (LOCALE_FONTMENU_HEAD                 , "colors.raw"          );
	CMenuWidget    lcdSettings         (LOCALE_LCDMENU_HEAD                  , "lcd.raw"             );
	//CMenuWidget    keySettings         (LOCALE_KEYBINDINGMENU_HEAD           , "keybinding.raw"      , 400);
	CMenuWidget    keySettings         (LOCALE_MAINSETTINGS_KEYBINDING           , "keybinding.raw"      , 400);
	CMenuWidget    miscSettings        (LOCALE_MISCSETTINGS_HEAD             , NEUTRINO_ICON_SETTINGS);
	CMenuWidget    audioplPicSettings  (LOCALE_AUDIOPLAYERPICSETTINGS_GENERAL, NEUTRINO_ICON_SETTINGS);
	CMenuWidget    scanSettings        (LOCALE_SERVICEMENU_SCANTS            , NEUTRINO_ICON_SETTINGS);
	CMenuWidget    service             (LOCALE_SERVICEMENU_HEAD              , NEUTRINO_ICON_SETTINGS);
	CMenuWidget    moviePlayer         (LOCALE_MOVIEPLAYER_HEAD              , "streaming.raw"       );
	CMenuWidget    shutdown            (LOCALE_MAINMENU_SHUTDOWN              , NEUTRINO_ICON_SETTINGS);
	gmoviePlayer = &moviePlayer;
	InitMainMenu(mainMenu, mainSettings, audioSettings, parentallockSettings, networkSettings, recordingSettings,
			colorSettings, lcdSettings, keySettings, languageSettings, miscSettings,
			service, fontSettings, audioplPicSettings, streamingSettings, moviePlayer, shutdown);
	InitServiceSettings(service, scanSettings);
	InitLanguageSettings(languageSettings);
	InitAudioplPicSettings(audioplPicSettings);
	InitMiscSettings(miscSettings);
	InitAudioSettings(audioSettings, audioSetupNotifier);
	InitParentalLockSettings(parentallockSettings);
	InitScanSettings(scanSettings);

	dprintf( DEBUG_NORMAL, "registering as event client\n");
#if 0
	g_Controld->registerEvent(CControldClient::EVT_MUTECHANGED, 222, NEUTRINO_UDS_NAME);
	g_Controld->registerEvent(CControldClient::EVT_VOLUMECHANGED, 222, NEUTRINO_UDS_NAME);
	g_Controld->registerEvent(CControldClient::EVT_MODECHANGED, 222, NEUTRINO_UDS_NAME);
	g_Controld->registerEvent(CControldClient::EVT_VCRCHANGED, 222, NEUTRINO_UDS_NAME);
#endif

	g_Sectionsd->registerEvent(CSectionsdClient::EVT_TIMESET, 222, NEUTRINO_UDS_NAME);
	g_Sectionsd->registerEvent(CSectionsdClient::EVT_GOT_CN_EPG, 222, NEUTRINO_UDS_NAME);
	g_Sectionsd->registerEvent(CSectionsdClient::EVT_SERVICES_UPDATE, 222, NEUTRINO_UDS_NAME);
	g_Sectionsd->registerEvent(CSectionsdClient::EVT_BOUQUETS_UPDATE, 222, NEUTRINO_UDS_NAME);
	g_Sectionsd->registerEvent(CSectionsdClient::EVT_WRITE_SI_FINISHED, 222, NEUTRINO_UDS_NAME);

#define ZAPIT_EVENT_COUNT 29
	const CZapitClient::events zapit_event[ZAPIT_EVENT_COUNT] =
	{
		CZapitClient::EVT_ZAP_COMPLETE,
		CZapitClient::EVT_ZAP_COMPLETE_IS_NVOD,
		CZapitClient::EVT_ZAP_FAILED,
		CZapitClient::EVT_ZAP_SUB_COMPLETE,
		CZapitClient::EVT_ZAP_SUB_FAILED,
		CZapitClient::EVT_ZAP_MOTOR,
		CZapitClient::EVT_ZAP_CA_CLEAR,
		CZapitClient::EVT_ZAP_CA_LOCK,
		CZapitClient::EVT_ZAP_CA_FTA,
		CZapitClient::EVT_ZAP_CA_ID,
		CZapitClient::EVT_RECORDMODE_ACTIVATED,
		CZapitClient::EVT_RECORDMODE_DEACTIVATED,
		CZapitClient::EVT_SCAN_COMPLETE,
		CZapitClient::EVT_SCAN_FAILED,
		CZapitClient::EVT_SCAN_NUM_TRANSPONDERS,
		CZapitClient::EVT_SCAN_REPORT_NUM_SCANNED_TRANSPONDERS,
		CZapitClient::EVT_SCAN_REPORT_FREQUENCY,
		CZapitClient::EVT_SCAN_REPORT_FREQUENCYP,
		CZapitClient::EVT_SCAN_SATELLITE,
		CZapitClient::EVT_SCAN_NUM_CHANNELS,
		CZapitClient::EVT_SCAN_PROVIDER,
		CZapitClient::EVT_BOUQUETS_CHANGED,
		CZapitClient::EVT_SERVICES_CHANGED,
		CZapitClient::EVT_SCAN_SERVICENAME,
		CZapitClient::EVT_SCAN_FOUND_A_CHAN,
		CZapitClient::EVT_SCAN_FOUND_TV_CHAN,
		CZapitClient::EVT_SCAN_FOUND_RADIO_CHAN,
		CZapitClient::EVT_SCAN_FOUND_DATA_CHAN,
		CZapitClient::EVT_SDT_CHANGED
	};

	for (int i = 0; i < ZAPIT_EVENT_COUNT; i++)
		g_Zapit->registerEvent(zapit_event[i], 222, NEUTRINO_UDS_NAME);

	g_Timerd->registerEvent(CTimerdClient::EVT_ANNOUNCE_SHUTDOWN, 222, NEUTRINO_UDS_NAME);
	g_Timerd->registerEvent(CTimerdClient::EVT_SHUTDOWN, 222, NEUTRINO_UDS_NAME);
	g_Timerd->registerEvent(CTimerdClient::EVT_ANNOUNCE_NEXTPROGRAM, 222, NEUTRINO_UDS_NAME);
	g_Timerd->registerEvent(CTimerdClient::EVT_NEXTPROGRAM, 222, NEUTRINO_UDS_NAME);
	g_Timerd->registerEvent(CTimerdClient::EVT_STANDBY_ON, 222, NEUTRINO_UDS_NAME);
	g_Timerd->registerEvent(CTimerdClient::EVT_STANDBY_OFF, 222, NEUTRINO_UDS_NAME);
	g_Timerd->registerEvent(CTimerdClient::EVT_ANNOUNCE_RECORD, 222, NEUTRINO_UDS_NAME);
	g_Timerd->registerEvent(CTimerdClient::EVT_RECORD_START, 222, NEUTRINO_UDS_NAME);
	g_Timerd->registerEvent(CTimerdClient::EVT_RECORD_STOP, 222, NEUTRINO_UDS_NAME);
	g_Timerd->registerEvent(CTimerdClient::EVT_ANNOUNCE_ZAPTO, 222, NEUTRINO_UDS_NAME);
	g_Timerd->registerEvent(CTimerdClient::EVT_ZAPTO, 222, NEUTRINO_UDS_NAME);
	g_Timerd->registerEvent(CTimerdClient::EVT_SLEEPTIMER, 222, NEUTRINO_UDS_NAME);
	g_Timerd->registerEvent(CTimerdClient::EVT_ANNOUNCE_SLEEPTIMER, 222, NEUTRINO_UDS_NAME);
	g_Timerd->registerEvent(CTimerdClient::EVT_REMIND, 222, NEUTRINO_UDS_NAME);
	g_Timerd->registerEvent(CTimerdClient::EVT_EXEC_PLUGIN, 222, NEUTRINO_UDS_NAME);

	InitNetworkSettings(networkSettings);
	InitKeySettings(keySettings);
	InitFontSettings(fontSettings);
	InitColorSettings(colorSettings, fontSettings);

	if (display_language_selection) {
		hintBox->hide();
		int ret = languageSettings.exec(NULL, "");

		if(ret != menu_return::RETURN_EXIT_ALL)
			videoSettings->exec(NULL, "");
		if(ret != menu_return::RETURN_EXIT_ALL)
			colorSettings.exec(NULL, "");
		if(ret != menu_return::RETURN_EXIT_ALL)
			if(tzSelect)
				tzSelect->exec(NULL);
		if(ret != menu_return::RETURN_EXIT_ALL)
			networkSettings.exec(NULL, "");
		if(ret != menu_return::RETURN_EXIT_ALL)
			scanSettings.exec(NULL, "");

		videoDecoder->StopPicture();
	}

	if(loadSettingsErg) {
		hintBox->hide();
		dprintf(DEBUG_INFO, "config file or options missing\n");
		ShowHintUTF(LOCALE_MESSAGEBOX_INFO, loadSettingsErg ==  1 ? g_Locale->getText(LOCALE_SETTINGS_NOCONFFILE)
				: g_Locale->getText(LOCALE_SETTINGS_MISSINGOPTIONSCONFFILE));
		configfile.setModifiedFlag(true);
		saveSetup(NEUTRINO_SETTINGS_FILE);
	}
#if 1 // FIXME
	system("mkdir /media/sda1 2> /dev/null");
	system("mount /media/sda1 2> /dev/null");
	system("mkdir /media/sdb1 2> /dev/null");
	system("mount /media/sdb1 2> /dev/null");
	CHDDDestExec * hdd = new CHDDDestExec();
	hdd->exec(NULL, "");
#endif

	InitZapper();
	InitRecordingSettings(recordingSettings);
	InitStreamingSettings(streamingSettings);
	InitLcdSettings(lcdSettings);

	AudioMute( current_muted, true);

	SHTDCNT::getInstance()->init();

	hintBox->hide();
	delete hintBox;

	cDvbCi::getInstance()->SetHook(CISendMessage);
	RealRun(mainMenu);

	ExitRun(true);

	return 0;
}

int tuxtx_main(int _rc, void * _fb, int pid, int x, int y, int w, int h);

void CNeutrinoApp::RealRun(CMenuWidget &mainMenu)
{
	neutrino_msg_t      msg;
	neutrino_msg_data_t data;

	dprintf(DEBUG_NORMAL, "initialized everything\n");

	g_PluginList->startPlugin("startup.cfg");
	if (!g_PluginList->getScriptOutput().empty()) {
		ShowMsgUTF(LOCALE_PLUGINS_RESULT, g_PluginList->getScriptOutput(), CMessageBox::mbrBack,CMessageBox::mbBack,NEUTRINO_ICON_SHELL);
	}
	g_RCInput->clearRCMsg();
	if(g_settings.power_standby)
		standbyMode(true);

	while( true ) {
		g_RCInput->getMsg(&msg, &data, 100);	// 10 secs..

		if( ( mode == mode_tv ) || ( ( mode == mode_radio ) ) ) {
			if( (msg == NeutrinoMessages::SHOW_EPG) /* || (msg == CRCInput::RC_info) */ ) {
				//g_EpgData->show( g_Zapit->getCurrentServiceID() );
				g_EpgData->show(live_channel_id);
			}
			else if( msg == CRCInput::RC_epg ) {
				g_EventList->exec(live_channel_id, channelList->getActiveChannelName());
			}
			else if( msg == CRCInput::RC_text) {
				g_RCInput->clearRCMsg();
				if(g_settings.mode_clock)
					InfoClock->StopClock();

				tuxtx_main(g_RCInput->getFileHandle(), frameBuffer->getFrameBufferPointer(), g_RemoteControl->current_PIDs.PIDs.vtxtpid,
					frameBuffer->getScreenX(), frameBuffer->getScreenY(), frameBuffer->getScreenWidth(), frameBuffer->getScreenHeight());

				frameBuffer->paintBackground();
				//if(!g_settings.cacheTXT)
				//	tuxtxt_stop();
				g_RCInput->clearRCMsg();
				AudioMute(current_muted, true);
				if(g_settings.mode_clock)
					InfoClock->StartClock();
			}
			else if( msg == CRCInput::RC_setup ) {
				if(!g_settings.minimode) {
					dvbsub_pause();
                                	if(g_settings.mode_clock)
                                	        InfoClock->StopClock();
					mainMenu.exec(NULL, "");
					// restore mute symbol
					AudioMute(current_muted, true);
                                	if(g_settings.mode_clock)
                                	        InfoClock->StartClock();
					dvbsub_start(0);
					saveSetup(NEUTRINO_SETTINGS_FILE);
				}
			}
			else if( msg == (neutrino_msg_t) g_settings.key_tvradio_mode ) {
				if( mode == mode_tv )
					radioMode();
				else if( mode == mode_radio )
					tvMode();
			}
			else if( ((msg == CRCInput::RC_tv) || (msg == CRCInput::RC_radio)) && ((neutrino_msg_t)g_settings.key_tvradio_mode == CRCInput::RC_nokey)) {
				if(mode == mode_radio )
					tvMode();
				else if(mode == mode_tv)
					radioMode();
			}
			else if( ( msg == (neutrino_msg_t) g_settings.key_quickzap_up ) || ( msg == (neutrino_msg_t) g_settings.key_quickzap_down ) )
			{
				//quickzap
				//changed for auto-shift if(!recordingstatus && g_settings.zap_cycle && (bouquetList!=NULL) && !(bouquetList->Bouquets.empty()))
				if(g_settings.zap_cycle && (bouquetList!=NULL) && !(bouquetList->Bouquets.empty()))
				{
					bouquetList->Bouquets[bouquetList->getActiveBouquetNumber()]->channelList->quickZap(msg, true);
				} else
					channelList->quickZap(msg);
			}
			else if( msg == (neutrino_msg_t) g_settings.key_subchannel_up || msg == CRCInput::RC_channelup ) {
			msg = (neutrino_msg_t) g_settings.key_subchannel_up;
			   if(g_RemoteControl->subChannels.size() > 0) {
				g_RemoteControl->subChannelUp();
				g_InfoViewer->showSubchan(); 
			    } else
				channelList->quickZap( msg ); 
			}
			else if( msg == (neutrino_msg_t) g_settings.key_subchannel_down || msg == CRCInput::RC_channeldown ) {
			msg = (neutrino_msg_t) g_settings.key_subchannel_down;
			   if(g_RemoteControl->subChannels.size()> 0) {
				g_RemoteControl->subChannelDown();
				g_InfoViewer->showSubchan();
			    } else
				channelList->quickZap( msg ); 
			}
			else if( msg == (neutrino_msg_t) g_settings.key_zaphistory ) {
				// Zap-History "Bouquet"
				dvbsub_pause();
				int res = channelList->numericZap( msg );
				if(res < 0)
					dvbsub_start(0);
			}
			else if( msg == (neutrino_msg_t) g_settings.key_lastchannel ) {
				// Quick Zap
				channelList->numericZap( msg );
			}
			else if( msg == (neutrino_msg_t) g_settings.key_plugin ) {
				g_PluginList->start_plugin_by_name(g_settings.onekey_plugin.c_str(), 0);
			}
			else if( (msg == (neutrino_msg_t) g_settings.key_timeshift) || msg == CRCInput::RC_rewind) {
			   if((g_settings.recording_type == RECORDING_FILE) && !g_settings.minimode) {
printf("[neutrino] timeshift try, recordingstatus %d, rec dir %s, timeshift dir %s temp timeshift %d ...\n", recordingstatus, g_settings.network_nfs_recordingdir, timeshiftDir, g_settings.temp_timeshift);
				std::string tmode;
				if(msg == CRCInput::RC_rewind)
					tmode = "rtimeshift"; // rewind
				else if(recordingstatus && msg == (neutrino_msg_t) g_settings.key_timeshift)
					tmode = "ptimeshift"; // already recording, pause
				else
					tmode = "timeshift"; // record just started

				if(g_RemoteControl->is_video_started) {
					if(recordingstatus) {
						//dvbsub_pause();
						moviePlayerGui->exec(NULL, tmode);
						//dvbsub_start(0);
					} else if(msg != CRCInput::RC_rewind) {
						//dvbsub_pause();
						if(g_settings.temp_timeshift) {
							startAutoRecord(true);
						} else {
							recordingstatus = 1;
							doGuiRecord(g_settings.network_nfs_recordingdir, true);
						}
						if(recordingstatus) {
							//dvbsub_pause();
							moviePlayerGui->exec(NULL, tmode);
							//dvbsub_start(0);
						}
					}
				}
			   }
			}
/*			else if((msg &~ CRCInput::RC_Repeat) == CRCInput::RC_tv)
			{
				printf("TRYING TO DETECT REPEAT\n");
			}
*/			else if( msg == CRCInput::RC_record || msg == CRCInput::RC_stop ) {
				printf("[neutrino] direct record\n");
				if(recordingstatus) {
					// AZBOX
					printf("[neutrino] direct record _ recordingstatus\n");
					if(ShowLocalizedMessage(LOCALE_MESSAGEBOX_INFO, LOCALE_SHUTDOWN_RECODING_QUERY, 
						CMessageBox::mbrYes, CMessageBox::mbYes | CMessageBox::mbNo, NULL, 450, 30, true) == CMessageBox::mbrYes)
							g_Timerd->stopTimerEvent(recording_id);
				} else if(msg != CRCInput::RC_stop ) {
					printf("[neutrino] direct record _ !recordingstatus\n");
					recordingstatus = 1;
					doGuiRecord(g_settings.network_nfs_recordingdir, true);
				}
			}
			else if( msg == CRCInput::RC_red ) {
				showUserMenu(SNeutrinoSettings::BUTTON_RED);
			}
			else if( (msg == CRCInput::RC_green) || ((msg == CRCInput::RC_audio) && !g_settings.audio_run_player) )
			{
				showUserMenu(SNeutrinoSettings::BUTTON_GREEN);
			}
			else if( msg == CRCInput::RC_yellow ) {       // NVODs
				showUserMenu(SNeutrinoSettings::BUTTON_YELLOW);
			}
			else if( msg == CRCInput::RC_blue ) {
				showUserMenu(SNeutrinoSettings::BUTTON_BLUE);
			}
			else if( (msg == CRCInput::RC_audio) && g_settings.audio_run_player) {
				dvbsub_pause();
				audioPlayer->exec(NULL, "");
				dvbsub_start(0);
			}
			else if( msg == CRCInput::RC_video || msg == CRCInput::RC_play ) {
				bool show = true;
				if ((g_settings.parentallock_prompt == PARENTALLOCK_PROMPT_ONSIGNAL) || (g_settings.parentallock_prompt == PARENTALLOCK_PROMPT_CHANGETOLOCKED)) {
                                        CZapProtection* zapProtection = new CZapProtection( g_settings.parentallock_pincode, 0x100 );
                                        show = zapProtection->check();
					delete zapProtection;
				}
				if(show) {
					//dvbsub_pause();
					if( mode == mode_radio )
						videoDecoder->StopPicture();
					moviePlayerGui->exec(NULL, "tsmoviebrowser");
					if( mode == mode_radio )
						videoDecoder->ShowPicture(DATADIR "/neutrino/icons/radiomode.mvi");
					//dvbsub_start(0);
				}
			}
			else if (CRCInput::isNumeric(msg) && g_RemoteControl->director_mode ) {
				g_RemoteControl->setSubChannel(CRCInput::getNumericValue(msg));
				g_InfoViewer->showSubchan();
			}
			else if (CRCInput::isNumeric(msg)) {
				channelList->numericZap( msg );
			}
			else if( ( msg == CRCInput::RC_help ) || ( msg == CRCInput::RC_info) ||
						( msg == NeutrinoMessages::SHOW_INFOBAR ) )
			{
				bool show_info = ((msg != NeutrinoMessages::SHOW_INFOBAR) || (g_InfoViewer->is_visible || g_settings.timing[SNeutrinoSettings::TIMING_INFOBAR] != 0));
			         // turn on LCD display
				CVFD::getInstance()->setMode(CVFD::MODE_TVRADIO); 

				// show Infoviewer
				if(show_info && channelList->getSize()) {
					dvbsub_pause();
					g_InfoViewer->showTitle(channelList->getActiveChannelNumber(), channelList->getActiveChannelName(), channelList->getActiveSatellitePosition(), channelList->getActiveChannel_ChannelID()); // UTF-8
					dvbsub_start(0);
				}
			}
#if 0
			else if( msg == CRCInput::RC_shift_blue ) {
				if(CVFD::getInstance()->has_lcd) {
					standbyMode( true );
					//this->rcLock->exec(NULL,CRCLock::NO_USER_INPUT);
					this->rcLock->lockBox();
					standbyMode( false );
				}
			}
			else if( msg == CRCInput::RC_shift_tv ) {
				if(CVFD::getInstance()->has_lcd) { // FIXME hack for 5xxx dreams
					scartMode( true );
					//this->rcLock->exec(NULL,CRCLock::NO_USER_INPUT);
					this->rcLock->lockBox();
					scartMode( false );
				}
			}
			else if( msg == CRCInput::RC_shift_red) {
				if(CVFD::getInstance()->has_lcd) { // FIXME hack for 5xxx dreams
					g_settings.minimode = !g_settings.minimode;
					saveSetup(NEUTRINO_SETTINGS_FILE);
				}
			} else if( msg == CRCInput::RC_shift_radio) {
				CVCRControl::getInstance()->Screenshot(g_RemoteControl->current_channel_id);
			} 
#endif
			else {
				if (msg == CRCInput::RC_home)
  					CVFD::getInstance()->setMode(CVFD::MODE_TVRADIO);
				handleMsg(msg, data);
			}
		}
		else {
			// mode == mode_scart
			if( msg == CRCInput::RC_home ) {
				if( mode == mode_scart ) {
					//wenn VCR Aufnahme dann stoppen
					if(CVCRControl::getInstance()->isDeviceRegistered()) {
						if ((CVCRControl::getInstance()->Device->getDeviceType() == CVCRControl::DEVICE_VCR) &&
						    (CVCRControl::getInstance()->getDeviceState() == CVCRControl::CMD_VCR_RECORD ||
						     CVCRControl::getInstance()->getDeviceState() == CVCRControl::CMD_VCR_PAUSE))
						{
							CVCRControl::getInstance()->Stop();
							recordingstatus=0;
							startNextRecording();
						}
					}
					// Scart-Mode verlassen
					scartMode( false );
				}
			}
			else {
				handleMsg(msg, data);
			}
		}
	}
}

int CNeutrinoApp::handleMsg(const neutrino_msg_t msg, neutrino_msg_data_t data)
{
	int res = 0;
//printf("[neutrino] handleMsg %X data %X\n", msg, data); fflush(stdout);

	if(msg == NeutrinoMessages::EVT_ZAP_COMPLETE) {
		g_Zapit->getAudioMode(&g_settings.audio_AnalogMode);
		if(g_settings.audio_AnalogMode < 0 || g_settings.audio_AnalogMode > 2)
			g_settings.audio_AnalogMode = 0;
#if 0 // per-channel auto volume save/restore
		unsigned int volume;
		g_Zapit->getVolume(&volume, &volume);
		current_volume = 100 - volume*100/63;
		printf("zapit volume %d new current %d mode %d\n", volume, current_volume, g_settings.audio_AnalogMode);
		setvol(current_volume,(g_settings.audio_avs_Control));
#endif
		if(shift_timer) {
			g_RCInput->killTimer (shift_timer);
			shift_timer = 0;
		}
		if (!recordingstatus && g_settings.auto_timeshift) {
			int delay = g_settings.auto_timeshift;
			shift_timer = g_RCInput->addTimer( delay*1000*1000, true );
			g_InfoViewer->handleMsg(NeutrinoMessages::EVT_RECORDMODE, 1);
		}

		if(scrambled_timer) {
			g_RCInput->killTimer(scrambled_timer);
			scrambled_timer = 0;
		}
		scrambled_timer = g_RCInput->addTimer(10*1000*1000, true);
	}
	if ((msg == NeutrinoMessages::EVT_TIMER)) {
		if(data == shift_timer) {
			shift_timer = 0;
			startAutoRecord(true);
			return messages_return::handled;
		} else if(data == scrambled_timer) {
			scrambled_timer = 0;
			if(videoDecoder->getBlank() && videoDecoder->getPlayState()) {
				const char * text = g_Locale->getText(LOCALE_SCRAMBLED_CHANNEL);
				ShowHintUTF (LOCALE_MESSAGEBOX_INFO, text, g_Font[SNeutrinoSettings::FONT_TYPE_MENU]->getRenderWidth (text, true) + 10, 5);
			}
			return messages_return::handled;
				
		}
	}

	res = res | g_RemoteControl->handleMsg(msg, data);
	res = res | g_InfoViewer->handleMsg(msg, data);
	res = res | channelList->handleMsg(msg, data);

	//printf("[neutrino] handleMsg %X unhandled by others\n", msg); fflush(stdout);
	if( res != messages_return::unhandled ) {
		if( ( msg>= CRCInput::RC_WithData ) && ( msg< CRCInput::RC_WithData+ 0x10000000 ) )
			delete (unsigned char*) data;
		return( res & ( 0xFFFFFFFF - messages_return::unhandled ) );
	}

	/* we assume g_CamHandler free/delete data if needed */
	res = g_CamHandler->handleMsg(msg, data);
	if( res != messages_return::unhandled ) {
		return(res & (0xFFFFFFFF - messages_return::unhandled));
	}

	/* ================================== KEYS ================================================ */
	if( msg == CRCInput::RC_ok || msg == CRCInput::RC_sat || msg == CRCInput::RC_favorites) {
		if( (mode == mode_tv) || (mode == mode_radio) || (mode == mode_ts)) {
			if(g_settings.mode_clock)
				InfoClock->StopClock();

			dvbsub_pause();

			int nNewChannel = -1;
			int old_num = 0;
			int old_b = bouquetList->getActiveBouquetNumber();
			int old_mode = g_settings.channel_mode;
			printf("************************* ZAP START: bouquetList %x size %d old_b %d\n", (int) bouquetList, bouquetList->Bouquets.size(), old_b);fflush(stdout);

			if(bouquetList->Bouquets.size()) {
				old_num = bouquetList->Bouquets[old_b]->channelList->getActiveChannelNumber();
			}

			//if(msg == CRCInput::RC_ok && bouquetList->Bouquets.size()) 
			if(msg == CRCInput::RC_ok) 
			{
				if(bouquetList->Bouquets.size() && bouquetList->Bouquets[old_b]->channelList->getSize() > 0)
					nNewChannel = bouquetList->Bouquets[old_b]->channelList->exec();//with ZAP!
				else
					nNewChannel = bouquetList->exec(true);
			} else if(msg == CRCInput::RC_sat) {
				SetChannelMode(LIST_MODE_SAT);
				nNewChannel = bouquetList->exec(true);
			} else if(msg == CRCInput::RC_favorites) {
				SetChannelMode(LIST_MODE_FAV);
				nNewChannel = bouquetList->exec(true);
			}
_repeat:
			printf("************************* ZAP RES: nNewChannel %d\n", nNewChannel);fflush(stdout);
			if(nNewChannel == -1) { // restore orig. bouquet and selected channel on cancel
				SetChannelMode(old_mode);
				bouquetList->activateBouquet(old_b, false);
				if(bouquetList->Bouquets.size())
					bouquetList->Bouquets[old_b]->channelList->setSelected(old_num-1);
			}
			else if(nNewChannel == -3) { // list mode changed
				printf("************************* ZAP NEW MODE: bouquetList %x size %d\n", (int) bouquetList, bouquetList->Bouquets.size());fflush(stdout);
				nNewChannel = bouquetList->exec(true);
				goto _repeat;
			}
			else if(nNewChannel == -4) {
				if(old_b_id < 0) old_b_id = old_b;
				g_Zapit->saveBouquets();
			}

			if(g_settings.mode_clock)
				InfoClock->StartClock();
			if(mode == mode_tv)
				dvbsub_start(0);
			return messages_return::handled;
		}
	}
	else if (msg == CRCInput::RC_standby) {
		if (data == 0) {
			neutrino_msg_t new_msg;

			/* Note: pressing the power button on the dbox (not the remote control) over 1 second */
			/*       shuts down the system even if !g_settings.shutdown_real_rcdelay (see below)  */
			gettimeofday(&standby_pressed_at, NULL);

			if ((mode != mode_standby) && (g_settings.shutdown_real)) {
				new_msg = NeutrinoMessages::SHUTDOWN;
			}
			else {
				new_msg = (mode == mode_standby) ? NeutrinoMessages::STANDBY_OFF : NeutrinoMessages::STANDBY_ON;
				//printf("standby: new msg %X\n", new_msg);
				if ((g_settings.shutdown_real_rcdelay)) {
					neutrino_msg_t      msg;
					neutrino_msg_data_t data;
					struct timeval      endtime;
					time_t              seconds;

					int timeout = 0;
					int timeout1 = 0;

					sscanf(g_settings.repeat_blocker, "%d", &timeout);
					sscanf(g_settings.repeat_genericblocker, "%d", &timeout1);

					if (timeout1 > timeout)
						timeout = timeout1;

					timeout += 500;
					//printf("standby: timeout %d\n", timeout);

					while(true) {
						g_RCInput->getMsg_ms(&msg, &data, timeout);

						//printf("standby: input msg %X\n", msg);
						if (msg == CRCInput::RC_timeout)
							break;

						gettimeofday(&endtime, NULL);
						seconds = endtime.tv_sec - standby_pressed_at.tv_sec;
						if (endtime.tv_usec < standby_pressed_at.tv_usec)
							seconds--;
						//printf("standby: input seconds %d\n", seconds);
						if (seconds >= 1) {
							if (msg == CRCInput::RC_standby)
								new_msg = NeutrinoMessages::SHUTDOWN;
							break;
						}
					}
				}
			}
			g_RCInput->postMsg(new_msg, 0);
			return messages_return::cancel_all | messages_return::handled;
		}
		else  /* data == 1: KEY_POWER released                         */
			if (standby_pressed_at.tv_sec != 0) /* check if we received a KEY_POWER pressed event before */
			{                                   /* required for correct handling of KEY_POWER events of  */
				/* the power button on the dbox (not the remote control) */
				struct timeval endtime;
				gettimeofday(&endtime, NULL);
				time_t seconds = endtime.tv_sec - standby_pressed_at.tv_sec;
				if (endtime.tv_usec < standby_pressed_at.tv_usec)
					seconds--;
				if (seconds >= 1) {
					g_RCInput->postMsg(NeutrinoMessages::SHUTDOWN, 0);
					return messages_return::cancel_all | messages_return::handled;
				}
			}
	}
	else if ((msg == CRCInput::RC_plus) || (msg == CRCInput::RC_minus))
	{
		setVolume(msg, (mode != mode_scart));
		return messages_return::handled;
	}
#if 0
	else if ((msg == CRCInput::RC_left) || (msg == CRCInput::RC_right)) // FIXME temp, until new remote
	{
		setVolume(msg, (mode != mode_scart));
		return messages_return::handled;
	}
#endif
	else if( msg == CRCInput::RC_spkr ) {
		if( mode == mode_standby ) {
			//switch lcd off/on
			CVFD::getInstance()->togglePower();
		}
		else {
			//mute
			AudioMute( !current_muted, true);
		}
		return messages_return::handled;
	}
	else if( msg == CRCInput::RC_mode ) {
		videoSettings->nextMode();
		return messages_return::handled;
	}
	else if( msg == CRCInput::RC_next ) {
		videoSettings->next43Mode();
		return messages_return::handled;
	}
	else if( msg == CRCInput::RC_prev ) {
		videoSettings->SwitchFormat();
		return messages_return::handled;
	}
	/* ================================== MESSAGES ================================================ */
	else if (msg == NeutrinoMessages::EVT_VOLCHANGED) {
		//setVolume(msg, false, true);
		return messages_return::handled;
	}
	else if( msg == NeutrinoMessages::EVT_VCRCHANGED ) {
		if (g_settings.vcr_AutoSwitch) {
			if( data != VCR_STATUS_OFF )
				g_RCInput->postMsg( NeutrinoMessages::VCR_ON, 0 );
			else
				g_RCInput->postMsg( NeutrinoMessages::VCR_OFF, 0 );
		}
		return messages_return::handled | messages_return::cancel_info;
	}
	else if( msg == NeutrinoMessages::EVT_MUTECHANGED ) {
		CControldMsg::commandMute* cmd = (CControldMsg::commandMute*) data;
		if(cmd->type == (CControld::volume_type)g_settings.audio_avs_Control)
			AudioMute( cmd->mute, true );
		delete[] (unsigned char*) data;
		return messages_return::handled;
	}
	else if( msg == NeutrinoMessages::EVT_SERVICESCHANGED ) {
		channelsInit();
		channelList->adjustToChannelID(live_channel_id);//FIXME what if deleted ?
		if(old_b_id >= 0) {
			bouquetList->activateBouquet(old_b_id, false);
			old_b_id = -1;
			g_RCInput->postMsg(CRCInput::RC_ok, 0);
		} 
	}
	else if( msg == NeutrinoMessages::EVT_BOUQUETSCHANGED ) {
		channelsInit();
		channelList->adjustToChannelID(live_channel_id);//FIXME what if deleted ?
		return messages_return::handled;
	}
	else if( msg == NeutrinoMessages::EVT_RECORDMODE ) {
		/* sent by rcinput, then got msg from zapit about record activated/deactivated */
		dprintf(DEBUG_DEBUG, "neutino - recordmode %s\n", ( data ) ? "on":"off" );
		if(!recordingstatus && was_record && (!data)) {
			g_Zapit->setStandby(true);
			was_record = 0;
			if( mode == mode_standby )
				cpuFreq->SetCpuFreq(g_settings.standby_cpufreq * 1000 * 1000);
		}
		if( mode == mode_standby && data )
			was_record = 1;
		recordingstatus = data;
		if( ( !g_InfoViewer->is_visible ) && data && !autoshift)
			g_RCInput->postMsg( NeutrinoMessages::SHOW_INFOBAR, 0 );

		return messages_return::handled;
	}
	else if (msg == NeutrinoMessages::RECORD_START) {
		if( mode == mode_standby )
			cpuFreq->SetCpuFreq(g_settings.cpufreq * 1000 * 1000);

		if(autoshift) {
			stopAutoRecord();
			recordingstatus = 0;
		}
		puts("[neutrino.cpp] executing " NEUTRINO_RECORDING_START_SCRIPT ".");
		if (system(NEUTRINO_RECORDING_START_SCRIPT) != 0)
			perror(NEUTRINO_RECORDING_START_SCRIPT " failed");
		/* set nextRecordingInfo to current event (replace other scheduled recording if available) */
		/* Note: CTimerd::RecordingInfo is a class!
		 * => typecast to avoid destructor call */

		if (nextRecordingInfo != NULL)
			delete[] (unsigned char *) nextRecordingInfo;

		nextRecordingInfo = (CTimerd::RecordingInfo *) data;
		startNextRecording();

		return messages_return::handled | messages_return::cancel_all;
	}
	else if( msg == NeutrinoMessages::RECORD_STOP) {
		if(((CTimerd::RecordingStopInfo*)data)->eventID == recording_id)
		{ // passendes stop zur Aufnahme
			//CVCRControl * vcr_control = CVCRControl::getInstance();
			if (CVCRControl::getInstance()->isDeviceRegistered()) {
				if ((CVCRControl::getInstance()->getDeviceState() == CVCRControl::CMD_VCR_RECORD) ||
						(CVCRControl::getInstance()->getDeviceState() == CVCRControl::CMD_VCR_PAUSE ))
				{
					CVCRControl::getInstance()->Stop();
					recordingstatus = 0;
					autoshift = 0;
				}
				else
					printf("falscher state\n");
			}
			else
				puts("[neutrino.cpp] no recording devices");

			startNextRecording();

			if (recordingstatus == 0) {
				puts("[neutrino.cpp] executing " NEUTRINO_RECORDING_ENDED_SCRIPT ".");
				if (system(NEUTRINO_RECORDING_ENDED_SCRIPT) != 0)
					perror(NEUTRINO_RECORDING_ENDED_SCRIPT " failed");
				if( mode == mode_standby )
					cpuFreq->SetCpuFreq(g_settings.standby_cpufreq * 1000 * 1000);
			}
		}
		else if(nextRecordingInfo!=NULL) {
			if(((CTimerd::RecordingStopInfo*)data)->eventID == nextRecordingInfo->eventID) {
				/* Note: CTimerd::RecordingInfo is a class!
				 * => typecast to avoid destructor call */
				delete[] (unsigned char *) nextRecordingInfo;
				nextRecordingInfo=NULL;
			}
		}
		delete[] (unsigned char*) data;
		return messages_return::handled;
	}
	else if( msg == NeutrinoMessages::ZAPTO) {
		CTimerd::EventInfo * eventinfo;
		eventinfo = (CTimerd::EventInfo *) data;
		if(recordingstatus==0) {
			bool isTVMode = g_Zapit->isChannelTVChannel(eventinfo->channel_id);

			dvbsub_stop();

			if ((!isTVMode) && (mode != mode_radio)) {
				radioMode(false);
			}
			else if (isTVMode && (mode != mode_tv)) {
				tvMode(false);
			}
			channelList->zapTo_ChannelID(eventinfo->channel_id);
		}
		delete[] (unsigned char*) data;
		return messages_return::handled;
	}
	else if( msg == NeutrinoMessages::ANNOUNCE_ZAPTO) {
		if( mode == mode_standby ) {
			standbyMode( false );
		}
		if( mode != mode_scart ) {
			std::string name = g_Locale->getText(LOCALE_ZAPTOTIMER_ANNOUNCE);

			CTimerd::TimerList tmpTimerList;
			CTimerdClient tmpTimerdClient;

			tmpTimerList.clear();
			tmpTimerdClient.getTimerList( tmpTimerList );

			if(tmpTimerList.size() > 0) {
				sort( tmpTimerList.begin(), tmpTimerList.end() );

				CTimerd::responseGetTimer &timer = tmpTimerList[0];

				CZapitClient Zapit;
				name += "\n";

				std::string zAddData = Zapit.getChannelName( timer.channel_id ); // UTF-8
				if( zAddData.empty()) {
					zAddData = g_Locale->getText(LOCALE_TIMERLIST_PROGRAM_UNKNOWN);
				}

				if(timer.epgID!=0) {
					CEPGData epgdata;
					zAddData += " :\n";
					//if (g_Sectionsd->getEPGid(timer.epgID, timer.epg_starttime, &epgdata))
					if (sectionsd_getEPGid(timer.epgID, timer.epg_starttime, &epgdata)) {
						zAddData += epgdata.title;
					}
					else if(strlen(timer.epgTitle)!=0) {
						zAddData += timer.epgTitle;
					}
				}
				else if(strlen(timer.epgTitle)!=0) {
					zAddData += timer.epgTitle;
				}

				name += zAddData;
			}
			ShowHintUTF( LOCALE_MESSAGEBOX_INFO, name.c_str() );
		}

		return messages_return::handled;
	}
	else if( msg == NeutrinoMessages::ANNOUNCE_RECORD) {
		system(NEUTRINO_RECORDING_TIMER_SCRIPT);
		if( g_settings.recording_server_wakeup ) {
			std::string command = "etherwake ";
			command += g_settings.recording_server_mac;
			if(system(command.c_str()) != 0)
				perror("etherwake failed");
		}
		if (g_settings.recording_type == RECORDING_FILE) {
			char * recDir = ((CTimerd::RecordingInfo*)data)->recordingDir;
			for(int i=0 ; i < NETWORK_NFS_NR_OF_ENTRIES ; i++) {
				if (strcmp(g_settings.network_nfs_local_dir[i],recDir) == 0) {
					printf("[neutrino] waking up %s (%s)\n",g_settings.network_nfs_ip[i].c_str(),recDir);
					std::string command = "etherwake ";
					command += g_settings.network_nfs_mac[i];
					if(system(command.c_str()) != 0)
						perror("etherwake failed");
					break;
				}
			}
			if(autoshift) {
				stopAutoRecord();
				recordingstatus = 0;
			}
			if(has_hdd) {
				system("(rm /media/sda1/.wakeup; touch /media/sda1/.wakeup; sync) > /dev/null  2> /dev/null &"); // wakeup hdd
			}
		}
		if( g_settings.recording_zap_on_announce ) {
			if(recordingstatus==0) {
				dvbsub_stop(); //FIXME if same channel ?
				t_channel_id channel_id=((CTimerd::RecordingInfo*)data)->channel_id;
				g_Zapit->zapTo_serviceID_NOWAIT(channel_id); 
			}
		}
		delete[] (unsigned char*) data;
		if( mode != mode_scart )
			ShowHintUTF(LOCALE_MESSAGEBOX_INFO, g_Locale->getText(LOCALE_RECORDTIMER_ANNOUNCE));
		return messages_return::handled;
	}
	else if( msg == NeutrinoMessages::ANNOUNCE_SLEEPTIMER) {
		if( mode != mode_scart )
			ShowHintUTF(LOCALE_MESSAGEBOX_INFO, g_Locale->getText(LOCALE_SLEEPTIMERBOX_ANNOUNCE));
		return messages_return::handled;
	}
	else if( msg == NeutrinoMessages::SLEEPTIMER) {
		if(g_settings.shutdown_real)
			ExitRun(true);
		else
			standbyMode( true );
		return messages_return::handled;
	}
	else if( msg == NeutrinoMessages::STANDBY_TOGGLE ) {
		standbyMode( !(mode & mode_standby) );
		g_RCInput->clearRCMsg();
		return messages_return::handled;
	}
	else if( msg == NeutrinoMessages::STANDBY_ON ) {
		if( mode != mode_standby ) {
			standbyMode( true );
		}
		g_RCInput->clearRCMsg();
		return messages_return::handled;
	}
	else if( msg == NeutrinoMessages::STANDBY_OFF ) {
		if( mode == mode_standby ) {
			standbyMode( false );
		}
		g_RCInput->clearRCMsg();
		return messages_return::handled;
	}
	else if( msg == NeutrinoMessages::ANNOUNCE_SHUTDOWN) {
		if( mode != mode_scart )
			skipShutdownTimer = (ShowLocalizedMessage(LOCALE_MESSAGEBOX_INFO, LOCALE_SHUTDOWNTIMER_ANNOUNCE, CMessageBox::mbrNo, CMessageBox::mbYes | CMessageBox::mbNo, NULL, 450, 5) == CMessageBox::mbrYes);
	}
	else if( msg == NeutrinoMessages::SHUTDOWN ) {
		if(!skipShutdownTimer) {
			ExitRun(true);
		}
		else {
			skipShutdownTimer=false;
		}
		return messages_return::handled;
	}
	else if (msg == NeutrinoMessages::EVT_POPUP) {
		if (mode != mode_scart)
			ShowHintUTF(LOCALE_MESSAGEBOX_INFO, (const char *) data); // UTF-8
		delete (unsigned char*) data;
		return messages_return::handled;
	}
	else if (msg == NeutrinoMessages::EVT_EXTMSG) {
		if (mode != mode_scart)
			ShowMsgUTF(LOCALE_MESSAGEBOX_INFO, (const char *) data, CMessageBox::mbrBack, CMessageBox::mbBack, NEUTRINO_ICON_INFO); // UTF-8
		delete[] (unsigned char*) data;
		return messages_return::handled;
	}
	else if (msg == NeutrinoMessages::EVT_RECORDING_ENDED) {
		if (mode != mode_scart) {
			neutrino_locale_t msgbody;
			if ((* (stream2file_status2_t *) data).status == STREAM2FILE_STATUS_BUFFER_OVERFLOW)
				msgbody = LOCALE_STREAMING_BUFFER_OVERFLOW;
			else if ((* (stream2file_status2_t *) data).status == STREAM2FILE_STATUS_WRITE_OPEN_FAILURE)
				msgbody = LOCALE_STREAMING_WRITE_ERROR_OPEN;
			else if ((* (stream2file_status2_t *) data).status == STREAM2FILE_STATUS_WRITE_FAILURE)
				msgbody = LOCALE_STREAMING_WRITE_ERROR;
			else
				goto skip_message;

			/* use a short timeout of only 5 seconds in case it was only a temporary network problem
			 * in case of STREAM2FILE_STATUS_IDLE we might even have to immediately start the next recording */
			ShowLocalizedMessage(LOCALE_MESSAGEBOX_INFO, msgbody, CMessageBox::mbrBack, CMessageBox::mbBack, NEUTRINO_ICON_INFO, 450, 5);

skip_message:
			;
		}
		if ((* (stream2file_status2_t *) data).status != STREAM2FILE_STATUS_IDLE) {
			/*
			 * note that changeNotify does not distinguish between LOCALE_MAINMENU_RECORDING_START and LOCALE_MAINMENU_RECORDING_STOP
			 * instead it checks the state of the variable recordingstatus
			 */
			/* restart recording */
			//FIXME doGuiRecord((*(stream2file_status2_t *) data).dir);
			//changeNotify(LOCALE_MAINMENU_RECORDING_START, data);
		}

		delete[] (unsigned char*) data;
		return messages_return::handled;
	}
	else if( msg == NeutrinoMessages::REMIND) {
		std::string text = (char*)data;
		std::string::size_type pos;
		while((pos=text.find('/'))!= std::string::npos)
		{
			text[pos] = '\n';
		}
		if( mode != mode_scart )
			ShowMsgUTF(LOCALE_TIMERLIST_TYPE_REMIND, text, CMessageBox::mbrBack, CMessageBox::mbBack, NEUTRINO_ICON_INFO); // UTF-8
		delete[] (unsigned char*) data;
		return messages_return::handled;
	}
	else if (msg == NeutrinoMessages::LOCK_RC) {
		this->rcLock->exec(NULL,CRCLock::NO_USER_INPUT);
		return messages_return::handled;
	}
	else if( msg == NeutrinoMessages::CHANGEMODE ) {
		int newmode = data & mode_mask;

		if((data & mode_mask)== mode_radio) {
			if( mode != mode_radio ) {
				if((data & norezap)==norezap)
					radioMode(false);
				else
					radioMode(true);
			}
		}
		if((data & mode_mask)== mode_tv) {
			if( mode != mode_tv ) {
				if((data & norezap)==norezap)
					tvMode(false);
				else
					tvMode(true);
			}
		}
		if((data &mode_mask)== mode_standby) {
			if(mode != mode_standby)
				standbyMode( true );
		}
		if((data &mode_mask)== mode_audio) {
			lastMode=mode;
			mode=mode_audio;
		}
		if((data &mode_mask)== mode_pic) {
			lastMode=mode;
			mode=mode_pic;
		}
		if((data &mode_mask)== mode_ts) {
			if(mode == mode_radio)
				videoDecoder->StopPicture();
			lastMode=mode;
			mode=mode_ts;
		}
	}
	else if( msg == NeutrinoMessages::VCR_ON ) {
		if( mode != mode_scart ) {
			scartMode( true );
		}
		else
			CVFD::getInstance()->setMode(CVFD::MODE_SCART);
	}

	else if( msg == NeutrinoMessages::VCR_OFF ) {
		if( mode == mode_scart ) {
			scartMode( false );
		}
	}
	else if (msg == NeutrinoMessages::EVT_START_PLUGIN) {
		g_PluginList->startPlugin((const char *)data);
		if (!g_PluginList->getScriptOutput().empty()) {
			ShowMsgUTF(LOCALE_PLUGINS_RESULT, g_PluginList->getScriptOutput(), CMessageBox::mbrBack,CMessageBox::mbBack,NEUTRINO_ICON_SHELL);
		}

		delete[] (unsigned char*) data;
		return messages_return::handled;
	}
	else if (msg == NeutrinoMessages::EVT_SERVICES_UPD) {
		ShowMsgUTF(LOCALE_MESSAGEBOX_INFO, g_Locale->getText(LOCALE_ZAPIT_SDTCHANGED),
				CMessageBox::mbrBack,CMessageBox::mbBack, NEUTRINO_ICON_INFO);
	}
	if ((msg >= CRCInput::RC_WithData) && (msg < CRCInput::RC_WithData + 0x10000000))
		delete [] (unsigned char*) data;

	return messages_return::unhandled;
}

void CNeutrinoApp::ExitRun(const bool write_si, int retcode)
{
	if (!recordingstatus ||
            ShowLocalizedMessage(LOCALE_MESSAGEBOX_INFO, LOCALE_SHUTDOWN_RECODING_QUERY, CMessageBox::mbrNo, CMessageBox::mbYes | CMessageBox::mbNo, NULL, 450, 30, true) == CMessageBox::mbrYes)
	{
		if(recordingstatus) {
			CVCRControl::getInstance()->Stop();
			g_Timerd->stopTimerEvent(recording_id);
		}
		CVFD::getInstance()->setMode(CVFD::MODE_SHUTDOWN);

		dprintf(DEBUG_INFO, "exit\n");
		g_Zapit->stopPlayBack();

		frameBuffer->paintBackground();
		videoDecoder->ShowPicture(DATADIR "/neutrino/icons/shutdown.mvi");

		networkConfig.automatic_start = (network_automatic_start == 1);
		networkConfig.commitConfig();
		saveSetup(NEUTRINO_SETTINGS_FILE);

		if(g_settings.epg_save /* && timeset && g_Sectionsd->getIsTimeSet ()*/) {
			saveEpg();
		}

			neutrino_msg_t      msg;
			neutrino_msg_data_t data;

			printf("entering off state\n");
			mode = mode_off;

			stop_daemons(false);

			system("/bin/sync");
			system("/bin/umount -a");
			cpuFreq->SetCpuFreq(g_settings.standby_cpufreq * 1000 * 1000);
			powerManager->SetStandby(true, true);
			//int fspeed = 0;
			//funNotifier->changeNotify(NONEXISTANT_LOCALE, (void *) &fspeed);

			if (powerManager) {
				powerManager->Close();
				delete powerManager;
			}
//			if (moviePlayerGui)
//				delete moviePlayerGui;
			//shutdown_cs_api();
			//system("/etc/init.d/rcK");
			CVFD::getInstance()->ShowIcon(VFD_ICON_CAM1, true);
			InfoClock->StopClock();
			if (g_RCInput != NULL)
				delete g_RCInput;

		if(retcode) {
			reboot(LINUX_REBOOT_CMD_RESTART);
/*			g_RCInput->clearRCMsg();
			while( true ) {
				g_RCInput->getMsg(&msg, &data, 10000);
				if( msg == CRCInput::RC_standby ) {
					printf("Power key, going to reboot...\n");
					sleep(2);
					reboot(LINUX_REBOOT_CMD_RESTART);
				} else if( ( msg == NeutrinoMessages::ANNOUNCE_RECORD) || ( msg == NeutrinoMessages::ANNOUNCE_ZAPTO) ) {
					printf("Zap/record timer, going to reboot...\n");
					sleep(2);
					reboot(LINUX_REBOOT_CMD_RESTART);
				}
			}
*/
		} else {
			reboot(LINUX_REBOOT_CMD_POWER_OFF);
/*			stop_daemons();
			
			int fspeed = 0;
			funNotifier->changeNotify(NONEXISTANT_LOCALE, (void *) &fspeed);
			//exit(retcode);//There seems to be no retcode
			exit(1);
*/
		}
	}
}
void CNeutrinoApp::saveEpg()
{
	struct stat my_stat;    
	if(stat(g_settings.epg_dir.c_str(), &my_stat) == 0){
		printf("Saving EPG to %s....\n", g_settings.epg_dir.c_str());
		neutrino_msg_t      msg;
		neutrino_msg_data_t data;
		g_Sectionsd->writeSI2XML(g_settings.epg_dir.c_str());
		while( true ) {
			g_RCInput->getMsg(&msg, &data, 300); // 30 secs..
			if (( msg == CRCInput::RC_timeout ) || (msg == NeutrinoMessages::EVT_SI_FINISHED)) {
				//printf("Msg %x timeout %d EVT_SI_FINISHED %x\n", msg, CRCInput::RC_timeout, NeutrinoMessages::EVT_SI_FINISHED);
				break;
			}
		}
	}
}
void CNeutrinoApp::AudioMute( int newValue, bool isEvent )
{
	//printf("MUTE: val %d current %d event %d\n", newValue, current_muted, isEvent);
	int dx = 40;
	int dy = 40;
	int x = g_settings.screen_EndX-dx;
	int y = g_settings.screen_StartY;

	CVFD::getInstance()->setMuted(newValue);
	current_muted = newValue;

printf("AudioMute: current %d new %d isEvent: %d\n", current_muted, newValue, isEvent);
	//if( !isEvent )
		g_Zapit->muteAudio(current_muted);

	if( isEvent && ( mode != mode_scart ) && ( mode != mode_audio) && ( mode != mode_pic))
	{
		if( current_muted ) {
			frameBuffer->paintBoxRel(x, y, dx, dy, COL_INFOBAR_PLUS_0);
			frameBuffer->paintIcon(NEUTRINO_ICON_BUTTON_MUTE, x+5, y+5);
		}
		else
			frameBuffer->paintBackgroundBoxRel(x, y, dx, dy);
	}
}

void CNeutrinoApp::setvol(int vol, int avs)
{
	audioDecoder->setVolume(vol, vol);
}

void CNeutrinoApp::setVolume(const neutrino_msg_t key, const bool bDoPaint, bool nowait)
{
	neutrino_msg_t msg = key;

	int dx = 256;
	int dy = 40;
#if 0 // orig
	int x = (((g_settings.screen_EndX- g_settings.screen_StartX)- dx) / 2) + g_settings.screen_StartX;
	int y = g_settings.screen_EndY - 100;
#else

	int x = frameBuffer->getScreenX() + 10;
	int y = frameBuffer->getScreenY() + 0;
#endif
	int vol = g_settings.current_volume;

	fb_pixel_t * pixbuf = NULL;

	if(bDoPaint) {
		pixbuf = new fb_pixel_t[dx * dy];
		if(pixbuf!= NULL)
			frameBuffer->SaveScreen(x, y, dx, dy, pixbuf);

		frameBuffer->paintIcon("volume.raw",x,y, COL_INFOBAR);
		frameBuffer->paintBoxRel (x + 40, y+12, 200, 15, COL_INFOBAR_PLUS_0);
		g_volscale->reset();
		g_volscale->paint(x + 41, y + 12, g_settings.current_volume);
	}

	neutrino_msg_data_t data;

	unsigned long long timeoutEnd;

	do {
		if (msg <= CRCInput::RC_MaxRC) {
			if (msg == CRCInput::RC_plus || msg == CRCInput::RC_right) { //FIXME
				if (g_settings.current_volume < 100 - 2)
					g_settings.current_volume += 2;
				else
					g_settings.current_volume = 100;
			}
			else if (msg == CRCInput::RC_minus || msg == CRCInput::RC_left) { //FIXME
				if (g_settings.current_volume > 2)
					g_settings.current_volume -= 2;
				else
					g_settings.current_volume = 0;
			}
			else {
				g_RCInput->postMsg(msg, data);
				break;
			}
			
			setvol(g_settings.current_volume,(g_settings.audio_avs_Control));
			//timeoutEnd = CRCInput::calcTimeoutEnd(nowait ? 1 : g_settings.timing[SNeutrinoSettings::TIMING_INFOBAR] / 2);
			timeoutEnd = CRCInput::calcTimeoutEnd(nowait ? 1 : 3);
		}
		else if (msg == NeutrinoMessages::EVT_VOLCHANGED) {
			//current_volume = g_Controld->getVolume((CControld::volume_type)g_settings.audio_avs_Control);//FIXME
//printf("setVolume EVT_VOLCHANGED %d\n", current_volume);
			//timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_INFOBAR] / 2);
			timeoutEnd = CRCInput::calcTimeoutEnd(3);
		}
		else if (handleMsg(msg, data) & messages_return::unhandled) {
			g_RCInput->postMsg(msg, data);
			break;
		}

		if (bDoPaint) {
			if(vol != g_settings.current_volume) {
				vol = g_settings.current_volume;
				g_volscale->paint(x + 41, y + 12, g_settings.current_volume);
			}
		}

		CVFD::getInstance()->showVolume(g_settings.current_volume);
		if (msg != CRCInput::RC_timeout) {
			g_RCInput->getMsgAbsoluteTimeout(&msg, &data, &timeoutEnd );
		}
	} while (msg != CRCInput::RC_timeout);

	//frameBuffer->paintBackgroundBoxRel(x, y, dx, dy); //FIXME osd bug

	if( (bDoPaint) && (pixbuf!= NULL) ) {
		frameBuffer->RestoreScreen(x, y, dx, dy, pixbuf);
		delete [] pixbuf;
	}
}

void CNeutrinoApp::tvMode( bool rezap )
{
	if(mode==mode_radio ) {
		videoDecoder->StopPicture();
		g_RCInput->killTimer(g_InfoViewer->lcdUpdateTimer);
		g_InfoViewer->lcdUpdateTimer = g_RCInput->addTimer( LCD_UPDATE_TIME_TV_MODE, false );
		CVFD::getInstance()->ShowIcon(VFD_ICON_RADIO, false);
		if(!rezap)
			dvbsub_start(0);
	}

	CVFD::getInstance()->setMode(CVFD::MODE_TVRADIO);
	CVFD::getInstance()->ShowIcon(VFD_ICON_TV, true);

        if( mode == mode_tv ) {
                if(g_settings.mode_clock) {
                        InfoClock->StopClock();
                        g_settings.mode_clock=false;
                } else {
                        InfoClock->StartClock();
                        g_settings.mode_clock=true;
                }
                return;
	} else if( mode == mode_scart ) {
		//g_Controld->setScartMode( 0 );
	} else if( mode == mode_standby ) {
		CVFD::getInstance()->setMode(CVFD::MODE_TVRADIO);
		videoDecoder->Standby(false);
	}

	bool stopauto = (mode != mode_ts);
	mode = mode_tv;
	if(stopauto && autoshift) {
//printf("standby on: autoshift ! stopping ...\n");
		stopAutoRecord();
		recordingstatus = 0;
	}

	frameBuffer->useBackground(false);
	frameBuffer->paintBackground();

	g_RemoteControl->tvMode();
	SetChannelMode(g_settings.channel_mode);//FIXME needed ?
	if( rezap ) {
		firstChannel();
		channelList->tuned = 0xfffffff;;
		channelList->zapTo( firstchannel.channelNumber -1 );
	}
#ifdef USEACTIONLOG
	g_ActionLog->println("mode: tv");
#endif
}

void CNeutrinoApp::scartMode( bool bOnOff )
{
	//printf( ( bOnOff ) ? "mode: scart on\n" : "mode: scart off\n" );

	if( bOnOff ) {
		// SCART AN
		frameBuffer->useBackground(false);
		frameBuffer->paintBackground();

		//g_Controld->setScartMode( 1 );
		CVFD::getInstance()->setMode(CVFD::MODE_SCART);
		lastMode = mode;
		mode = mode_scart;
	} else {
		// SCART AUS
		//g_Controld->setScartMode( 0 );

		mode = mode_unknown;
		//re-set mode
		if( lastMode == mode_radio ) {
			radioMode( false );
		}
		else if( lastMode == mode_tv ) {
			tvMode( false );
		}
		else if( lastMode == mode_standby ) {
			standbyMode( true );
		}
	}
}

void CNeutrinoApp::standbyMode( bool bOnOff )
{
	static bool wasshift = false;
//printf("********* CNeutrinoApp::standbyMode, was_record %d bOnOff %d\n", was_record, bOnOff);
	//printf( ( bOnOff ) ? "mode: standby on\n" : "mode: standby off\n" );
	if( bOnOff ) {
		puts("[neutrino.cpp] executing " NEUTRINO_ENTER_STANDBY_SCRIPT ".");
		if (system(NEUTRINO_ENTER_STANDBY_SCRIPT) != 0)
			perror(NEUTRINO_ENTER_STANDBY_SCRIPT " failed");

		if(autoshift) {
			stopAutoRecord();
			wasshift = true;
			recordingstatus = 0;
		}
		if( mode == mode_scart ) {
			//g_Controld->setScartMode( 0 );
		}
		dvbsub_pause();

		frameBuffer->useBackground(false);
		frameBuffer->paintBackground();

		CVFD::getInstance()->Clear();
		CVFD::getInstance()->setMode(CVFD::MODE_STANDBY);

		videoDecoder->Standby(true);
		if(recordingstatus) was_record = 1;
		if(!was_record) {
			g_Zapit->setStandby(true);
			system("hdparm -y /dev/ide/host0/bus0/target0/lun0/disc >/dev/null 2>/dev/null");
		} else
			g_Zapit->stopPlayBack();

		g_Sectionsd->setPauseScanning(true);
		if(g_settings.epg_save) {
			saveEpg();
		}
                if(g_settings.mode_clock)
                        InfoClock->StopClock();


		lastMode = mode;
		mode = mode_standby;
		int fspeed = 1;
		funNotifier->changeNotify(NONEXISTANT_LOCALE, (void *) &fspeed);

		frameBuffer->setActive(false);
		// Active standby on
		powerManager->SetStandby(true, false);
		if(!was_record)
			cpuFreq->SetCpuFreq(g_settings.standby_cpufreq * 1000 * 1000);
	} else {
		// Active standby off
		cpuFreq->SetCpuFreq(g_settings.cpufreq * 1000 * 1000);

		powerManager->SetStandby(false, false);
		frameBuffer->setActive(true);

		funNotifier->changeNotify(NONEXISTANT_LOCALE, (void*) &g_settings.fan_speed);
		puts("[neutrino.cpp] executing " NEUTRINO_LEAVE_STANDBY_SCRIPT ".");
		if (system(NEUTRINO_LEAVE_STANDBY_SCRIPT) != 0)
			perror(NEUTRINO_LEAVE_STANDBY_SCRIPT " failed");

		CVFD::getInstance()->setMode(CVFD::MODE_TVRADIO);
		g_Zapit->setStandby(false);
		if(was_record) 
			g_Zapit->startPlayBack();
		if(recordingstatus) was_record = 0;

		videoDecoder->Standby(false);
		g_Sectionsd->setPauseScanning(false);

                if(g_settings.mode_clock)
                        InfoClock->StartClock();

		mode = mode_unknown;
		if( lastMode == mode_radio ) {
			radioMode( false );
		} else {
			tvMode( false );
		}
		AudioMute(current_muted, false );
		if((mode == mode_tv) && wasshift) {
			startAutoRecord(true);
		}
		wasshift = false;
		dvbsub_start(0);
	}
}

void CNeutrinoApp::radioMode( bool rezap)
{
printf("radioMode: rezap %s\n", rezap ? "yes" : "no");
	if(mode==mode_tv ) {
		g_RCInput->killTimer(g_InfoViewer->lcdUpdateTimer);
		g_InfoViewer->lcdUpdateTimer = g_RCInput->addTimer( LCD_UPDATE_TIME_RADIO_MODE, false );
		CVFD::getInstance()->ShowIcon(VFD_ICON_TV, false);
		dvbsub_pause();
	}
	CVFD::getInstance()->setMode(CVFD::MODE_TVRADIO);
	CVFD::getInstance()->ShowIcon(VFD_ICON_RADIO, true);

	if( mode==mode_radio ) {
                if(g_settings.mode_clock) {
                        InfoClock->StopClock();
                        g_settings.mode_clock=false;
                } else {
                        InfoClock->StartClock();
                        g_settings.mode_clock=true;
                }
		return;
	}
	else if( mode == mode_scart ) {
		//g_Controld->setScartMode( 0 );
	}
	else if( mode == mode_standby ) {
		CVFD::getInstance()->setMode(CVFD::MODE_TVRADIO);
		videoDecoder->Standby(false);
	}
	mode = mode_radio;
	if(autoshift) {
//printf("standby on: autoshift ! stopping ...\n");
		stopAutoRecord();
		recordingstatus = 0;
	}

	g_RemoteControl->radioMode();
	SetChannelMode(g_settings.channel_mode);//FIXME needed?
	if( rezap ) {
		firstChannel();
		channelList->tuned = 0xfffffff;;
		channelList->zapTo( firstchannel.channelNumber -1 );
	}
	videoDecoder->ShowPicture(DATADIR "/neutrino/icons/radiomode.mvi");
}

void CNeutrinoApp::startNextRecording()
{
	if ((recordingstatus == 0) && (nextRecordingInfo != NULL)) {
		bool doRecord = true;
		if (CVCRControl::getInstance()->isDeviceRegistered()) {
			recording_id = nextRecordingInfo->eventID;
			if (g_settings.recording_type == RECORDING_FILE) {
				char *recDir = strlen(nextRecordingInfo->recordingDir) > 0 ?
					nextRecordingInfo->recordingDir : g_settings.network_nfs_recordingdir;
				if (!CFSMounter::isMounted(recDir)) {
					doRecord = false;
					for(int i=0 ; i < NETWORK_NFS_NR_OF_ENTRIES ; i++) {
						if (strcmp(g_settings.network_nfs_local_dir[i],recDir) == 0) {
							CFSMounter::MountRes mres =
								CFSMounter::mount(g_settings.network_nfs_ip[i].c_str(), g_settings.network_nfs_dir[i],
										  g_settings.network_nfs_local_dir[i], (CFSMounter::FSType) g_settings.network_nfs_type[i],
										  g_settings.network_nfs_username[i], g_settings.network_nfs_password[i],
										  g_settings.network_nfs_mount_options1[i], g_settings.network_nfs_mount_options2[i]);
							if (mres == CFSMounter::MRES_OK) {
								doRecord = true;
							} else {
								const char * merr = mntRes2Str(mres);
								int msglen = strlen(merr) + strlen(nextRecordingInfo->recordingDir) + 7;
								char msg[msglen];
								strcpy(msg,merr);
								strcat(msg,"\nDir: ");
								strcat(msg,nextRecordingInfo->recordingDir);

								ShowHintUTF(LOCALE_MESSAGEBOX_ERROR, msg); // UTF-8
								doRecord = false;
							}
							break;
						}
					}
					if (!doRecord) {
						// recording dir does not seem to exist in config anymore
						// or an error occured while mounting
						// -> try default dir
						recDir = g_settings.network_nfs_recordingdir;
						doRecord = true;
					}
				}
				(static_cast<CVCRControl::CFileDevice*>(recordingdevice))->Directory = std::string(recDir);
printf("CNeutrinoApp::startNextRecording: start to dir %s\n", recDir);
			}
			if(doRecord && CVCRControl::getInstance()->Record(nextRecordingInfo))
				recordingstatus = 1;
			else
				recordingstatus = 0;
		}
		else
			puts("[neutrino.cpp] no recording devices");

		/* Note: CTimerd::RecordingInfo is a class!
		 * What a brilliant idea to send classes via the eventserver!
		 * => typecast to avoid destructor call
		 */
		delete [](unsigned char *)nextRecordingInfo;

		nextRecordingInfo = NULL;
	}
}
/**************************************************************************************
*          CNeutrinoApp -  exec, menuitem callback (shutdown)                         *
**************************************************************************************/
void SaveMotorPositions();
int CNeutrinoApp::exec(CMenuTarget* parent, const std::string & actionKey)
{
	//	printf("ac: %s\n", actionKey.c_str());
	int returnval = menu_return::RETURN_REPAINT;

	if(actionKey == "help_recording") {
		ShowLocalizedMessage(LOCALE_SETTINGS_HELP, LOCALE_RECORDINGMENU_HELP, CMessageBox::mbrBack, CMessageBox::mbBack);
	}
	else if(actionKey=="shutdown") {
		ExitRun(true);
		returnval = menu_return::RETURN_NONE;
	}
	else if(actionKey=="reboot")
	{
//		FILE *f = fopen("/tmp/.reboot", "w");
//		fclose(f);
		ExitRun(true, 1);
//		unlink("/tmp/.reboot");
		returnval = menu_return::RETURN_NONE;
	}
	else if(actionKey=="tv") {
		tvMode();
		returnval = menu_return::RETURN_EXIT_ALL;
	}
	else if(actionKey=="radio") {
		radioMode();
		returnval = menu_return::RETURN_EXIT_ALL;
	}
	else if(actionKey=="scart") {
		g_RCInput->postMsg( NeutrinoMessages::VCR_ON, 0 );
		returnval = menu_return::RETURN_EXIT_ALL;
	}
	else if(actionKey=="network") {
		networkConfig.automatic_start = (network_automatic_start == 1);
		networkConfig.stopNetwork();
		networkConfig.commitConfig();
		networkConfig.startNetwork();
	}
	else if(actionKey=="networktest") {
		dprintf(DEBUG_INFO, "doing network test...\n");
		testNetworkSettings(networkConfig.address.c_str(), networkConfig.netmask.c_str(), networkConfig.broadcast.c_str(), networkConfig.gateway.c_str(), networkConfig.nameserver.c_str(), networkConfig.inet_static);
	}
	else if(actionKey=="networkshow") {
		dprintf(DEBUG_INFO, "showing current network settings...\n");
		showCurrentNetworkSettings();
	}
	else if (actionKey=="theme_neutrino") {
		setupColors_neutrino();
		colorSetupNotifier->changeNotify(NONEXISTANT_LOCALE, NULL);
	}
	else if (actionKey=="theme_classic") {
		setupColors_classic();
		colorSetupNotifier->changeNotify(NONEXISTANT_LOCALE, NULL);
	}
	else if (actionKey=="theme_dblue") {
		setupColors_dblue();
		colorSetupNotifier->changeNotify(NONEXISTANT_LOCALE, NULL);
	}
	else if (actionKey=="theme_dvb2k") {
		setupColors_dvb2k();
		colorSetupNotifier->changeNotify(NONEXISTANT_LOCALE, NULL);
	}
	else if(actionKey=="theme_ru") {
		setupColors_ru();
		colorSetupNotifier->changeNotify(NONEXISTANT_LOCALE, NULL);
	}
	else if(actionKey=="theme_red") {
		setupColors_red();
		colorSetupNotifier->changeNotify(NONEXISTANT_LOCALE, NULL);
	}
	else if(actionKey=="savescansettings") {
		SaveMotorPositions();
	}
	else if(actionKey=="savesettings") {
		CHintBox * hintBox = new CHintBox(LOCALE_MESSAGEBOX_INFO, g_Locale->getText(LOCALE_MAINSETTINGS_SAVESETTINGSNOW_HINT)); // UTF-8
		hintBox->paint();

		networkConfig.automatic_start = (network_automatic_start == 1);
		networkConfig.commitConfig();
		saveSetup(NEUTRINO_SETTINGS_FILE);
		SaveMotorPositions();

		zapitCfg.gotoXXLatitude = strtod(zapit_lat, NULL);
		zapitCfg.gotoXXLongitude = strtod(zapit_long, NULL);

		setZapitConfig(&zapitCfg);
		if(g_settings.cacheTXT) {
			tuxtxt_init();
		} else
			tuxtxt_close();

		//g_Sectionsd->setEventsAreOldInMinutes((unsigned short) (g_settings.epg_old_hours*60));
		//g_Sectionsd->setHoursToCache((unsigned short) (g_settings.epg_cache_days*24));
		hintBox->hide();
		delete hintBox;
	}
	else if(actionKey=="recording") {
		setupRecordingDevice();
	}
	else if(actionKey=="reloadchannels") {
		//reloadhintBox->paint();
		g_Zapit->reinitChannels();
#if 0
		system(mode == mode_radio
				? "wget -q -O /dev/null http://127.0.0.1/control/setmode?radio > /dev/null 2>&1"
				: "wget -q -O /dev/null http://127.0.0.1/control/setmode?tv > /dev/null 2>&1");
#endif
		//reloadhintBox->hide();

	}
	else if(actionKey=="reloadplugins") {
		CHintBox * hintBox = new CHintBox(LOCALE_MESSAGEBOX_INFO, g_Locale->getText(LOCALE_SERVICEMENU_GETPLUGINS_HINT));
		hintBox->paint();

		g_PluginList->loadPlugins();

		hintBox->hide();
		delete hintBox;
	}
	else if(actionKey=="restart") {
		if (recordingstatus)
			DisplayErrorMessage(g_Locale->getText(LOCALE_SERVICEMENU_RESTART_REFUSED_RECORDING));
		else {
			CHintBox * hintBox = new CHintBox(LOCALE_MESSAGEBOX_INFO, g_Locale->getText(LOCALE_SERVICEMENU_RESTART_HINT));
			hintBox->paint();

			stop_daemons();

			delete g_RCInput;
			delete g_Sectionsd;
			delete g_Timerd;
			delete g_RemoteControl;
			delete g_fontRenderer;
			delete g_Zapit;
			delete CVFD::getInstance();

			networkConfig.automatic_start = (network_automatic_start == 1);
			networkConfig.commitConfig();
			saveSetup(NEUTRINO_SETTINGS_FILE);
			delete frameBuffer;
			execvp(global_argv[0], global_argv); // no return if successful
			exit(1);
		}
	}
	else if(strncmp(actionKey.c_str(), "fontsize.d", 10) == 0) {
		for (int i = 0; i < 6; i++) {
			if (actionKey == font_sizes_groups[i].actionkey)
				for (unsigned int j = 0; j < font_sizes_groups[i].count; j++) {
					SNeutrinoSettings::FONT_TYPES k = font_sizes_groups[i].content[j];
					configfile.setInt32(locale_real_names[neutrino_font[k].name], neutrino_font[k].defaultsize);
				}
		}
		fontsizenotifier.changeNotify(NONEXISTANT_LOCALE, NULL);
	}
	else if(actionKey=="osd.def") {
		for (int i = 0; i < TIMING_SETTING_COUNT; i++)
			g_settings.timing[i] = default_timing[i];

		SetupTiming();
	}
	else if(actionKey == "audioplayerdir") {
		parent->hide();
		CFileBrowser b;
		b.Dir_Mode=true;
		if (b.exec(g_settings.network_nfs_audioplayerdir))
			strncpy(g_settings.network_nfs_audioplayerdir, b.getSelectedFile()->Name.c_str(), sizeof(g_settings.network_nfs_audioplayerdir)-1);
		return menu_return::RETURN_REPAINT;
	}
	else if(actionKey == "picturedir") {
		parent->hide();
		CFileBrowser b;
		b.Dir_Mode=true;
		if (b.exec(g_settings.network_nfs_picturedir))
			strncpy(g_settings.network_nfs_picturedir, b.getSelectedFile()->Name.c_str(), sizeof(g_settings.network_nfs_picturedir)-1);
		return menu_return::RETURN_REPAINT;
	}
	else if(actionKey == "moviedir") {
		parent->hide();
		CFileBrowser b;
		b.Dir_Mode=true;
		if (b.exec(g_settings.network_nfs_moviedir))
			strncpy(g_settings.network_nfs_moviedir, b.getSelectedFile()->Name.c_str(), sizeof(g_settings.network_nfs_moviedir)-1);
		return menu_return::RETURN_REPAINT;
	}
	else if(actionKey == "recordingdir") {
		parent->hide();
		CFileBrowser b;
		b.Dir_Mode=true;
		if (b.exec(g_settings.network_nfs_recordingdir)) {
			const char * newdir = b.getSelectedFile()->Name.c_str();
printf("New recordingdir: selected %s\n", newdir);
			if(check_dir(newdir))
				printf("Wrong/unsupported recording dir %s\n", newdir);
			else {
				strncpy(g_settings.network_nfs_recordingdir, b.getSelectedFile()->Name.c_str(), sizeof(g_settings.network_nfs_recordingdir)-1);
printf("New recordingdir: %s (timeshift %s)\n", g_settings.network_nfs_recordingdir, g_settings.timeshiftdir);
				if(strlen(g_settings.timeshiftdir) == 0) {
					sprintf(timeshiftDir, "%s/.timeshift", g_settings.network_nfs_recordingdir);
					safe_mkdir(timeshiftDir);
printf("New timeshift dir: %s\n", timeshiftDir);
				}
			}
		}
		return menu_return::RETURN_REPAINT;
	}
	else if(actionKey == "timeshiftdir") {
		parent->hide();
		CFileBrowser b;
		b.Dir_Mode=true;
		if (b.exec(g_settings.timeshiftdir)) {
			const char * newdir = b.getSelectedFile()->Name.c_str();
printf("New timeshift: selected %s\n", newdir);
			if(check_dir(newdir))
				printf("Wrong/unsupported recording dir %s\n", newdir);
			else {
printf("New timeshift dir: old %s (record %s)\n", g_settings.timeshiftdir, g_settings.network_nfs_recordingdir);
				if(strcmp(newdir, g_settings.network_nfs_recordingdir)) {
printf("New timeshift != rec dir\n");
					strncpy(g_settings.timeshiftdir, b.getSelectedFile()->Name.c_str(), sizeof(g_settings.timeshiftdir)-1);
					strcpy(timeshiftDir, g_settings.timeshiftdir);
				} else {
					sprintf(timeshiftDir, "%s/.timeshift", g_settings.network_nfs_recordingdir);
					strcpy(g_settings.timeshiftdir, newdir);
					safe_mkdir(timeshiftDir);
printf("New timeshift == rec dir\n");
				}
printf("New timeshift dir: %s\n", timeshiftDir);
			}
		}
		return menu_return::RETURN_REPAINT;
	}
	else if(actionKey == "update_dir") {
		parent->hide();
		CFileBrowser fileBrowser;
		fileBrowser.Dir_Mode = true;
		if (fileBrowser.exec(g_settings.update_dir) == true) {
			const char * newdir = fileBrowser.getSelectedFile()->Name.c_str();
			if(check_dir(newdir))
				printf("Wrong/unsupported recording dir %s\n", newdir);
			else {
				strcpy(g_settings.update_dir, fileBrowser.getSelectedFile()->Name.c_str());
				printf("[neutrino] new update dir %s\n", fileBrowser.getSelectedFile()->Name.c_str());
			}
		}
		return menu_return::RETURN_REPAINT;
	}
	else if(actionKey == "epgdir") {
		parent->hide();
		CFileBrowser b;
		b.Dir_Mode=true;
		if (b.exec(g_settings.epg_dir.c_str())) {
			const char * newdir = b.getSelectedFile()->Name.c_str();
			if(check_dir(newdir))
				printf("Wrong/unsupported epg dir %s\n", newdir);
			else {
				g_settings.epg_dir = b.getSelectedFile()->Name;
				SendSectionsdConfig();
			}
		}

		return menu_return::RETURN_REPAINT;
	}
	else if(actionKey == "select_font") {
		parent->hide();
		CFileBrowser fileBrowser;
		CFileFilter fileFilter;
		fileFilter.addFilter("ttf");
		fileBrowser.Filter = &fileFilter;
		if (fileBrowser.exec(FONTDIR) == true) {
			strcpy(g_settings.font_file, fileBrowser.getSelectedFile()->Name.c_str());
			printf("[neutrino] new font file %s\n", fileBrowser.getSelectedFile()->Name.c_str());
			SetupFonts();
		}
		return menu_return::RETURN_REPAINT;
	}
	else if(actionKey == "movieplugin") {
		parent->hide();
		CMenuWidget MoviePluginSelector(LOCALE_MOVIEPLAYER_DEFPLUGIN, "features.raw", 350);
		MoviePluginSelector.addItem(GenericMenuSeparator);

		char id[5];
		int cnt = 0;
		int enabled_count = 0;
		for(unsigned int count=0;count < (unsigned int) g_PluginList->getNumberOfPlugins();count++) {
			if (g_PluginList->getType(count)== CPlugins::P_TYPE_TOOL && !g_PluginList->isHidden(count)) {
				// zB vtxt-plugins
				sprintf(id, "%d", count);
				enabled_count++;
				MoviePluginSelector.addItem(new CMenuForwarderNonLocalized(g_PluginList->getName(count), true, NULL, MoviePluginChanger, id, CRCInput::convertDigitToKey(count)), (cnt == 0));
				cnt++;
			}
		}

		MoviePluginSelector.exec(NULL, "");
		return menu_return::RETURN_REPAINT;
	}
	else if(actionKey == "onekeyplugin") {
		parent->hide();
		CMenuWidget MoviePluginSelector(LOCALE_EXTRA_KEY_PLUGIN, "features.raw", 350);
		MoviePluginSelector.addItem(GenericMenuSeparator);

		char id[5];
		int cnt = 0;
		int enabled_count = 0;
		for(unsigned int count=0;count < (unsigned int) g_PluginList->getNumberOfPlugins();count++) {
			if (g_PluginList->getType(count)== CPlugins::P_TYPE_TOOL && !g_PluginList->isHidden(count)) {
				// zB vtxt-plugins
				sprintf(id, "%d", count);
				enabled_count++;
				MoviePluginSelector.addItem(new CMenuForwarderNonLocalized(g_PluginList->getName(count), true, NULL, OnekeyPluginChanger, id, CRCInput::convertDigitToKey(count)), (cnt == 0));
				cnt++;
			}
		}

		MoviePluginSelector.exec(NULL, "");
		return menu_return::RETURN_REPAINT;
	}
	else if(actionKey == "loadconfig") {
		parent->hide();
		CFileBrowser fileBrowser;
		CFileFilter fileFilter;
		fileFilter.addFilter("conf");
		fileBrowser.Filter = &fileFilter;
		if (fileBrowser.exec("/var/tuxbox/config") == true) {
			loadSetup(fileBrowser.getSelectedFile()->Name.c_str());
			colorSetupNotifier->changeNotify(NONEXISTANT_LOCALE, NULL);
			CVFD::getInstance()->setlcdparameter();
			printf("[neutrino] new settings: %s\n", fileBrowser.getSelectedFile()->Name.c_str());
		}
		return menu_return::RETURN_REPAINT;
	}
	else if(actionKey == "saveconfig") {
		parent->hide();
		CFileBrowser fileBrowser;
		fileBrowser.Dir_Mode = true;
		if (fileBrowser.exec("/var/tuxbox") == true) {
			char  fname[256] = "neutrino.conf", sname[256];
			CStringInputSMS * sms = new CStringInputSMS(LOCALE_EXTRA_SAVECONFIG, fname, 30, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, "abcdefghijklmnopqrstuvwxyz0123456789. ");
			sms->exec(NULL, "");
			sprintf(sname, "%s/%s", fileBrowser.getSelectedFile()->Name.c_str(), fname);
			printf("[neutrino] save settings: %s\n", sname);
			saveSetup(sname);
			delete sms;
		}
		return menu_return::RETURN_REPAINT;
	}
	else if(actionKey == "loadcolors") {
		parent->hide();
		CFileBrowser fileBrowser;
		CFileFilter fileFilter;
		fileFilter.addFilter("conf");
		fileBrowser.Filter = &fileFilter;
		if (fileBrowser.exec("/var/tuxbox/config") == true) {
			loadColors(fileBrowser.getSelectedFile()->Name.c_str());
			//printf("[neutrino] new colors: %s\n", fileBrowser.getSelectedFile()->Name.c_str());
		}
		return menu_return::RETURN_REPAINT;
	}
	else if(actionKey == "savecolors") {
		parent->hide();
		CFileBrowser fileBrowser;
		fileBrowser.Dir_Mode = true;
		if (fileBrowser.exec("/var/tuxbox") == true) {
			char  fname[256] = "colors.conf", sname[256];
			CStringInputSMS * sms = new CStringInputSMS(LOCALE_EXTRA_SAVECOLORS, fname, 30, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, "abcdefghijklmnopqrstuvwxyz0123456789. ");
			sms->exec(NULL, "");
			sprintf(sname, "%s/%s", fileBrowser.getSelectedFile()->Name.c_str(), fname);
			printf("[neutrino] save colors: %s\n", sname);
			saveColors(sname);
			delete sms;
		}
		return menu_return::RETURN_REPAINT;
	}
	else if(actionKey == "loadkeys") {
		parent->hide();
		CFileBrowser fileBrowser;
		CFileFilter fileFilter;
		fileFilter.addFilter("conf");
		fileBrowser.Filter = &fileFilter;
		if (fileBrowser.exec("/var/tuxbox/config") == true) {
			loadKeys(fileBrowser.getSelectedFile()->Name.c_str());
			printf("[neutrino] new keys: %s\n", fileBrowser.getSelectedFile()->Name.c_str());
		}
		return menu_return::RETURN_REPAINT;
	}
	else if(actionKey == "savekeys") {
		parent->hide();
		CFileBrowser fileBrowser;
		fileBrowser.Dir_Mode = true;
		if (fileBrowser.exec("/var/tuxbox") == true) {
			char  fname[256] = "keys.conf", sname[256];
			CStringInputSMS * sms = new CStringInputSMS(LOCALE_EXTRA_SAVEKEYS, fname, 30, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, "abcdefghijklmnopqrstuvwxyz0123456789. ");
			sms->exec(NULL, "");
			sprintf(sname, "%s/%s", fileBrowser.getSelectedFile()->Name.c_str(), fname);
			printf("[neutrino] save keys: %s\n", sname);
			saveKeys(sname);
			delete sms;
		}
		return menu_return::RETURN_REPAINT;
	}
	else if(actionKey == "autolink") {
		if(autoshift) {
			char buf[512];
			autoshift = false;
			sprintf(buf, "ln %s/* %s", timeshiftDir, g_settings.network_nfs_recordingdir);
			system(buf);
			autoshift_delete = true;
			//sprintf(buf, "rm -f %s/* &", timeshiftDir);
			//system(buf);
		}
		returnval = menu_return::RETURN_EXIT_ALL;
	}
	else if(actionKey == "clearSectionsd")
	{
		g_Sectionsd->freeMemory();
	}

	return returnval;
}

/**************************************************************************************
*          changeNotify - features menu recording start / stop                        *
**************************************************************************************/
bool CNeutrinoApp::changeNotify(const neutrino_locale_t OptionName, void *data)
{
	if ((ARE_LOCALES_EQUAL(OptionName, LOCALE_MAINMENU_RECORDING_START)) || (ARE_LOCALES_EQUAL(OptionName, LOCALE_MAINMENU_RECORDING)))
	{
		if(g_RemoteControl->is_video_started) {
			bool res = doGuiRecord(NULL,ARE_LOCALES_EQUAL(OptionName, LOCALE_MAINMENU_RECORDING));
			return res;
		}
		else {
			recordingstatus = 0;
			return false;
		}
	}
	else if (ARE_LOCALES_EQUAL(OptionName, LOCALE_LANGUAGESETUP_SELECT)) {
		g_Locale->loadLocale(g_settings.language);
		return true;
	}
	return false;
}

/**************************************************************************************
*          Main programm - no function here                                           *
**************************************************************************************/
void stop_daemons(bool stopall)
{
	dvbsub_close();
	tuxtxt_stop();
	tuxtxt_close();

	printf("httpd shutdown\n");
	pthread_cancel(nhttpd_thread);
	pthread_join(nhttpd_thread, NULL);
	printf("httpd shutdown done\n");
	streamts_stop = 1;
	pthread_join(stream_thread, NULL);
	sectionsd_stop = 1;
	if(stopall) {
		printf("timerd shutdown\n");
		g_Timerd->shutdown();
		pthread_join(timer_thread, NULL);
		printf("timerd shutdown done\n");
	}
#ifndef DISABLE_SECTIONSD
	printf("sectionsd shutdown\n");
	pthread_join(sections_thread, NULL);
	printf("sectionsd shutdown done\n");
#endif
	printf("zapit shutdown\n");
	g_Zapit->shutdown();
	pthread_join(zapit_thread, NULL);
	printf("zapit shutdown done\n");
	CVFD::getInstance()->Clear();
	if(stopall) {
		delete moviePlayerGui;
		if (powerManager) {
			powerManager->Close();
			delete powerManager;
		}
		shutdown_cs_api();
	}
}

void sighandler (int signum)
{
	signal (signum, SIG_IGN);
        switch (signum) {
          case SIGTERM:
          case SIGINT:
		stop_daemons();
                _exit(0);
          default:
                break;
        }
}

int main(int argc, char **argv)
{
	setDebugLevel(DEBUG_NORMAL);
        signal(SIGTERM, sighandler);
        signal(SIGINT, sighandler);
        signal(SIGHUP, SIG_IGN);

	for(int i = 3; i < 256; i++)
		close(i);

	tzset();
	initGlobals();

	return CNeutrinoApp::getInstance()->run(argc, argv);
}

void CNeutrinoApp::loadColors(const char * fname)
{
	bool res;
	CConfigFile tconfig(',', true);

	printf("CNeutrinoApp::loadColors: %s\n", fname);

	res = tconfig.loadConfig(fname);
	if(!res) return;
	//colors (neutrino defaultcolors)
	g_settings.menu_Head_alpha = tconfig.getInt32( "menu_Head_alpha", 0x00 );
	g_settings.menu_Head_red = tconfig.getInt32( "menu_Head_red", 0x00 );
	g_settings.menu_Head_green = tconfig.getInt32( "menu_Head_green", 0x0A );
	g_settings.menu_Head_blue = tconfig.getInt32( "menu_Head_blue", 0x19 );

	g_settings.menu_Head_Text_alpha = tconfig.getInt32( "menu_Head_Text_alpha", 0x00 );
	g_settings.menu_Head_Text_red = tconfig.getInt32( "menu_Head_Text_red", 0x5f );
	g_settings.menu_Head_Text_green = tconfig.getInt32( "menu_Head_Text_green", 0x46 );
	g_settings.menu_Head_Text_blue = tconfig.getInt32( "menu_Head_Text_blue", 0x00 );

	g_settings.menu_Content_alpha = tconfig.getInt32( "menu_Content_alpha", 0x14 );
	g_settings.menu_Content_red = tconfig.getInt32( "menu_Content_red", 0x00 );
	g_settings.menu_Content_green = tconfig.getInt32( "menu_Content_green", 0x0f );
	g_settings.menu_Content_blue = tconfig.getInt32( "menu_Content_blue", 0x23 );

	g_settings.menu_Content_Text_alpha = tconfig.getInt32( "menu_Content_Text_alpha", 0x00 );
	g_settings.menu_Content_Text_red = tconfig.getInt32( "menu_Content_Text_red", 0x64 );
	g_settings.menu_Content_Text_green = tconfig.getInt32( "menu_Content_Text_green", 0x64 );
	g_settings.menu_Content_Text_blue = tconfig.getInt32( "menu_Content_Text_blue", 0x64 );

	g_settings.menu_Content_Selected_alpha = tconfig.getInt32( "menu_Content_Selected_alpha", 0x14 );
	g_settings.menu_Content_Selected_red = tconfig.getInt32( "menu_Content_Selected_red", 0x19 );
	g_settings.menu_Content_Selected_green = tconfig.getInt32( "menu_Content_Selected_green", 0x37 );
	g_settings.menu_Content_Selected_blue = tconfig.getInt32( "menu_Content_Selected_blue", 0x64 );

	g_settings.menu_Content_Selected_Text_alpha = tconfig.getInt32( "menu_Content_Selected_Text_alpha", 0x00 );
	g_settings.menu_Content_Selected_Text_red = tconfig.getInt32( "menu_Content_Selected_Text_red", 0x00 );
	g_settings.menu_Content_Selected_Text_green = tconfig.getInt32( "menu_Content_Selected_Text_green", 0x00 );
	g_settings.menu_Content_Selected_Text_blue = tconfig.getInt32( "menu_Content_Selected_Text_blue", 0x00 );

	g_settings.menu_Content_inactive_alpha = tconfig.getInt32( "menu_Content_inactive_alpha", 0x14 );
	g_settings.menu_Content_inactive_red = tconfig.getInt32( "menu_Content_inactive_red", 0x00 );
	g_settings.menu_Content_inactive_green = tconfig.getInt32( "menu_Content_inactive_green", 0x0f );
	g_settings.menu_Content_inactive_blue = tconfig.getInt32( "menu_Content_inactive_blue", 0x23 );

	g_settings.menu_Content_inactive_Text_alpha = tconfig.getInt32( "menu_Content_inactive_Text_alpha", 0x00 );
	g_settings.menu_Content_inactive_Text_red = tconfig.getInt32( "menu_Content_inactive_Text_red", 55 );
	g_settings.menu_Content_inactive_Text_green = tconfig.getInt32( "menu_Content_inactive_Text_green", 70 );
	g_settings.menu_Content_inactive_Text_blue = tconfig.getInt32( "menu_Content_inactive_Text_blue", 85 );

	g_settings.infobar_alpha = tconfig.getInt32( "infobar_alpha", 0x14 );
	g_settings.infobar_red = tconfig.getInt32( "infobar_red", 0x00 );
	g_settings.infobar_green = tconfig.getInt32( "infobar_green", 0x0e );
	g_settings.infobar_blue = tconfig.getInt32( "infobar_blue", 0x23 );

	g_settings.infobar_Text_alpha = tconfig.getInt32( "infobar_Text_alpha", 0x00 );
	g_settings.infobar_Text_red = tconfig.getInt32( "infobar_Text_red", 0x64 );
	g_settings.infobar_Text_green = tconfig.getInt32( "infobar_Text_green", 0x64 );
	g_settings.infobar_Text_blue = tconfig.getInt32( "infobar_Text_blue", 0x64 );
	for (int i = 0; i < LCD_SETTING_COUNT; i++)
		g_settings.lcd_setting[i] = tconfig.getInt32(lcd_setting[i].name, lcd_setting[i].default_value);
	strcpy(g_settings.lcd_setting_dim_time, tconfig.getString("lcd_dim_time","0").c_str());
	strcpy(g_settings.lcd_setting_dim_brightness, tconfig.getString("lcd_dim_brightness","0").c_str());

	strcpy( g_settings.font_file, configfile.getString( "font_file", "/share/fonts/neutrino.ttf" ).c_str() );
	colorSetupNotifier->changeNotify(NONEXISTANT_LOCALE, NULL);
	SetupFonts();
	CVFD::getInstance()->setlcdparameter();
}

void CNeutrinoApp::saveColors(const char * fname)
{
	//bool res;
	CConfigFile tconfig(',', true);

	//colors
	tconfig.setInt32( "menu_Head_alpha", g_settings.menu_Head_alpha );
	tconfig.setInt32( "menu_Head_red", g_settings.menu_Head_red );
	tconfig.setInt32( "menu_Head_green", g_settings.menu_Head_green );
	tconfig.setInt32( "menu_Head_blue", g_settings.menu_Head_blue );

	tconfig.setInt32( "menu_Head_Text_alpha", g_settings.menu_Head_Text_alpha );
	tconfig.setInt32( "menu_Head_Text_red", g_settings.menu_Head_Text_red );
	tconfig.setInt32( "menu_Head_Text_green", g_settings.menu_Head_Text_green );
	tconfig.setInt32( "menu_Head_Text_blue", g_settings.menu_Head_Text_blue );

	tconfig.setInt32( "menu_Content_alpha", g_settings.menu_Content_alpha );
	tconfig.setInt32( "menu_Content_red", g_settings.menu_Content_red );
	tconfig.setInt32( "menu_Content_green", g_settings.menu_Content_green );
	tconfig.setInt32( "menu_Content_blue", g_settings.menu_Content_blue );

	tconfig.setInt32( "menu_Content_Text_alpha", g_settings.menu_Content_Text_alpha );
	tconfig.setInt32( "menu_Content_Text_red", g_settings.menu_Content_Text_red );
	tconfig.setInt32( "menu_Content_Text_green", g_settings.menu_Content_Text_green );
	tconfig.setInt32( "menu_Content_Text_blue", g_settings.menu_Content_Text_blue );

	tconfig.setInt32( "menu_Content_Selected_alpha", g_settings.menu_Content_Selected_alpha );
	tconfig.setInt32( "menu_Content_Selected_red", g_settings.menu_Content_Selected_red );
	tconfig.setInt32( "menu_Content_Selected_green", g_settings.menu_Content_Selected_green );
	tconfig.setInt32( "menu_Content_Selected_blue", g_settings.menu_Content_Selected_blue );

	tconfig.setInt32( "menu_Content_Selected_Text_alpha", g_settings.menu_Content_Selected_Text_alpha );
	tconfig.setInt32( "menu_Content_Selected_Text_red", g_settings.menu_Content_Selected_Text_red );
	tconfig.setInt32( "menu_Content_Selected_Text_green", g_settings.menu_Content_Selected_Text_green );
	tconfig.setInt32( "menu_Content_Selected_Text_blue", g_settings.menu_Content_Selected_Text_blue );

	tconfig.setInt32( "menu_Content_inactive_alpha", g_settings.menu_Content_inactive_alpha );
	tconfig.setInt32( "menu_Content_inactive_red", g_settings.menu_Content_inactive_red );
	tconfig.setInt32( "menu_Content_inactive_green", g_settings.menu_Content_inactive_green );
	tconfig.setInt32( "menu_Content_inactive_blue", g_settings.menu_Content_inactive_blue );

	tconfig.setInt32( "menu_Content_inactive_Text_alpha", g_settings.menu_Content_inactive_Text_alpha );
	tconfig.setInt32( "menu_Content_inactive_Text_red", g_settings.menu_Content_inactive_Text_red );
	tconfig.setInt32( "menu_Content_inactive_Text_green", g_settings.menu_Content_inactive_Text_green );
	tconfig.setInt32( "menu_Content_inactive_Text_blue", g_settings.menu_Content_inactive_Text_blue );

	tconfig.setInt32( "infobar_alpha", g_settings.infobar_alpha );
	tconfig.setInt32( "infobar_red", g_settings.infobar_red );
	tconfig.setInt32( "infobar_green", g_settings.infobar_green );
	tconfig.setInt32( "infobar_blue", g_settings.infobar_blue );

	tconfig.setInt32( "infobar_Text_alpha", g_settings.infobar_Text_alpha );
	tconfig.setInt32( "infobar_Text_red", g_settings.infobar_Text_red );
	tconfig.setInt32( "infobar_Text_green", g_settings.infobar_Text_green );
	tconfig.setInt32( "infobar_Text_blue", g_settings.infobar_Text_blue );
	for (int i = 0; i < LCD_SETTING_COUNT; i++)
		tconfig.setInt32(lcd_setting[i].name, g_settings.lcd_setting[i]);
	tconfig.setString("lcd_dim_time", g_settings.lcd_setting_dim_time);
	tconfig.setString("lcd_dim_brightness", g_settings.lcd_setting_dim_brightness);

	tconfig.setString("font_file", g_settings.font_file);
	tconfig.saveConfig(fname);
}

void CNeutrinoApp::loadKeys(const char * fname)
{
	bool res;
	CConfigFile tconfig(',', true);

	res = tconfig.loadConfig(fname);
	if(!res) return;
	//rc-key configuration
	g_settings.key_tvradio_mode = tconfig.getInt32( "key_tvradio_mode", CRCInput::RC_nokey );

	g_settings.key_channelList_pageup = tconfig.getInt32( "key_channelList_pageup",  CRCInput::RC_minus );
	g_settings.key_channelList_pagedown = tconfig.getInt32( "key_channelList_pagedown", CRCInput::RC_plus );
	g_settings.key_channelList_cancel = tconfig.getInt32( "key_channelList_cancel",  CRCInput::RC_home );
	g_settings.key_channelList_sort = tconfig.getInt32( "key_channelList_sort",  CRCInput::RC_blue );
	g_settings.key_channelList_addrecord = tconfig.getInt32( "key_channelList_addrecord",  CRCInput::RC_nokey );
	g_settings.key_channelList_addremind = tconfig.getInt32( "key_channelList_addremind",  CRCInput::RC_nokey );

	g_settings.key_list_start = tconfig.getInt32( "key_list_start", CRCInput::RC_nokey );
	g_settings.key_list_end = tconfig.getInt32( "key_list_end", CRCInput::RC_nokey );
	g_settings.menu_left_exit = tconfig.getInt32( "menu_left_exit", 0 );

	g_settings.audio_run_player = tconfig.getInt32( "audio_run_player", 1 );
	g_settings.key_click = tconfig.getInt32( "key_click", 0 );
	g_settings.mpkey_rewind = tconfig.getInt32( "mpkey.rewind", CRCInput::RC_rewind );
	g_settings.mpkey_forward = tconfig.getInt32( "mpkey.forward", CRCInput::RC_forward );
	g_settings.mpkey_pause = tconfig.getInt32( "mpkey.pause", CRCInput::RC_pause );
	//g_settings.mpkey_stop = tconfig.getInt32( "mpkey.stop", CRCInput::RC_tv );
	g_settings.mpkey_stop = tconfig.getInt32( "mpkey.stop", CRCInput::RC_stop );
	g_settings.mpkey_play = tconfig.getInt32( "mpkey.play", CRCInput::RC_play );
	g_settings.mpkey_audio = tconfig.getInt32( "mpkey.audio", CRCInput::RC_audio );
	g_settings.mpkey_subt = tconfig.getInt32( "mpkey.subt", CRCInput::RC_video );
	g_settings.mpkey_time = tconfig.getInt32( "mpkey.time", CRCInput::RC_ok );
	g_settings.mpkey_bookmark = tconfig.getInt32( "mpkey.bookmark", CRCInput::RC_yellow );
	g_settings.mpkey_plugin = tconfig.getInt32( "mpkey.plugin", CRCInput::RC_blue );
	g_settings.key_timeshift = configfile.getInt32( "key_timeshift", CRCInput::RC_nokey );
	g_settings.key_plugin = configfile.getInt32( "key_plugin", CRCInput::RC_nokey );

	g_settings.key_quickzap_up = tconfig.getInt32( "key_quickzap_up",  CRCInput::RC_up );
	g_settings.key_quickzap_down = tconfig.getInt32( "key_quickzap_down",  CRCInput::RC_down );
	g_settings.key_subchannel_up = tconfig.getInt32( "key_subchannel_up",  CRCInput::RC_right );
	g_settings.key_subchannel_down = tconfig.getInt32( "key_subchannel_down",  CRCInput::RC_left );
	g_settings.key_zaphistory = tconfig.getInt32( "key_zaphistory",  CRCInput::RC_home );
	g_settings.key_lastchannel = tconfig.getInt32( "key_lastchannel",  CRCInput::RC_0 );

	g_settings.key_bouquet_up = tconfig.getInt32( "key_bouquet_up",  CRCInput::RC_right);
	g_settings.key_bouquet_down = tconfig.getInt32( "key_bouquet_down",  CRCInput::RC_left);
	strcpy(g_settings.repeat_blocker, tconfig.getString("repeat_blocker", "300").c_str());
	strcpy(g_settings.repeat_genericblocker, tconfig.getString("repeat_genericblocker", "100").c_str());
}

void CNeutrinoApp::saveKeys(const char * fname)
{
	CConfigFile tconfig(',', true);
	//rc-key configuration
	tconfig.setInt32( "key_tvradio_mode", g_settings.key_tvradio_mode );

	tconfig.setInt32( "key_channelList_pageup", g_settings.key_channelList_pageup );
	tconfig.setInt32( "key_channelList_pagedown", g_settings.key_channelList_pagedown );
	tconfig.setInt32( "key_channelList_cancel", g_settings.key_channelList_cancel );
	tconfig.setInt32( "key_channelList_sort", g_settings.key_channelList_sort );
	tconfig.setInt32( "key_channelList_addrecord", g_settings.key_channelList_addrecord );
	tconfig.setInt32( "key_channelList_addremind", g_settings.key_channelList_addremind );

	tconfig.setInt32( "key_quickzap_up", g_settings.key_quickzap_up );
	tconfig.setInt32( "key_quickzap_down", g_settings.key_quickzap_down );
	tconfig.setInt32( "key_bouquet_up", g_settings.key_bouquet_up );
	tconfig.setInt32( "key_bouquet_down", g_settings.key_bouquet_down );
	tconfig.setInt32( "key_subchannel_up", g_settings.key_subchannel_up );
	tconfig.setInt32( "key_subchannel_down", g_settings.key_subchannel_down );
	tconfig.setInt32( "key_zaphistory", g_settings.key_zaphistory );
	tconfig.setInt32( "key_lastchannel", g_settings.key_lastchannel );

	tconfig.setInt32( "key_list_start", g_settings.key_list_start );
	tconfig.setInt32( "key_list_end", g_settings.key_list_end );
	tconfig.setInt32( "menu_left_exit", g_settings.menu_left_exit );
	tconfig.setInt32( "audio_run_player", g_settings.audio_run_player );
	tconfig.setInt32( "key_click", g_settings.key_click );
	tconfig.setInt32( "mpkey.rewind", g_settings.mpkey_rewind );
	tconfig.setInt32( "mpkey.forward", g_settings.mpkey_forward );
	tconfig.setInt32( "mpkey.pause", g_settings.mpkey_pause );
	tconfig.setInt32( "mpkey.stop", g_settings.mpkey_stop );
	tconfig.setInt32( "mpkey.play", g_settings.mpkey_play );
	tconfig.setInt32( "mpkey.audio", g_settings.mpkey_audio );
	tconfig.setInt32( "mpkey.subt", g_settings.mpkey_subt );
	tconfig.setInt32( "mpkey.time", g_settings.mpkey_time );
	tconfig.setInt32( "mpkey.bookmark", g_settings.mpkey_bookmark );
	tconfig.setInt32( "mpkey.plugin", g_settings.mpkey_plugin );
	tconfig.setInt32( "key_timeshift", g_settings.key_timeshift );
	tconfig.setInt32( "key_plugin", g_settings.key_plugin );
	tconfig.setInt32( "key_unlock", g_settings.key_unlock );

	tconfig.setString( "repeat_blocker", g_settings.repeat_blocker );
	tconfig.setString( "repeat_genericblocker", g_settings.repeat_genericblocker );
	tconfig.saveConfig(fname);
}
