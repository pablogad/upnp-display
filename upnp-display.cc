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

#include "upnp-display.h"

#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>

#include <ithread.h>

#include "printer.h"
#include "renderer-state.h"

// Time between display updates.
// This influences scroll speed and 'pause' blinking.
// Note, too fast scrolling looks blurry on cheap displays.
static const int kDisplayUpdateMillis = 400;

// We do the signal receiving the classic static way, as creating callbacks to
// c functions is more readable than with c++ methods :)
volatile bool signal_received = false;
static void SigReceiver(int) {
  signal_received = true;
}

UPnPDisplay::UPnPDisplay(const std::string &friendly_name, Printer *printer,
                         int screensave_timeout)
  : player_match_name_(friendly_name),
    printer_(printer), screensave_timeout_(screensave_timeout),
    current_state_(NULL) {
  ithread_mutex_init(&mutex_, NULL);
  signal(SIGTERM, &SigReceiver);
  signal(SIGINT, &SigReceiver);
}

void UPnPDisplay::Loop() {

  time_t last_update = 0;

  signal_received = false;
  while (!signal_received) {

    usleep(kDisplayUpdateMillis * 1000);
    const time_t now = time(NULL);

    last_update = 0;
    bool renderer_available = false;

    ithread_mutex_lock(&mutex_);

    if (current_state_ != NULL) {
      renderer_available = true;
      printer_->fillVars(current_state_);

      last_update = current_state_->last_event_update();
    }
    ithread_mutex_unlock(&mutex_);

    if (screensave_timeout_ > 0 && last_update > 0 &&
        (now - last_update) > screensave_timeout_) {
      printer_->SaveScreen();
      continue;
    }

    if (!renderer_available)
      printer_->noRendererPrint();
    else
      printer_->rendererPrint( current_state_ );

  } // while loop

  printer_->goodBye();
}

void UPnPDisplay::AddRenderer(const std::string &uuid,
                              RendererState *state) {
  printf("%s: connected (uuid=%s)\n",            // not to LCD, different thread
         state->friendly_name().c_str(), uuid.c_str());
  ithread_mutex_lock(&mutex_);
  if (current_state_ == NULL
      && (player_match_name_.empty()
          || player_match_name_ == uuid
          || player_match_name_ == state->friendly_name())) {
    uuid_ = uuid;
    current_state_ = state;
  }
  ithread_mutex_unlock(&mutex_);
}

void UPnPDisplay::RemoveRenderer(const std::string &uuid) {
  printf("disconnect (uuid=%s)\n", uuid.c_str()); // not to LCD, different thread
  ithread_mutex_lock(&mutex_);
  if (current_state_ != NULL && uuid == uuid_) {
    current_state_ = NULL;
  }
  ithread_mutex_unlock(&mutex_);
}

