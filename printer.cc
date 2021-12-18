//  -*- c++ -*-
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

#include "printer.h"

#include <stdio.h>

#define STOP_SYMBOL "\u2b1b"   // ⬛
#define PLAY_SYMBOL "\u25b6"   // ▶
#define PAUSE_SYMBOL "]["      // TODO: add symbol in private unicode range.

// Number of periods, a changed volume flashes up.
static const int kVolumeFlashTime = 3;

void ConsolePrinter::Print(int line, const std::string &text) {
  printf("[%d]%s\n", line, text.c_str());
}

void Printer::fillVars(RendererState* current_state_) {

//PGAD TEST
//TODO call every 2 or 3 seconds to adjust and update timer every second 
current_state_->GetPositionInfo();
//\PGAD TEST
   player_name = current_state_->friendly_name();
   title = current_state_->GetVar("Meta_Title");
   composer = current_state_->GetVar("Meta_Composer");
   artist = current_state_->GetVar("Meta_Artist");
   std::string creator = current_state_->GetVar("Meta_Creator");

   if (artist == composer && !creator.empty() && creator != artist) {
     artist = creator;
   }

   album = current_state_->GetVar("Meta_Album");
   play_state = current_state_->GetVar("TransportState");
   time = parseTime(current_state_->GetVar("RelTime"));
   track_time = parseTime(current_state_->GetVar("CurrentTrackDuration"));
   volume = current_state_->GetVar("Volume");
   muted = current_state_->GetVar("Mute") == "1";
}

void Printer::noRendererPrint() {
   this->Print(0, "Waiting for");
   std::string to_print = (player_match_name_.empty()
                           ? "any Renderer"
                           : player_match_name_);
   CenterAlign(&to_print, this->width());
   this->Print(1, to_print);
}

void Printer::rendererPrint( RendererState* current_state_ ) {

    // First line is "[composer: ]Title"
    std::string print_line = composer;
    if (!print_line.empty()) print_line.append(": ");
    print_line.append(title);

    const bool no_title_to_display = (print_line.empty() && album.empty());
    if (no_title_to_display) {
      // No title, so show at least player name.
      print_line = player_name;
      CenterAlign(&print_line, this->width());
      this->Print(0, print_line);
    }

    // Second line: Show volume related things if relevant.
    // Either we're muted, or there was a volume change that we display
    // for kVolumeFlashTime
    if (muted) {
      print_line = "[Muted]";
      CenterAlign(&print_line, this->width());
      this->Print(1, print_line);
      return;
    }
    else if (volume != previous_volume || volume_countdown > 0) {
      if (!previous_volume.empty()) {
        if (volume != previous_volume)
          volume_countdown = kVolumeFlashTime;
        else
          --volume_countdown;
        std::string volume_line = "Volume " + volume;
        CenterAlign(&volume_line, this->width());
        this->Print(1, volume_line);
      }
      previous_volume = volume;
      return;
    }

    if (no_title_to_display) {
      // Nothing really to display ? Show play-state.
      print_line = play_state;
      if (play_state == "STOPPED")
        print_line = STOP_SYMBOL " [Stopped]";
      else if (play_state == "PAUSED_PLAYBACK")
        print_line = PAUSE_SYMBOL" [Paused]";
      else if (play_state == "PLAYING")
        print_line = PLAY_SYMBOL " [Playing]";

      CenterAlign(&print_line, this->width());
      this->Print(1, print_line);
      return;
    }

    // Alright, we have a title. If short enough, center, otherwise scroll.
    CenterAlign(&print_line, this->width());
    first_line_scroller.SetValue(print_line, this->width());
    this->Print(0, first_line_scroller.GetScrolledContent());

    std::string formatted_time;
    if (play_state == "STOPPED") {
      formatted_time = "  " STOP_SYMBOL " ";
    } else {
      formatted_time = formatTime(track_time);
      // 'Blinking' time when paused.
      if (play_state == "PAUSED_PLAYBACK" && blink_time % 2 == 0) {
        formatted_time = std::string(formatted_time.size(), ' ');
      }
    }
    const int remaining_len = this->width() - formatted_time.length() - 1;

    // Assemble second line from album. Add artist, but only if we wouldn't
    // exceed length (or, if we already exceed length, also append).
    print_line = album;

    std::string artist_addition;
    if (!artist.empty() && artist != album) {
      if (!print_line.empty()) artist_addition.append("/");
      artist_addition.append(artist);
    }
    // Only append it if we'd stay within allocated screen-width. Unless the
    // Album name is already so long that we'd exceed the length anyway. In
    // that case, we have to scroll no matter what and including the artist
    // does less harm.
    if (utf8_len(print_line + artist_addition) <= remaining_len
        || utf8_len(print_line) > remaining_len) {
      print_line += artist_addition;
    }

    // Show album/artist right aligned in space next to time. Or scroll if long.
    RightAlign(&print_line, remaining_len);
    second_line_scroller.SetValue(print_line, remaining_len);

    this->Print(1, formatted_time + " "
                    + second_line_scroller.GetScrolledContent());

    blink_time++;
    first_line_scroller.NextTick();
    second_line_scroller.NextTick();
}

void Printer::goodBye() {
   std::string msg = "Goodbye!";
   CenterAlign(&msg, this->width());
   this->Print(0, msg);
   // Show off unicode :)
   msg = "\u2192 \u266a\u266b\u266a\u2669 \u2190";  // → ♪♫♩ ←
   CenterAlign(&msg, this->width());
   this->Print(1, msg);
}

int Printer::parseTime(const std::string &upnp_time) {
   int hour = 0;
   int minute = 0;
   int second = 0;
   if (sscanf(upnp_time.c_str(), "%d:%02d:%02d", &hour, &minute, &second) == 3)
      return hour * 3600 + minute * 60 + second;
   return 0;
}

std::string Printer::formatTime(int time) {

   char buf[32];

   const bool is_neg = (time < 0);
   time = abs(time);
   const int hour = time / 3600; time %= 3600;
   const int minute = time / 60; time %= 60;
   const int second = time;

   char *pos = buf;

   if (is_neg) *pos++ = '-';

   if (hour > 0)
      snprintf(pos, sizeof(buf)-1, "%dh%02d:%02d", hour, minute, second);
   else
      snprintf(pos, sizeof(buf)-1, "%d:%02d", minute, second);

   return buf;
}

void Printer::CenterAlign(std::string *to_print, int width) {
   const int len = utf8_len(*to_print);
   if (len < width)
      to_print->insert(0, std::string((width - len) / 2, ' '));
}

void Printer::RightAlign(std::string *to_print, int width) {
   const int len = utf8_len(*to_print);
   if (len < width)
      to_print->insert(0, std::string(width - len, ' '));
}

