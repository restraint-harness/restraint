/* Copyright (C) 1998-2018 Free Software Foundation, Inc.
   This file is part of the GNU C Library.
   Contributed by Zack Weinberg <zack@rabi.phys.columbia.edu>, 1998.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <http://www.gnu.org/licenses/>.  */

#include <sys/types.h>
#include <termios.h>
#include <unistd.h>
#include <utmp.h>
#include <pty.h>

/* Pass in a setup() function that will be called by the child process
   This is used to reset signal handlers after a fork() */

int
restraint_forkpty (int *amaster, char *name, const struct termios *termp,
	 const struct winsize *winp, void (*setup)(void))
{
  int master, slave, pid;

  /* old versions of glibc do not declare terp and winp as const.
     This casting can be removed when we drop RHEL5 support */
  if (openpty (&master, &slave, name, (struct termios*)termp, (struct winsize*)winp) == -1)
    return -1;

  switch (pid = fork ())
    {
    case -1:
      close (master);
      close (slave);
      return -1;
    case 0:
      /* Child.  */
      close (master);
      if(setup != NULL) {
        setup();
      }
      if (login_tty (slave))
	_exit (1);

      return 0;
    default:
      /* Parent.  */
      *amaster = master;
      close (slave);

      return pid;
    }
}
