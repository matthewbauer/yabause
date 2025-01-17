#ifndef _MSC_VER
#include <stdbool.h>
#endif
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _MSC_VER
#define snprintf _snprintf
#pragma pack(1)
#endif

#include <sys/stat.h>

#include "libretro.h"

#include "vdp1.h"
#include "vdp2.h"
//#include "scsp.h"
#include "peripheral.h"
#include "cdbase.h"
#include "yabause.h"
#include "yui.h"

//#include "m68kc68k.h"
#include "cs0.h"
#include "cs2.h"

#include "m68kcore.h"
#include "vidogl.h"
#include "vidsoft.h"

yabauseinit_struct yinit;

uint16_t *vid_buf = NULL;
int game_width;
int game_height;

static bool hle_bios_force = false;
static bool frameskip_enable = false;
static int addon_cart_type = CART_NONE;

struct retro_perf_callback perf_cb;
retro_get_cpu_features_t perf_get_cpu_features_cb = NULL;

retro_log_printf_t log_cb;
static retro_video_refresh_t video_cb;
static retro_input_poll_t input_poll_cb;
static retro_input_state_t input_state_cb;
static retro_environment_t environ_cb;
static retro_audio_sample_batch_t audio_batch_cb;

#define RETRO_DEVICE_MTAP_PAD RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 0)
#define RETRO_DEVICE_MTAP_3D  RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_ANALOG, 0)

void retro_set_environment(retro_environment_t cb)
{
   static const struct retro_variable vars[] = {
      { "yabause_frameskip", "Frameskip; disabled|enabled" },
      { "yabause_force_hle_bios", "Force HLE BIOS (restart); disabled|enabled" },
      { "yabause_addon_cart", "Addon Cartridge (restart); none|1M_ram|4M_ram" },
      { NULL, NULL },
   };

   static const struct retro_controller_description peripherals[] = {
       { "Saturn Pad", RETRO_DEVICE_JOYPAD },
       { "Saturn 3D Pad", RETRO_DEVICE_ANALOG },
       { "None", RETRO_DEVICE_NONE },
   };
   
   static const struct retro_controller_description mtaps[] = {
       { "Saturn Pad", RETRO_DEVICE_JOYPAD },
       { "Saturn 3D Pad", RETRO_DEVICE_ANALOG },
       { "Multitap + Pad", RETRO_DEVICE_MTAP_PAD },
       { "Multitap + 3D Pad", RETRO_DEVICE_MTAP_3D },
       { "None", RETRO_DEVICE_NONE },
   };
   
   static const struct retro_controller_info ports[] = {
      { mtaps, 5 },
      { mtaps, 5 },
      { peripherals, 3 },
      { peripherals, 3 },
      { peripherals, 3 },
      { peripherals, 3 },
      { peripherals, 3 },
      { peripherals, 3 },
      { 0 },
   };
   
   environ_cb = cb;

   cb(RETRO_ENVIRONMENT_SET_VARIABLES, (void*)vars);
   environ_cb(RETRO_ENVIRONMENT_SET_CONTROLLER_INFO, (void*)ports);
}
void retro_set_video_refresh(retro_video_refresh_t cb) { video_cb = cb; }
void retro_set_audio_sample(retro_audio_sample_t cb) { (void)cb; }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { audio_batch_cb = cb; }
void retro_set_input_poll(retro_input_poll_t cb) { input_poll_cb = cb; }
void retro_set_input_state(retro_input_state_t cb) { input_state_cb = cb; }

// PERLIBRETRO
#define PERCORE_LIBRETRO 2

static int pad_type[8] = {1,1,1,1,1,1,1,1};
static unsigned players = 2;
static bool multitap[2] = {0};

int PERLIBRETROInit(void)
{
    PortData_struct* portdata = 0;
    u32 i = 0;
    u8 j = 0;
    void *controller;
    
    if(!multitap[0] && !multitap[1])
        players = 2;
    else if(multitap[0] && multitap[1])
        players = 8;
    else
        players = 6;
    
    PerPortReset();
    
    for(i = 0; i < players; i++) {
        //Ports can handle 6 peripherals, fill port 1 first.
        if(players > 2 && i < 6 || i == 0)
            portdata = &PORTDATA1;
        else
            portdata = &PORTDATA2;
        switch(pad_type[i]){
            case RETRO_DEVICE_NONE:
                controller = NULL;
                break;
            case RETRO_DEVICE_ANALOG:
                controller = (void*)Per3DPadAdd(portdata);
                for(j = PERPAD_UP; j <= PERPAD_Z; j++) {
                    PerSetKey((i << 8) + j, j, controller);
                }
                for(j = PERANALOG_AXIS1; j <= PERANALOG_AXIS7; j++) {
                    PerSetKey((i << 8) + j, j, controller);
                }
                break;
            case RETRO_DEVICE_JOYPAD:
            default:
                controller = (void*)PerPadAdd(portdata);
                for(j = PERPAD_UP; j <= PERPAD_Z; j++) {
                    PerSetKey((i << 8) + j, j, controller);
                }
                break;
        }
    }
    return 0;
}

