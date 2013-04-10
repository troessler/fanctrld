/*
 * fanctrld -- control the fan on an IBM Thinkpad with the ibm-acpi
 * module loaded.
 * 
 * This program turns on the fan once a certain threshold
 * temperature is exceeded.  It turns off the fan once the BIOS has
 * slowed it down below a certain threshold speed.
 * 
 * Copyright (c) 2005  Thomas Roessler  <roessler@does-not-exist.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 */ 


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <setjmp.h>

#include <unistd.h>
#include <syslog.h>
#include <signal.h>
#include <errno.h>



int current_thm (void);
int current_rpm (void);
void set_fan (int);

void handler (int);

#define IBM_ACPI_FAN "/proc/acpi/ibm/fan"
#define IBM_ACPI_THERMAL "/proc/acpi/ibm/thermal"
#define IBM_ACPI "/proc/acpi/ibm"

char *Progname;
int Debug = 0;

jmp_buf bail;
int jmpcnt = 0;
int interrupted = 0;

int main (int argc, char *argv[])
{
  char *p;
  int c;
  int threshold_rpm = 3500;
  int threshold_thm = 45;
  int poll_time = 5;
  
  int fan_running = 1;

  Progname = argv[0];
  
  if ((p = strrchr (Progname, '/')))
    Progname = p + 1;
  
  openlog (Progname, LOG_PID, LOG_DAEMON);
  
  while ((c = getopt (argc, argv, "r:t:p:d")) != -1)
  {
    switch (c) 
    {
      case 'r': threshold_rpm = atoi (optarg); break;
      case 't': threshold_thm = atoi (optarg); break;
      case 'p': poll_time = atoi (optarg); break;
      case 'd': Debug = 1; break;
      case '?': fprintf (stderr, "%s: unknown parameter.\n", Progname); return 1;
      case ':': fprintf (stderr, "%s: missing argument.\n", Progname); return 1;
    }
  }
  
  if (threshold_rpm < 1000)
  {
    fprintf (stderr, "%s: RPM threshold must be at least 1000.\n", Progname);
    return 1;
  }
  
  if (threshold_thm > 60)
  {
    fprintf (stderr, "%s: You like it hot.  Please give a lower thermal threshold.\n",
	     Progname);
    return 1;
  }
  
  if (threshold_thm < 0)
  {
    fprintf (stderr, "%s: You want hell to freeze? Please give a positive thermal threshold.\n",
	     Progname);
    return 1;
  }
  
  if (poll_time < 0 || poll_time > 600)
  {
    fprintf (stderr, "%s: Please select a poll interval between 0 and 600.\n",
	     Progname);
    return 1;
  }

  if (chdir (IBM_ACPI) == -1) 
  {
    fprintf (stderr, "%s: Can't chdir to %s -- %s.\n",
	     Progname, IBM_ACPI, strerror (errno));
    return 1;
  }
  
  signal (SIGHUP, handler);
  signal (SIGINT, handler);
  signal (SIGQUIT, handler);
  signal (SIGTERM, handler);
    
  if (daemon (0, 0) == -1)
  {
    fprintf (stderr, "%s: %s.  Exiting.", Progname, strerror (errno));
    return 1;
  }
  
  switch (setjmp (bail))
  {
    case 0: break;
    case 1:
      syslog (LOG_ALERT, "Trying to turn on the fan before exiting.");
      set_fan (1);
      closelog ();
      exit (1);
    default: 
      syslog (LOG_EMERG, "Too many errors, exiting. Risk of overheating.");
      closelog ();
      exit (1);
  }
  
  while (!interrupted)
  {
    int r = current_rpm();
    int t = current_thm();
    int d;

    d = fan_running = r > 0 ? 1 : 0;

    if (Debug)
      syslog (LOG_DEBUG, "r = %d, t = %d\n", r, t);

    if (t > threshold_thm)
      d = 1;
    else if (r < threshold_rpm)
      d = 0;
    
    if (fan_running != d)
    {
      syslog (LOG_INFO, "r = %d, t = %d  --  %s\n", r, t, d ? "enabling" : "disabling");
      set_fan (d);
    }

    sleep (poll_time);
    
  }
  
  set_fan (1);
  
  return 0;
}

int current_thm (void)
{
  char l[80];
  char *s;
  FILE *fp;
  
  int max = 0;
  
  if ((fp = fopen (IBM_ACPI_THERMAL, "r")) == NULL)
  {
    syslog (LOG_ALERT, "Can't open %s. %m.", IBM_ACPI_THERMAL);
    longjmp (bail, ++jmpcnt);
  }
  
  if (fgets (l, sizeof (l), fp) == NULL)
  {
    syslog (LOG_ALERT, "Can't parse %s: Premature end of file.", IBM_ACPI_THERMAL);
    longjmp (bail, ++jmpcnt);
  }
  
  fclose (fp);
  
  if ((s = strtok (l, ": ")) == NULL || strcasecmp (s, "temperatures") != 0)
  {
    syslog (LOG_ALERT, "Can't parse temperature. Exiting.");
    longjmp (bail, ++jmpcnt);
  }
  
  while ((s = strtok (NULL, ": ")))
  {
    int t = atoi (s);
    if (t > max)
      max = t;
  }
  
  return max;
}

int current_rpm (void)
{
  char l[80];
  FILE *fp;

  int rpm = 0;
  
  if ((fp = fopen(IBM_ACPI_FAN, "r")) == NULL)
  {
    syslog (LOG_ALERT, "Can't open %s: %m.", IBM_ACPI_FAN);
    longjmp (bail, ++jmpcnt);
  }
  
  while (fgets (l, sizeof (l), fp))
  {
    if (strncmp (l, "speed:", 6) == 0)
    {
      char *c;
      for (c = l + 7; *c && isspace (*c); c++)
	;
      rpm = atoi (c);
    }
  }
  
  fclose (fp);
  return rpm;
}

void set_fan (int state)
{
  FILE *fp;

  if ((fp = fopen (IBM_ACPI_FAN, "w")) == NULL)
  {
    syslog (LOG_ALERT, "Can't open %s: %m!", IBM_ACPI_FAN);
    longjmp (bail, ++jmpcnt);
  }
  
  fprintf (fp, "%s\n", state ? "enable" : "disable");
  
  if (ferror (fp))
  {
    syslog (LOG_ALERT, "Can't set fan state to %s.", state ? "enable" : "disable");
    longjmp (bail, ++jmpcnt);
  }
  
  fclose (fp);
}

void handler (int s)
{
  syslog (LOG_NOTICE, "Caught signal %d, exiting.", s);
  interrupted = 1;
}
