#include <cassert>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <ctime>
#include <chrono>

#include "vfd-display.h"
#include "pt6312_commands.h"

VFDDisplay::VFDDisplay(const std::string& match_name, const std::string& def_file_path) : Printer(match_name),
          display(DisplayDef(def_file_path)),
	  vfd(&display), data_scroller("  "),
	  groupForTime(0), groupForData(0xFF) {}

static inline bool findGroupForData( uint8_t& groupForData,
         const uint8_t gt1, const uint8_t gt2, const uint8_t numGroups ) {
   bool foundData = false;
   groupForData = 0;
   while(numGroups > groupForData) {
      if( gt1 != groupForData && gt2 != groupForData ) {
         foundData = true;
         break;
      }
      groupForData++;
   }
   return foundData;
}

bool VFDDisplay::Init() {

   // Does nothing if already initialized
   initialized_ = vfd.Init();

   if (initialized_) {
      // Search a group of digits with dots to show time (format 88:88)
      // Search other group of digits to show data
      posTimeIni = -1;
      posHMSIni = -1;

      bool foundTime = false;  // Found a group of digits that can represent 88:88
      bool foundHMS = false;  // Found a group of digits that can represent 8:88:88
      bool foundData = false;  // Found another group of digits


      for (uint8_t g=0; g<display.getNumberOfGroups(); g++) {
         if ( !foundTime && display.canShowTime(g, posTimeIni)) {
            groupForTime = g;
            foundTime = true;
         }
         if ( !foundHMS && display.canShowHMS(g, posHMSIni)) {
            groupForHMS = g;
	    foundHMS = true;
	 }
      }

      groupForData = 0;
      foundData = findGroupForData( groupForData, groupForTime, groupForHMS,
		                                  display.getNumberOfGroups());
      if (!foundData)
         groupForData = 0xFF;
   }

   return initialized_;
}

void VFDDisplay::Print(int where, const std::string& what) {
   assert(initialized_);  // call Init() first.

   display.resetGroup( groupForData );
   display.setDigits( groupForData, what, 0 );
}

static std::vector<KeyId> keysBuffer;  // Buffer containing current pressed keys

void VFDDisplay::noRendererPrint() {

   assert(initialized_);  // call Init() first.

   display.clearRoundSector();
   display.clearDigits();

   // If a group valid to show time is found, show time while not connected
   if (groupForTime != 0xFF)
      printClock();
}

void VFDDisplay::printClock() {

   static char timebuffer[32];

   // Print clock while not playing
   auto current_time = std::chrono::system_clock::now();
   std::time_t tt = std::chrono::system_clock::to_time_t(current_time);
   std::tm* tm = std::localtime(&tt);
   std::strftime(timebuffer, sizeof(timebuffer) - 1, "%R", tm);

   // Show 7:54 instead of 07:54
   if( *timebuffer == '0' ) *timebuffer = ' ';

   display.setDigits( groupForTime, timebuffer, posTimeIni );
   //display.setDots( groupForTime, posTimeIni + 1 );

   vfd.updateDisplay();
}

void VFDDisplay::printPlayingTime() {

   uint8_t groupToPrint = groupForHMS;
   bool isHms = true;

   if (groupToPrint == 0xFF ) {
      isHms = false;
      groupToPrint = groupForTime; // If we cannot use HMS, try HM
   }
   if (groupToPrint != 0xFF )
      printTime( groupToPrint, time, isHms );
}

void VFDDisplay::printTime( const uint8_t groupToPrint, int time, bool isHms ) {

   const int hour = time / 3600;
   const int minute = (time % 3600) / 60;
   const int second = (time % 60);

   static char cstr[16] = {0};
   int posIni = 0;

   if( isHms ) {
      if( hour == 0 ) {
         sprintf( cstr, "%2.d:%02d", minute, second );
         posIni = posHMSIni;
      } else
         sprintf( cstr, "%d:%02d:%02d", hour, minute, second );
   }
   else {
      sprintf( cstr, "%2.d:%02d", hour, minute );
      posIni = posTimeIni;
   }

   std::string hms (cstr);
   display.setDigits( groupToPrint, hms, posIni );
}

void VFDDisplay::clearPlayingTime() {
   uint8_t groupToClear = groupForHMS;
   if (groupToClear == 0xFF ) {
      groupToClear = groupForTime; // If we cannot use HMS, try HM
   }
   if (groupToClear != 0xFF ) {
      display.resetGroup( groupToClear );
      display.removeDots( groupToClear );
   }
}

void VFDDisplay::rendererPrint( RendererState* current_state_ ) {

   assert(initialized_);  // call Init() first.

   bool playing = false;
   bool pause = false;

   if (play_state == "PAUSED_PLAYBACK") {

      display.setSymbol(SymbolId::SYM_Pause);
      display.resetSymbol(SymbolId::SYM_Play);

      playing = true;
      pause = true;

      // Blink time on pause
      if( blink_flag )
         printPlayingTime();
      else
         clearPlayingTime();
   }
   else if (play_state == "PLAYING") {

      // Lit play symbol
      display.resetSymbol(SymbolId::SYM_Pause);
      display.setSymbol(SymbolId::SYM_Play);

      display.resetGroup( groupForData );
      clearPlayingTime();

      bool flag = blink_flag;

      // Print either odd or even round sectors
      for (unsigned int idx=0; idx<display.getRoundSectorLevelCount(); idx++) {
         if (flag)
            display.setRoundSectorLevel( idx );
         else
            display.resetRoundSectorLevel( idx );
         flag = !flag;
      }

      // Print current playing time
      printPlayingTime();

      playing = true;
   }
   else if (play_state == "STOPPED") {
      display.resetSymbol(SymbolId::SYM_Pause);
      display.resetSymbol(SymbolId::SYM_Play);
      display.setDigits( groupForData, "STOP", 0 );
      display.resetGroup( groupForTime );
      display.removeDots( groupForTime, posTimeIni + 1 );
      display.clearRoundSector();

      printClock();
   }

   uint32_t raw_keys = vfd.readKeys();
   display.getKeys( keysBuffer, raw_keys );

   for (KeyId k:keysBuffer) {
         //std::string ks;
	 if (k == KeyId::KEY_PLAY)
            current_state_->Play(); //TEST
	 else if (k == KeyId::KEY_PAUSE)
            current_state_->Pause(); //TEST
	 else if (k == KeyId::KEY_STOP)
            current_state_->Stop(); //TEST
/*	 else if (k == KeyId::KEY_REW) ks = "REW";
	 else if (k == KeyId::KEY_FWD) ks = "FWD";
	 else if (k == KeyId::KEY_PREV) ks = "PREV";
	 else if (k == KeyId::KEY_NEXT) ks = "NEXT";
	 else if (k == KeyId::KEY_EJECT) ks = "EJECT"; */
   }

   // Print info if playing
   if (playing) {

      static std::string pausetxt("PAUSE");
      static std::string playtxt("PLAY");
      std::string& ref = pause ? pausetxt : playtxt;
      data_scroller.SetValue( ref, display.getNumberOfDigitsOnGroup( groupForData ) );
      Print( 0, data_scroller.GetScrolledContent() );
 
      data_scroller.NextTick();
   }

   vfd.updateDisplay();

   blink_flag = !blink_flag;
}

void VFDDisplay::goodBye() {
   display.clearData();
   vfd.updateDisplay();
}

