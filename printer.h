// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
//  This file is part of UPnP LCD Display
//
//  Copyright (C) 2013 Henner Zeller <h.zeller@acm.org>
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
#ifndef UPNP_DISPLAY_PRINTER_
#define UPNP_DISPLAY_PRINTER_

#include <string>

#include "renderer-state.h"
#include "scroller.h"
#include "utf8.h"

// Interface for a simple display.
class Printer {
public:

   Printer(const std::string& match_name) :
	 player_match_name_(match_name),
	 play_state("STOPPED"), track_time(0), muted(false), blink_time(0) {};
   virtual ~Printer() {}

   virtual int width() const { return 16; }

   // Print line. The text is given in UTF-8, the printer has to attempt
   // to try its best to display it.
   virtual void Print(int line, const std::string &text) = 0;
   virtual void noRendererPrint();
   virtual void rendererPrint( RendererState* current_state_ );
   virtual void goodBye();
   virtual void SaveScreen() {}

   void fillVars(RendererState* state);

protected:
   const std::string& player_match_name_;
   std::string player_name;
   std::string title, composer, artist, album;
   std::string play_state;
   std::string volume, previous_volume;
   int track_time;
   int time;
   bool muted;
   Scroller first_line_scroller {"  -  "};
   Scroller second_line_scroller {"  -  "};
   int volume_countdown;
   uint8_t blink_time;

private:
   int parseTime(const std::string &upnp_time);
   std::string formatTime(int time);
   void CenterAlign(std::string *to_print, int width);
   void RightAlign(std::string *to_print, int width);
};

// Very simple implementation of the above, mostly for debugging. Just prints
// stuff continuously.
class ConsolePrinter : public Printer {
public:
   explicit ConsolePrinter(const std::string& match_name, int width) : Printer(match_name), width_(width) {}
   virtual int width() const { return width_; }
   virtual void Print(int line, const std::string &text);

private:
   const int width_;
};

#endif  // UPNP_DISPLAY_PRINTER_
