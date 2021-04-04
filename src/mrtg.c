/*
 * mrtglib is a collection of functions to support creating C programs
 * creating sources for MRTG.
 * Additional infos (including a license notice) are at the end of this file.
 */

#include "mrtg.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// https://stackoverflow.com/questions/12523704/mac-os-x-equivalent-header-file-for-sysinfo-h-in-linux
#ifdef __linux__
#include <sys/sysinfo.h>
#elif __APPLE__
#include <sysctl.h>
#endif

/*
 * get_uptime
 *
 * returns the system uptime in seconds, 0 if an error occurs
 */
static long get_uptime(void)
{
#ifdef __linux__
	struct sysinfo s_info;
	int error = sysinfo(&s_info);
	if(error != 0)
	{
		return 0;
	}
	return s_info.uptime;
#elif __APPLE__
	return kern.boottime;
#endif
}

/*
 * timeStamp
 *
 * return human readable timestamp
 */
static char *timeStamp(void)
{
	char tmp[72];
	time_t t;
	struct tm *local;

	// get current time
	t = time(NULL);
	local = localtime(&t);

	(void)sprintf(tmp, "%04d-%02d-%02d %02d:%02d:%02d",
		local->tm_year + 1900,
		local->tm_mon + 1,
		local->tm_mday,
		local->tm_hour,
		local->tm_min,
		local->tm_sec);

	char *timestamp = (char *) malloc(strlen(tmp) + 1);
	strcpy(timestamp, tmp);

	return timestamp;
}

/*
 * extend_plural
 *
 * extends plural if the given value is != 1 and
 * adds a postfix
 */
static void extend_plural(long value, char* string, const char* extension)
{
	if (value != 1)
	{
		strcat(string, "s");
	}
	strcat(string, extension);
}

/*
 * print_unit
 *
 * prints a unit human readable, respecting plural
 */

static char *print_unit(long value, const char* unit, const char* extension, int no_omit)
{
	char *retstring;

	// long: max 11 characters, 1 blank, 1 for NULL
	retstring = malloc(strlen(unit) + 11 + 2);
	if ((value != 0) || (no_omit != 0))
	{
		sprintf(retstring, "%lu %s", value, unit);
		extend_plural(value, retstring, extension);
	}
	else
	{
		retstring[0] = 0;
	}

	return retstring;
}


/*
 * extract_part_from_seconds
 *
 * returns the amount in higher units from the given
 * value in seconds and reduces the seconds
 */
static int extract_part_from_seconds(long *seconds, char* unit)
{
	int seconds_of_unit = 1;
	int retval;

	if (!strcmp(unit, "week")) { seconds_of_unit = 24 * 60 * 60 * 7; }
	else if (!strcmp(unit, "day")) { seconds_of_unit = 24 * 60 * 60; }
	else if (!strcmp(unit, "hour")) { seconds_of_unit = 60 * 60; }
	else if (!strcmp(unit, "minute")) { seconds_of_unit = 60; }

	retval = (*seconds) / seconds_of_unit;
	(*seconds) -= retval * seconds_of_unit;

	return retval;
}

/*
 * pretty_print_time
 *
 * converts seconds to human readable time
 */
static char *pretty_print_time(long seconds)
{
	int weeks;
	int days;
	int minutes;
	int hours;
	char *retstring;
	char *tmp;

	weeks = extract_part_from_seconds(&seconds, "week");
	days = extract_part_from_seconds(&seconds, "day");
	hours = extract_part_from_seconds(&seconds, "hour");
	minutes = extract_part_from_seconds(&seconds, "minute");

	tmp = print_unit(weeks, "week", ", ", 0);
	retstring = malloc(strlen(tmp) + 1);
	strcpy(retstring, tmp);
	free(tmp);

	tmp = print_unit(days, "day", ", ", 0);
	retstring = (char *) realloc(retstring, 
		strlen(retstring) + strlen(tmp) + 1);
	strcat(retstring, tmp);
	free(tmp);

	tmp = print_unit(hours, "hour", ", ", 0);
	retstring = (char *) realloc(retstring, 
		strlen(retstring) + strlen(tmp) + 1);
	strcat(retstring, tmp);
	free(tmp);

	tmp = print_unit(minutes, "minute", " and ", 0);
	retstring = (char *) realloc(retstring, 
		strlen(retstring) + strlen(tmp) + 1);
	strcat(retstring, tmp);
	free(tmp);

	tmp = print_unit(seconds, "second", "", 1);
	retstring = (char *) realloc(retstring, 
		strlen(retstring) + strlen(tmp) + 1);
	strcat(retstring, tmp);
	free(tmp);

	return retstring;
}

MRTG_LIB_EXPORT void print_mrtg_signature(const char *programname, const char *version, const char *errormessage)
{
	long uptime;
	char *tmp;

	// 3rd line for mrtg: uptime (if available), else timestamp
	uptime = get_uptime();
	if (uptime > 0)
	{
		tmp = pretty_print_time(uptime);
		printf("%s\n", tmp);
		free(tmp);
	}
	else
	{
		printf("%s\n", timeStamp());
	}

	// 4th line for mrtg: Systemname
	if (strlen(errormessage) > 0)
	{
		printf("%s %s (%s)\n", programname, version, errormessage);
	}
	else
	{
		printf("%s %s\n", programname, version);
	}
}

MRTG_LIB_EXPORT void print_mrtg_error(const char *programname, const char *version, const char *errormessage)
{
	printf("%s\n%s\n", INVALID_VALUE, INVALID_VALUE);
	print_mrtg_signature(programname, version, errormessage);
}

MRTG_LIB_EXPORT void print_mrtg_values(const char *programname, const char *version, const char *in, const char *out)
{
	printf("%s\n%s\n", in, out);
	print_mrtg_signature(programname, version, "");
}

MRTG_LIB_EXPORT char *libmrtg_version(void)
{
	return LIBMRTG_VERSION;
}

/*
 * mrtglib Copyright (C) 2020 Armin Fuerst (armin@fuerst.priv.at)
 *
 * This file is part of mrtglib.
 *
 * mrtglib is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * mrtglib is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with mrtglib.  If not, see <https://www.gnu.org/licenses/>.
 *
 *
 * Why did I put the license notice at the bottom of the file?
 * Because most of the time, when you open the file, the probability
 * you want to know about the license is very little. If you open the
 * file to learn about the license, it is still easy to find.
 *
 * Perhaps you want to have a look at the talk "Clean Coders Hate What 
 * Happens to Your Code When You Use These Enterprise Programming Tricks"
 * from Kevlin Henney at the NDC Conferences in London, January 16th-20th 
 * 2017. While watching the whole talk is a good idea, starting at around 
 * 27:50 provides some input what to put on the top of source code files.
 */ 