static int PERLIBRETROHandleEvents(void)
{
   int i = 0;
   int analog_left_x = 0;
   int analog_left_y = 0;
   
   for(i = 0; i < players; i++) {
       
      analog_left_x = 0;
      analog_left_y = 0;
      
      switch(pad_type[i]){
         case RETRO_DEVICE_ANALOG:
         analog_left_x = input_state_cb(i, RETRO_DEVICE_ANALOG, 
            RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X);
         
         PerAxisValue((i << 8) + PERANALOG_AXIS1, (u8)((analog_left_x + 0x8000) >> 8));

         analog_left_y = input_state_cb(i, RETRO_DEVICE_ANALOG,
            RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y);

         PerAxisValue((i << 8) + PERANALOG_AXIS2, (u8)((analog_left_y + 0x8000) >> 8));
         case RETRO_DEVICE_JOYPAD:
         if (input_state_cb(i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP))
            PerKeyDown((i << 8) + PERPAD_UP);
         else
            PerKeyUp((i << 8) + PERPAD_UP);
         if (input_state_cb(i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN))
            PerKeyDown((i << 8) + PERPAD_DOWN);
         else
            PerKeyUp((i << 8) + PERPAD_DOWN);
         if (input_state_cb(i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT))
            PerKeyDown((i << 8) + PERPAD_LEFT);
         else
            PerKeyUp((i << 8) + PERPAD_LEFT);
         if (input_state_cb(i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT))
            PerKeyDown((i << 8) + PERPAD_RIGHT);
         else
            PerKeyUp((i << 8) + PERPAD_RIGHT);
         if (input_state_cb(i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y))
            PerKeyDown((i << 8) + PERPAD_A);
         else
            PerKeyUp((i << 8) + PERPAD_A);
         if (input_state_cb(i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B))
            PerKeyDown((i << 8) + PERPAD_B);
         else
            PerKeyUp((i << 8) + PERPAD_B);
         if (input_state_cb(i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A))
            PerKeyDown((i << 8) + PERPAD_C);
         else
            PerKeyUp((i << 8) + PERPAD_C);
         if (input_state_cb(i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X))
            PerKeyDown((i << 8) + PERPAD_X);
         else
            PerKeyUp((i << 8) + PERPAD_X);
         if (input_state_cb(i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L))
            PerKeyDown((i << 8) + PERPAD_Y);
         else
            PerKeyUp((i << 8) + PERPAD_Y);
         if (input_state_cb(i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R))
            PerKeyDown((i << 8) + PERPAD_Z);
         else
            PerKeyUp((i << 8) + PERPAD_Z);
         if (input_state_cb(i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START))
            PerKeyDown((i << 8) + PERPAD_START);
         else
            PerKeyUp((i << 8) + PERPAD_START);
         if (input_state_cb(i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2))
            PerKeyDown((i << 8) + PERPAD_LEFT_TRIGGER);
         else
            PerKeyUp((i << 8) + PERPAD_LEFT_TRIGGER);
         if (input_state_cb(i, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2))
            PerKeyDown((i << 8) + PERPAD_RIGHT_TRIGGER);
         else
            PerKeyUp((i << 8) + PERPAD_RIGHT_TRIGGER);
         break;

         default:
         break;
      }
   }
   
   if ( YabauseExec() != 0 )
      return -1;
   return 0;
}

void PERLIBRETRODeInit(void) {
   /* Nothing */
}

void PERLIBRETRONothing(void) {
   /* Nothing */
}

u32 PERLIBRETROScan(u32 flags) {
   /* Nothing */
   return 0;
}

void PERLIBRETROKeyName(u32 key, char *name, int size) {
   /* Nothing */
}

PerInterface_struct PERLIBRETROJoy = {
    PERCORE_LIBRETRO,
    "Libretro Input Interface",
    PERLIBRETROInit,
    PERLIBRETRODeInit,
    PERLIBRETROHandleEvents,
    PERLIBRETROScan,
    0,
    PERLIBRETRONothing,
    PERLIBRETROKeyName
};

