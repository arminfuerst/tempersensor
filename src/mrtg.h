/*
 * mrtglib is a collection of functions to support creating C programs
 * creating sources for MRTG.
 * Additional infos (including a license notice) are at the end of this file.
 */

#ifndef MRTG_H
#define MRTG_H

#define LIBMRTG_VERSION "0.1.1"
#define INVALID_VALUE "UNKNOWN"

#ifdef _WIN32
	#define MRTG_LIB_EXPORT __declspec(dllexport)
#else
	#define MRTG_LIB_EXPORT
#endif

/* 
 * print_mrtg_signature
 *
 * Prints time and given program name to fulfill MRTG syntax
 * If an errormessage is given, it is printed after programname 
 * in parenthesis.
 *
 */
MRTG_LIB_EXPORT void print_mrtg_signature(const char *programname, const char *version, const char *errormessage);

/*
 * print_mrtg_error
 *
 * Prints an errormessage as an appendix to the programname
 * and fulfills MRTG syntax by returning defined values
 */
MRTG_LIB_EXPORT void print_mrtg_error(const char *programname, const char *version, const char *errormessage);

/*
 * print_mrtg_values
 *
 * Prints in and out value and fulfills MRTG syntax
 */
MRTG_LIB_EXPORT void print_mrtg_values(const char *programname, const char *version, const char *in, const char *out);

/*
 * libmrtg_version
 *
 * returns the version of the library
 */

MRTG_LIB_EXPORT char *libmrtg_version(void);

#endif // MRTG_H

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
