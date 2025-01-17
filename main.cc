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

#include <limits.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <iostream>

#include "controller-state.h"
#include "upnp-display.h"
#include "lcd-display.h"
#include "vfd-display.h"
#include "printer.h"

// Width of your display. Usually this is just 16 wide, but you can get 24 or
// even 40 wide displays. You can also set this via the -w option.
#define DEFAULT_LCD_DISPLAY_WIDTH 16

int main(int argc, char *argv[]) {
  std::string match_name;
  int display_width = DEFAULT_LCD_DISPLAY_WIDTH;
  bool as_daemon = false;
  bool on_console = false;

  bool on_lcd = false;
  bool on_vfd = false;
  std::string vfd_display_def_file;

  int screensave_after = -1;

  int opt;
  while ((opt = getopt(argc, argv, "hn:w:dclv:s:")) != -1) {
    switch (opt) {
    case 'n':
      if (optarg != NULL) match_name = optarg;
      break;

    case 'd':
      as_daemon = true;
      break;

    case 'l':
      on_lcd = true;
      break;

    case 'v':
      on_vfd = true;
      vfd_display_def_file = optarg;
      break;

    case 'c':
      on_console = true;
      break;

    case 'w': {
      int w = atoi(optarg);
      if (w < 8) {
        fprintf(stderr, "Invalid width %d\n", w);
        return 1;
      }
      display_width = w;
      break;
    }

    case 's':
      screensave_after = atoi(optarg);
      break;

    case 'h':
    default:
      fprintf(stderr, "Usage: %s <options>\n", argv[0]);
      fprintf(stderr, "\t-n <name or \"uuid:\"<uuid>"
              ": Connect to this renderer.\n"
              "\t-l                       : Use LCD display.\n"
              "\t-v <display-def>         : Use VFD display with specified definition file.\n"
              "\t-w <display-width>       : Set LCD display width.\n"
              "\t-d                       : Run as daemon.\n"
              "\t-c                       : On console instead LCD (debug).\n"
              "\t-s <timeout-seconds>     : Screensave after this time.\n"
              );
      return 1;
    }
  }

  Printer *printer = NULL;
  if (on_console || (!on_lcd && !on_vfd)) {
    printer = new ConsolePrinter(match_name, display_width);
  } else {
    if (on_lcd) {
      LCDDisplay *display = new LCDDisplay(match_name, display_width);
      if (!display->Init()) {
        fprintf(stderr, "You need to run this as root to have access "
                "to GPIO pins. Run with sudo (or with option -c to output on "
                "console instead).\n");
        return 1;
      }

      // The LCDs have timeouts when certain write operations take too long,
      // so make sure we tell the kernel we're serious about timing.
      struct sched_param p;
      p.sched_priority = 99;
      if (sched_setscheduler(0, SCHED_FIFO, &p) < 0) {
          fprintf(stderr,
                  "Couldn't set realtime priority which we need to make sure "
                  "hardware timing is correct .\nConsider running as root or "
                  "granting CAP_SYS_NICE capability "
                  "(sudo setcap cap_sys_nice=eip <program>).\n");
      }

      printer = display;
      std::cout << "Printer: LCD" << std::endl;
    }
    else if (on_vfd) {

      VFDDisplay *display = new VFDDisplay(match_name, vfd_display_def_file);

      if (!display->Init()) {
        fprintf(stderr, "You need to run this as root to have access "
                "to GPIO pins. Run with sudo (or with option -c to output on "
                "console instead).\n");
        return 1;
      }

      printer = display;
      std::cout << "Printer: VFD" << std::endl;
    }
  }

  // TODO: if running as root: drop priviliges now.

  if (as_daemon) {
    daemon(0, 0);
  }

  UPnPDisplay ui(match_name, printer, screensave_after);
  ControllerState controller(&ui, printer);
  ui.Loop();

  delete printer;

  return 0;
}
