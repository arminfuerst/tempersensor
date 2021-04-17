/*
 * temperlib contains all functions for tempersensor that
 * can be tested automatically.
 * Additional infos (including a license notice) are at the end of this file.
 */

//#include <ctype.h>
#include <getopt.h>
//#include <stdarg.h>
//#include <stdbool.h>
//#include <stdio.h>
//#include <stdlib.h>
//#include <stdint.h>
//#include <string.h>
//#include <unistd.h>
//#include <math.h>
//#include <linux/hidraw.h>
//#include <sys/select.h>
//#include <fcntl.h>
//#include <errno.h>
//#include <dirent.h>
//#include "mrtg.h"

char *option_string(const struct option *long_options);

float fahrenheit(float celsius);

char *extend_errormessage(const char* errormessage, int errnum);

float calc_value(const unsigned char *valuestring, int startchar, int conversion_method);

int char_index(const char *string, char c);

/*
 * tempersensor Copyright (C) 2020-2021 Armin Fuerst (armin@fuerst.priv.at)
 * tempersensor is a complete rewrite based on the ideas from
 * pcsensor.c which was written by Juan Carlos Perez (cray@isp-sl.com)
 * based on Temper.c by Robert Kavaler (kavaler@diva.com)
 *
 * This file is part of tempersensor.
 *
 * Tempersensor is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Tempersensor is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with tempersensor.  If not, see <https://www.gnu.org/licenses/>.
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