// SNDLIBRETRO
#define SNDCORE_LIBRETRO   11
#define SAMPLERATE 44100
#define SAMPLEFRAME 735
#define BUFFER_LEN 65536

static uint32_t video_freq;
static uint32_t audio_size;

static int SNDLIBRETROInit(void);
static void SNDLIBRETRODeInit(void);
static int SNDLIBRETROReset(void);
static int SNDLIBRETROChangeVideoFormat(int vertfreq);
static void SNDLIBRETROUpdateAudio(u32 *leftchanbuffer, u32 *rightchanbuffer, u32 num_samples);
static u32 SNDLIBRETROGetAudioSpace(void);
static void SNDLIBRETROMuteAudio(void);
static void SNDLIBRETROUnMuteAudio(void);
static void SNDLIBRETROSetVolume(int volume);

static int SNDLIBRETROInit(void)
{
	//SNDLIBRETROChangeVideoFormat(60);
    return 0;
}

static void SNDLIBRETRODeInit(void)
{
}

static int SNDLIBRETROReset(void)
{
    return 0;
}

static int SNDLIBRETROChangeVideoFormat(int vertfreq)
{
	//video_freq = vertfreq;
	//audio_size = 44100 / vertfreq;
    return 0;
}

static void sdlConvert32uto16s(s32 *srcL, s32 *srcR, s16 *dst, u32 len) {
   u32 i;

   for (i = 0; i < len; i++)
   {
      // Left Channel
      if (*srcL > 0x7FFF)
         *dst = 0x7FFF;
      else if (*srcL < -0x8000)
         *dst = -0x8000;
      else
         *dst = *srcL;
      srcL++;
      dst++;

      // Right Channel
      if (*srcR > 0x7FFF)
         *dst = 0x7FFF;
      else if (*srcR < -0x8000)
         *dst = -0x8000;
      else
         *dst = *srcR;
      srcR++;
      dst++;
   } 
}

static void SNDLIBRETROUpdateAudio(u32 *leftchanbuffer, u32 *rightchanbuffer, u32 num_samples)
{
   s16 sound_buf[4096];
   sdlConvert32uto16s((s32*)leftchanbuffer, (s32*)rightchanbuffer, sound_buf, num_samples);
   audio_batch_cb(sound_buf, num_samples);

   audio_size -= num_samples;
}

static u32 SNDLIBRETROGetAudioSpace(void)
{
	return audio_size;
}

void SNDLIBRETROMuteAudio()
{
}

void SNDLIBRETROUnMuteAudio()
{
}

void SNDLIBRETROSetVolume(int volume)
{
}

SoundInterface_struct SNDLIBRETRO = {
    SNDCORE_LIBRETRO,
    "Libretro Sound Interface",
    SNDLIBRETROInit,
    SNDLIBRETRODeInit,
    SNDLIBRETROReset,
    SNDLIBRETROChangeVideoFormat,
    SNDLIBRETROUpdateAudio,
    SNDLIBRETROGetAudioSpace,
    SNDLIBRETROMuteAudio,
    SNDLIBRETROUnMuteAudio,
    SNDLIBRETROSetVolume
};

M68K_struct *M68KCoreList[] = {
    &M68KDummy,
    &M68KC68K,
    NULL
};

SH2Interface_struct *SH2CoreList[] = {
    &SH2Interpreter,
    &SH2DebugInterpreter,
    NULL
};

PerInterface_struct *PERCoreList[] = {
    &PERDummy,
    &PERLIBRETROJoy,
    NULL
};

CDInterface *CDCoreList[] = {
    &DummyCD,
    &ISOCD,
    NULL
};

SoundInterface_struct *SNDCoreList[] = {
    &SNDDummy,
    &SNDLIBRETRO,
    NULL
};

VideoInterface_struct *VIDCoreList[] = {
    //&VIDDummy,
    &VIDSoft,
    NULL
};

#pragma mark Yabause Callbacks

void YuiErrorMsg(const char *string)
{
   if (log_cb)
      log_cb(RETRO_LOG_ERROR, "Yabause: %s\n", string);
}

void YuiSetVideoAttribute(int type, int val)
{
   if (log_cb)
      log_cb(RETRO_LOG_INFO, "Yabause called back to YuSetVideoAttribute.\n");
}

