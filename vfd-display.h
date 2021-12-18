#include "printer.h"
#include "scroller.h"

#include "vfd_interface.h"
#include "display_def.h"

class VFDDisplay : public Printer {

public:

   VFDDisplay(const std::string& match_name, const std::string& def_file_path);

   bool Init();

   virtual void Print(int where, const std::string& what);
   virtual void noRendererPrint();
   virtual void rendererPrint( RendererState* current_state_ );
   virtual void goodBye();

private:
   bool initialized_;

   DisplayDef display;
   VfdInterface vfd;

   Scroller data_scroller;

   uint8_t groupForTime; // Group of digits to use for showing time
   uint8_t groupForHMS;  // Group of digits to use for showing playback time
   uint8_t groupForData; // Another group of digits for showing data

   uint8_t posTimeIni;   // Index of first digit for time
   uint8_t posHMSIni;   // Index of first digit for time

   void printPlayingTime();
   void printClock();
   void printTime( const uint8_t groupToPrint, int time, bool isHms );
   void clearPlayingTime();

   bool blink_flag;  // Changes on every tick

};

