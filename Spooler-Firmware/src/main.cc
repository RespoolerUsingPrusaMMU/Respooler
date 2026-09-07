//--------------------------------------------------------------------
/**
 * @file main.cc
 * @brief Provides the entry point for the Prusa-controller filament rewinder.
 *
 * Copyright (C) 2026 Andre Pruitt
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 *
 * @author Andre Pruitt (andre@pruittfamily.com)
 * @date 2026-09-03
 *
 * @details
 * The application initializes the reused Prusa MMU controller board, creates
 * the high-level rewinder state machine, enables interrupts, and then services
 * the rewinder continuously.  Time-critical step generation is performed by
 * timer interrupts owned by the motion controller.
 *
 * The pure-virtual handler intentionally traps forever because dynamic recovery
 * is not possible on this embedded target and exceptions are disabled.
 */
//--------------------------------------------------------------------
// Global includes
#include <avr/interrupt.h>

// Local includes
#include "app/Rewinder.hh"
#include "hardware/Board.hh"
#include "hardware/Debug.hh"

//--------------------------------------------------------------------
//--------------------------------------------------------------------
int main()
{
  // Construct all hardware abstractions statically on the main stack.  No heap
  // allocation is used by the spooler application.
  spooler::hardware::Board tBoard;
  const bool tDriversOk = tBoard.init();

#ifdef SPOOLER_DEBUG
  spooler::hardware::Debug::init();
  spooler::hardware::Debug::printLine("Spooler starting");
#endif

  spooler::Rewinder tRewinder(tBoard);
  tRewinder.init(tDriversOk);

  // Interrupts are enabled only after the board, timers, drivers, and state
  // machine are fully initialized.
  sei();

  for(;;)
  {
    tRewinder.update();
  }

  return 0;
}

//--------------------------------------------------------------------
/**
 * @brief Pure virtual function handler.
 *
 * This function is called when a pure virtual function is invoked.
 * It traps forever because dynamic recovery is not possible on this embedded target.
 */
//--------------------------------------------------------------------
extern "C" void __cxa_pure_virtual()
{
  for(;;)
  {
  }
}
//--------------------------------------------------------------------