int YuiSetVideoMode(int width, int height, int bpp, int fullscreen)
{
   if (log_cb)
      log_cb(RETRO_LOG_INFO, "Yabause called, it wants to set width of %d and height of %d.\n", width, height);
    return 0;
}

void updateCurrentResolution(void)
{
    int current_width = 320;
    int current_height = 240;

    // Test if VIDCore valid AND NOT the Dummy Interface (or at least VIDCore->id != 0). 
    // Avoid calling GetGlSize if Dummy/id=0 is selected
    if (VIDCore && VIDCore->id) 
       VIDCore->GetGlSize(&current_width, &current_height);

    game_width = current_width;
    game_height = current_height;
}

void YuiSwapBuffers(void) 
{
    updateCurrentResolution();
}

/************************************
 * libretro implementation
 ************************************/

static struct retro_system_av_info g_av_info;

void retro_get_system_info(struct retro_system_info *info)
{
    memset(info, 0, sizeof(*info));
	info->library_name = "Yabause";
	info->library_version = "v0.9.14";
	info->need_fullpath = true;
	info->valid_extensions = "bin|cue|iso";
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
    memset(info, 0, sizeof(*info));
    // Just assume NTSC for now. TODO: Verify FPS.
    info->timing.fps            = 60;
    info->timing.sample_rate    = 44100;
    info->geometry.base_width   = game_width;
    info->geometry.base_height  = game_height;
    info->geometry.max_width    = 704;
    info->geometry.max_height   = 512;
    info->geometry.aspect_ratio = 4.0 / 3.0;
}

void retro_set_controller_port_device(unsigned port, unsigned device)
{   
   switch(device){
       case RETRO_DEVICE_JOYPAD:
       case RETRO_DEVICE_ANALOG:
           pad_type[port] = device;
           if(port < 2)
               multitap[port] = false;
           break;
       //assumes only ports 1 and 2 can report as multitap
       case RETRO_DEVICE_MTAP_PAD:
           pad_type[port] = RETRO_DEVICE_JOYPAD;
           if(port < 2)
              multitap[port] = true;
           break;
       case RETRO_DEVICE_MTAP_3D:
           pad_type[port] = RETRO_DEVICE_ANALOG;
           if(port < 2)
              multitap[port] = true;
           break;
   }
   
   if(PERCore) PERCore->Init();
}

size_t retro_serialize_size(void) 
{ 
	//return STATE_SIZE;
	return 0;
}

bool retro_serialize(void *data, size_t size)
{ 
   //if (size != STATE_SIZE)
   //   return FALSE;

	ScspMuteAudio(SCSP_MUTE_SYSTEM);
	int error = YabSaveState((const char*)data);
	ScspUnMuteAudio(SCSP_MUTE_SYSTEM);
   return !error;
}

bool retro_unserialize(const void *data, size_t size)
{
   //if (size != STATE_SIZE)
   //   return FALSE;

	ScspMuteAudio(SCSP_MUTE_SYSTEM);
	int error = YabLoadState((const char*)data);
	ScspUnMuteAudio(SCSP_MUTE_SYSTEM);
   return !error;
}

void retro_cheat_reset(void)
{
   CheatClearCodes();
}

void retro_cheat_set(unsigned index, bool enabled, const char *code)
{
   (void)index;
   (void)enabled;
   (void)code;

   if (CheatAddARCode(code) == 0)
      return;
}

static char full_path[256];
static char bios_path[256];

static void check_variables(void)
{
   struct retro_variable var;
   var.key = "yabause_frameskip";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "disabled") == 0 && frameskip_enable)
      {
         DisableAutoFrameSkip();
         frameskip_enable = false;
      }
      else if (strcmp(var.value, "enabled") == 0 && !frameskip_enable)
      {
         EnableAutoFrameSkip();
         frameskip_enable = true;
      }
   }

   var.key = "yabause_force_hle_bios";
   var.value = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "disabled") == 0 && hle_bios_force)
         hle_bios_force = false;
      else if (strcmp(var.value, "enabled") == 0 && !hle_bios_force)
         hle_bios_force = true;
   }
   
   var.key = "yabause_addon_cart";
   var.value = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (strcmp(var.value, "none") == 0 && addon_cart_type != CART_NONE)
         addon_cart_type = CART_NONE;
      else if (strcmp(var.value, "1M_ram") == 0 && addon_cart_type != CART_DRAM8MBIT)
         addon_cart_type = CART_DRAM8MBIT;
      else if (strcmp(var.value, "4M_ram") == 0 && addon_cart_type != CART_DRAM32MBIT)
         addon_cart_type = CART_DRAM32MBIT;
   } 
}

static int does_file_exist(const char *filename)
{
   struct stat st;
   int result = stat(filename, &st);
   return result == 0;
}

void retro_init(void)
{
   struct retro_log_callback log;

   if (environ_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log))
      log_cb = log.log;
   else
      log_cb = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_PERF_INTERFACE, &perf_cb))
      perf_get_cpu_features_cb = perf_cb.get_cpu_features;
   else
      perf_get_cpu_features_cb = NULL;

	game_width = 320;
	game_height = 240;
	
   const char *dir = NULL;
   environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &dir);

   if (dir)
   {
#ifdef _WIN32
      char slash = '\\';
#else
      char slash = '/';
#endif
      snprintf(bios_path, sizeof(bios_path), "%s%c%s", dir, slash, "saturn_bios.bin");
   }
   
	vid_buf = (u16 *)calloc(sizeof(u16), 704 * 512);
    
    if(PERCore) PERCore->Init();
	
    // Performance level for interpreter CPU core is 16
    unsigned level = 16;
    environ_cb(RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL, &level);
}
bool retro_load_game(const struct retro_game_info *info)
{
   check_variables();

   snprintf(full_path, sizeof(full_path), "%s", info->path);

   struct retro_input_descriptor desc[] = {
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "D-Pad Left" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,    "D-Pad Up" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,  "D-Pad Down" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "D-Pad Right" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,     "B" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,     "C" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X,     "X" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y,     "A" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L,     "Y" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R,     "Z" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2,    "L" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2,    "R" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Start" },
      { 0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X, "Analog X" },
      { 0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y, "Analog Y" },

      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "D-Pad Left" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,    "D-Pad Up" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,  "D-Pad Down" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "D-Pad Right" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,     "B" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,     "C" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X,     "X" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y,     "A" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L,     "Y" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R,     "Z" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2,    "L" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2,    "R" },
      { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Start" },
      { 1, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X, "Analog X" },
      { 1, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y, "Analog Y" },

      { 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "D-Pad Left" },
      { 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,    "D-Pad Up" },
      { 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,  "D-Pad Down" },
      { 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "D-Pad Right" },
      { 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,     "B" },
      { 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,     "C" },
      { 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X,     "X" },
      { 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y,     "A" },
      { 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L,     "Y" },
      { 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R,     "Z" },
      { 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2,    "L" },
      { 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2,    "R" },
      { 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Start" },
      { 2, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X, "Analog X" },
      { 2, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y, "Analog Y" },

      { 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "D-Pad Left" },
      { 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,    "D-Pad Up" },
      { 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,  "D-Pad Down" },
      { 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "D-Pad Right" },
      { 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,     "B" },
      { 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,     "C" },
      { 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X,     "X" },
      { 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y,     "A" },
      { 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L,     "Y" },
      { 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R,     "Z" },
      { 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2,    "L" },
      { 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2,    "R" },
      { 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Start" },
      { 3, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X, "Analog X" },
      { 3, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y, "Analog Y" },

      { 4, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "D-Pad Left" },
      { 4, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,    "D-Pad Up" },
      { 4, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,  "D-Pad Down" },
      { 4, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "D-Pad Right" },
      { 4, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,     "B" },
      { 4, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,     "C" },
      { 4, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X,     "X" },
      { 4, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y,     "A" },
      { 4, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L,     "Y" },
      { 4, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R,     "Z" },
      { 4, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2,    "L" },
      { 4, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2,    "R" },
      { 4, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Start" },
      { 4, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X, "Analog X" },
      { 4, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y, "Analog Y" },

      { 5, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "D-Pad Left" },
      { 5, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,    "D-Pad Up" },
      { 5, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,  "D-Pad Down" },
      { 5, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "D-Pad Right" },
      { 5, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,     "B" },
      { 5, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,     "C" },
      { 5, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X,     "X" },
      { 5, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y,     "A" },
      { 5, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L,     "Y" },
      { 5, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R,     "Z" },
      { 5, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2,    "L" },
      { 5, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2,    "R" },
      { 5, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Start" },
      { 5, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X, "Analog X" },
      { 5, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y, "Analog Y" },

      { 6, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "D-Pad Left" },
      { 6, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,    "D-Pad Up" },
      { 6, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,  "D-Pad Down" },
      { 6, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "D-Pad Right" },
      { 6, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,     "B" },
      { 6, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,     "C" },
      { 6, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X,     "X" },
      { 6, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y,     "A" },
      { 6, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L,     "Y" },
      { 6, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R,     "Z" },
      { 6, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2,    "L" },
      { 6, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2,    "R" },
      { 6, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Start" },
      { 6, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X, "Analog X" },
      { 6, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y, "Analog Y" },

      { 7, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "D-Pad Left" },
      { 7, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,    "D-Pad Up" },
      { 7, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,  "D-Pad Down" },
      { 7, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "D-Pad Right" },
      { 7, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,     "B" },
      { 7, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,     "C" },
      { 7, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X,     "X" },
      { 7, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y,     "A" },
      { 7, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L,     "Y" },
      { 7, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R,     "Z" },
      { 7, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2,    "L" },
      { 7, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2,    "R" },
      { 7, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Start" },
      { 7, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X, "Analog X" },
      { 7, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y, "Analog Y" },

      { 0 },
   };

   environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, desc);
   
   yinit.cdcoretype = CDCORE_ISO;

   yinit.cdpath = full_path;
   // Emulate BIOS
   yinit.biospath = (bios_path[0] != '\0' && does_file_exist(bios_path) && !hle_bios_force) ? bios_path : NULL;

   yinit.percoretype = PERCORE_LIBRETRO;
#ifdef SH2_DYNAREC
   yinit.sh2coretype = 2;
#else
   yinit.sh2coretype = SH2CORE_INTERPRETER;
#endif

   yinit.vidcoretype = VIDCORE_SOFT;

   yinit.sndcoretype = SNDCORE_LIBRETRO;
   yinit.m68kcoretype = M68KCORE_C68K;
   yinit.carttype = addon_cart_type;
   yinit.regionid = REGION_AUTODETECT;
   yinit.buppath = NULL;
   yinit.mpegpath = NULL;

   yinit.videoformattype = VIDEOFORMATTYPE_NTSC;

   yinit.frameskip = frameskip_enable;
   yinit.clocksync = 0;
   yinit.basetime = 0;
#ifdef HAVE_THREADS
   yinit.usethreads = 1;
#else
   yinit.usethreads = 0;
#endif

   int result = YabauseInit(&yinit);
   YabauseSetDecilineMode(1);

   return !result;
}

bool retro_load_game_special(unsigned game_type, const struct retro_game_info *info, size_t num_info)
{
   (void)game_type;
   (void)info;
   (void)num_info;
   return false;
}

void retro_unload_game(void) 
{
	YabauseDeInit();
}

unsigned retro_get_region(void)
{  
   return Cs2GetRegionID() > 6 ? RETRO_REGION_PAL : RETRO_REGION_NTSC;
}

unsigned retro_api_version(void)
{
    return RETRO_API_VERSION;
}

void *retro_get_memory_data(unsigned id)
{
   uint8_t *data;

   switch (id)
   {
      case RETRO_MEMORY_SAVE_RAM:
         data = BupRam;
         break;
      default:
         data = NULL;
         break;
   }
   return data;
}

size_t retro_get_memory_size(unsigned id)
{
   unsigned size;

   switch (id)
   {
      case RETRO_MEMORY_SAVE_RAM:
         size = 0x10000;
         break;
      default:
         size = 0;
         break;
   }

   return size;
}

void retro_deinit(void)
{
   if (vid_buf)
      free(vid_buf);
}

void retro_reset(void)
{
	YabauseResetButton();
   YabauseInit(&yinit);
   YabauseSetDecilineMode(1);
}

void retro_run(void) 
{
   bool updated = false;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
      check_variables();
	
   audio_size = SAMPLEFRAME;
   
   input_poll_cb();
   
   //YabauseExec(); runs from handle events
   if(PERCore) PERCore->HandleEvents();

   for (unsigned i = 0; i < game_height * game_width; i++)
   {
      uint32_t source = dispbuffer[i];

      uint16_t r = ((source & 0xF80000) >> 19);
      uint16_t g = ((source & 0x00F800) >> 6);
      uint16_t b = ((source & 0x0000F8) << 7);
      vid_buf[i] = r | g | b;
   }
	
	video_cb(vid_buf, game_width, game_height, game_width * 2);
}

#ifdef ANDROID
#include <wchar.h>

size_t mbstowcs(wchar_t *pwcs, const char *s, size_t n)
{
   if (pwcs == NULL)
      return strlen(s);
   return mbsrtowcs(pwcs, &s, n, NULL);
}

size_t wcstombs(char *s, const wchar_t *pwcs, size_t n)
{
   return wcsrtombs(s, &pwcs, n, NULL);
}

#endif
