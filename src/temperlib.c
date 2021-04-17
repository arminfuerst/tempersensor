/*
 * temperlib contains all functions for tempersensor that
 * can be tested automatically.
 * Additional infos (including a license notice) are at the end of this file.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "temperlib.h"

/*
 * option_string
 *
 * generate the string with options for getopt_long based
 * on the struct so additional parameters don't have to be
 * added on two places.
 */

char *option_string(const struct option *long_options)
{
	int i;
	unsigned long len;
	char *returnstring;
	char tmp[2];

	returnstring = (char *) malloc(1);
	returnstring[0] = 0;
	if (long_options == NULL)
		return returnstring;
	for (i = 0; long_options[i].name; i++)
	{
		if (((long_options[i].val >= 65) &&
			(long_options[i].val <= 90)) ||
			((long_options[i].val >= 97) &&
			(long_options[i].val <= 122)))
		{
			len = strlen(returnstring);
			if (long_options[i].has_arg)
			{
				returnstring = (char *) realloc(returnstring, len + 2 + 1);
			}
			else
			{
				returnstring = (char *) realloc(returnstring, len + 1 + 1);
			}
			(void)sprintf(tmp,"%c", long_options[i].val);
			strcat(returnstring, tmp);
			if (long_options[i].has_arg)
			{
				strcat(returnstring,":");
			}
		}
	}
	return returnstring;
}

float fahrenheit(float celsius)
{
	return (float)((celsius * (9.0 / 5.0)) + 32.0);
}

/*
 * extend_errormessage
 *
 * extends the given errormessage with the textual representation
 * of the errno error
 */

char *extend_errormessage(const char* errormessage, int errnum)
{
	char *fullmsg;
	
	fullmsg = (char *) malloc(strlen(errormessage) +
		strlen(strerror(errnum)) + 3);
	sprintf(fullmsg, "%s: %s", errormessage,
		strerror(errnum));
	
	return fullmsg;
}

/*
 * calc_value
 *
 * calculates value from response from given starting character
 */

float calc_value(const unsigned char *valuestring, int startchar, int conversion_method)
{
	/*
	 * To be improved: send only a string with two characters
	 * and make selection of the correct characters outside this
	 * function, combine it with autodetection based on Firmware string.
	 */

	/*
	   reverse engineering results for TEMPer 1.3:
	    + relevant for this device are positions 4 + 5
	    + in good case, the lower sigificant part of position 5
	      is always 0
	    + if error case, the lower sigificant part of position 5
	      is F (or not zero?)
	    + the value is represented in two's complement
	    + position 4 is the part before the comma
	    + the higher 4 bits of position 5 are after the comma
	 */

	if (conversion_method == 1)
	{
		/*
		 * conversion_method 1: value in two's complement with fraction
		 * If MSB is set, the temperature is negative in 2'complement form.
		 * Since we cannot know how negative numbers are on the system,
		 * we calculate it manually. Since TEMPer only has 4 bits after the
		 * comma and they are the higher part of the byte, we shift the two
		 * bytes after concatinating 4 bits to the right, deal with 12 bits
		 * (0xFFF) and divide by 16.
		 * Alternatively, we could omit the shift to the right, deal with
		 * 16 bits (0xFFFF) and divide by 256.
		 */

		/* see whether we have valid result */
		if ((valuestring[startchar + 1] & 0x0F) != 0)
		{
			return -999.0;
		}

		if ((valuestring[startchar] & 0x80) != 0)
		{
			// convert fixed comma from 2'complement form, < 0
			// 1. ignore comma
			// 2. subtract 1
			// 3. invert bits
			// 4. multiply by -1
			// 5. divide by amount of distinct values behind the comma
			//    - here we calculate with 4 bits => 2 by power of 4

			return (float)(((((((valuestring[startchar] << 8) +
				valuestring[startchar + 1]) >> 4) - 1) ^ 0xFFF) * -1) / pow(2, 4));
		}
		else
		{
			// convert fixed comma from 2'complement form, >= 0
			// 1. ignore comma
			// 2. divide by amount of distinct values behind the comma
			//    - here we calculate with 4 bits => 2 by power of 4

			return (float)((((valuestring[startchar] << 8) + valuestring[startchar + 1]) >> 4) / pow(2, 4));

		}
	}
	else if (conversion_method == 2)
	{
		/*
		 * conversion_method 2: value in two's complement multiplied
		 * by 100, e.g. 22.06 °C = 2206
		 */
		if ((valuestring[startchar] & 0x80) != 0)
		{
			// convert two's complement, < 0
			// 1. convert two bytes to integer
			// 2. subtract 1 from integer
			// 3. invert bits
			// 4. make negative
			// 5. after conversion, divide by 100
			return (float)((((((valuestring[startchar] << 8) + valuestring[startchar + 1])
				- 1) ^ 0xFFFF) * -1) / 100.0);
		}
		else
		{
			// convert two's complement, >= 0
			// 1. convert to bytes to integer
			// 2. after conversion, divide by 100
			return (float)(((valuestring[startchar] << 8) + valuestring[startchar + 1]) / 100.0);
		}
	}
	else
	{
		/*
		 * if an unknown conversion_method is defined,
		 * return invalid value
		 */
		return -999.0;
	}
}

int char_index(const char *string, char c)
{
	for (int i = 0; string[i] != '\0'; i++)
		if (string[i] == c)
			return i;

	return -1;
}

//static void test_calc()
//{
//	unsigned char answer[4096];
//	float tmp;
//
//	// there will only be an output in debug mode
//	config.debug = 1;
//	device.conversion_method = 1;
//
//	memmove(answer, (unsigned char[8]){ 0x80, 0x04, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00 }, 8);
//	tmp = calc_value(answer, 4);
//	debug_print("temp: %.4f / expected: error (-999.00)\n", tmp);
//
//	memmove(answer, (unsigned char[8]){ 0x80, 0x04, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00 }, 8);
//	tmp = calc_value(answer, 2);
//	debug_print("temp: %.4f / expected: error (-999.00)\n", tmp);
//
//	memmove(answer, (unsigned char[8]){ 0x80, 0x04, 0x1a, 0x1a, 0x1a, 0x10, 0x00, 0x00 }, 8);
//	tmp = calc_value(answer, 4);
//	debug_print("temp: %.4f / expected: 20.0625\n", tmp);
//
//	memmove(answer, (unsigned char[8]){ 0x80, 0x04, 0x14, 0x14, 0x14, 0xd0, 0x00, 0x00 }, 8);
//	tmp = calc_value(answer, 4);
//	debug_print("temp: %.4f / expected: 20.8125\n", tmp);
//
//	memmove(answer, (unsigned char[8]){ 0x80, 0x04, 0x01, 0x01, 0x01, 0x60, 0x00, 0x00 }, 8);
//	tmp = calc_value(answer, 4);
//	debug_print("temp: %.4f / expected: 1.3750\n", tmp);
//
//	memmove(answer, (unsigned char[8]){ 0x80, 0x04, 0x00, 0x00, 0x00, 0x60, 0x00, 0x00 }, 8);
//	tmp = calc_value(answer, 4);
//	debug_print("temp: %.4f / expected: 0.3750\n", tmp);
//
//	memmove(answer, (unsigned char[8]){ 0x80, 0x04, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00 }, 8);
//	tmp = calc_value(answer, 4);
//	debug_print("temp: %.4f / expected: 0.0625\n", tmp);
//
//	memmove(answer, (unsigned char[8]){ 0x80, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 8);
//	tmp = calc_value(answer, 4);
//	debug_print("temp: %.4f / expected: 0.0000\n", tmp);
//
//	memmove(answer, (unsigned char[8]){ 0x80, 0x04, 0x00, 0x00, 0xff, 0xf0, 0x00, 0x00 }, 8);
//	tmp = calc_value(answer, 4);
//	debug_print("temp: %.4f / expected: -0.0625\n", tmp);
//
//	memmove(answer, (unsigned char[8]){ 0x80, 0x04, 0xff, 0xff, 0xff, 0x40, 0x00, 0x00 }, 8);
//	tmp = calc_value(answer, 4);
//	debug_print("temp: %.4f / expected: -0.7500\n", tmp);
//
//	memmove(answer, (unsigned char[8]){ 0x80, 0x04, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00 }, 8);
//	tmp = calc_value(answer, 4);
//	debug_print("temp: %.4f / expected: -1.0000\n", tmp);
//
//	memmove(answer, (unsigned char[8]){ 0x80, 0x04, 0xfe, 0xfe, 0xfe, 0xf0, 0x00, 0x00 }, 8);
//	tmp = calc_value(answer, 4);
//	debug_print("temp: %.4f / expected: -1.0625\n", tmp);
//
//	memmove(answer, (unsigned char[8]){ 0x80, 0x04, 0xfe, 0xfe, 0xfe, 0x00, 0x00, 0x00 }, 8);
//	tmp = calc_value(answer, 4);
//	debug_print("temp: %.4f / expected: -2.0000\n", tmp);
//
//	memmove(answer, (unsigned char[8]){ 0x80, 0x04, 0xfd, 0xfd, 0xfd, 0xf0, 0x00, 0x00 }, 8);
//	tmp = calc_value(answer, 4);
//	debug_print("temp: %.4f / expected: -2.0625\n", tmp);
//
//	memmove(answer, (unsigned char[8]){ 0x80, 0x04, 0xfe, 0xfe, 0x01, 0x00, 0x00, 0x00 }, 8);
//	tmp = calc_value(answer, 4);
//	debug_print("temp: %.4f / expected: 1.0000\n", tmp);
//
//	memmove(answer, (unsigned char[8]){ 0x80, 0x04, 0xfe, 0xfe, 0x00, 0x00, 0x00, 0x00 }, 8);
//	tmp = calc_value(answer, 4);
//	debug_print("temp: %.4f / expected: 0.0000\n", tmp);
//
//	memmove(answer, (unsigned char[8]){ 0x80, 0x04, 0x00, 0x00, 0xff, 0xf0, 0x00, 0x00 }, 8);
//	tmp = calc_value(answer, 4);
//	debug_print("temp: %.4f / expected: -0.0625\n", tmp);
//
//	memmove(answer, (unsigned char[8]){ 0x80, 0x04, 0xfe, 0xfe, 0xFF, 0x00, 0x00, 0x00 }, 8);
//	tmp = calc_value(answer, 4);
//	debug_print("temp: %.4f / expected: -1.0000\n", tmp);
//
//	memmove(answer, (unsigned char[8]){ 0x80, 0x04, 0xfe, 0xfe, 0xFE, 0x00, 0x00, 0x00 }, 8);
//	tmp = calc_value(answer, 4);
//	debug_print("temp: %.4f / expected: -2.0000\n", tmp);
//
//	memmove(answer, (unsigned char[8]){ 0x80, 0x04, 0xfe, 0xfe, 0xFD, 0x00, 0x00, 0x00 }, 8);
//	tmp = calc_value(answer, 4);
//	debug_print("temp: %.4f / expected: -3.0000\n", tmp);
//
//	memmove(answer, (unsigned char[8]){ 0x80, 0x04, 0xee, 0xee, 0xee, 0x40, 0x00, 0x00 }, 8);
//	tmp = fahrenheit(calc_value(answer, 4));
//	debug_print("temp: %.4f / expected: 0.0500\n", tmp);
//
//	memmove(answer, (unsigned char[8]){ 0x80, 0x04, 0xee, 0xee, 0xee, 0x30, 0x00, 0x00 }, 8);
//	tmp = fahrenheit(calc_value(answer, 4));
//	debug_print("temp: %.4f / expected: -0.0625\n", tmp);
//
//	memmove(answer, (unsigned char[8]){ 0x80, 0x04, 0xfe, 0xfe, 0x00, 0x00, 0x00, 0x00 }, 8);
//	tmp = fahrenheit(calc_value(answer, 4));
//	debug_print("temp: %.4f / expected: 32.0000\n", tmp);
//
//	memmove(answer, (unsigned char[8]){ 0x80, 0x04, 0x23, 0x23, 0x23, 0x90, 0x00, 0x00 }, 8);
//	tmp = fahrenheit(calc_value(answer, 4));
//	debug_print("temp: %.4f / expected: 96.0125\n", tmp);
//
//	device.conversion_method = 2;
//
//	memmove(answer, (unsigned char[8]){ 0x80, 0x04, 0x09, 0x66, 0x00, 0x00, 0x00, 0x00 }, 8);
//	tmp = calc_value(answer, 2);
//	debug_print("temp: %.4f / expected: 24.0600\n", tmp);
//
//	memmove(answer, (unsigned char[8]){ 0x80, 0x04, 0xf6, 0x9a, 0x00, 0x00, 0x00, 0x00 }, 8);
//	tmp = calc_value(answer, 2);
//	debug_print("temp: %.4f / expected: -24.0600\n", tmp);
//
//	memmove(answer, (unsigned char[8]){ 0x80, 0x04, 0x05, 0x90, 0x00, 0x00, 0x00, 0x00 }, 8);
//	tmp = calc_value(answer, 2);
//	debug_print("temp: %.4f / expected: 14.2400\n", tmp);
//
//	memmove(answer, (unsigned char[8]){ 0x80, 0x04, 0xfa, 0x70, 0x00, 0x00, 0x00, 0x00 }, 8);
//	tmp = calc_value(answer, 2);
//	debug_print("temp: %.4f / expected: -14.2400\n", tmp);
//
//	/* some values provided by Samuel Progin from TEMPer V1.4:
//	 * 80 02 19 30 65 72 46 31
//	 * 80 02 19 40 65 72 46 31
//	 * 80 02 19 50 65 72 46 31
//	 * 80 02 19 60 65 72 46 31
//	 * 80 02 1a 90 65 72 46 31
//	 * Temperature sensor seems to be starting at offset 2
//	 * probably the byte at offset 1 defines where the sensor values can be read?
//	 * probably some other bytes define what/how many sensors are present?
//	 * Compare to TEMPer V1.3:
//	 * 80 04 06 06 06 f0 00 00
//	 * Compare to TEMPerHUM (TEMPerX_V3.3)
//	 * 80 40 0a f9 14 63 00 00
//	 * 80 40 0b 5c 14 94 00 00
//	 * Offset 0: constant 80 - could mean one temperature sensor?
//	 * Offset 1: 04 at V1.3 - this is the offset where temperature sensor has it's values
//	 *           02 at V1.4 - this is the offset where temperature sensor has it's values
//	 *           40 at TEMPerHUM (TEMPerX_V3.3) - no idea what this could mean...
//	 * Offset 2: V1.3: has always the same value as offset 3 and 4
//	 *           V1.4: first byte of value
//	 * Offset 3: V1.3: has always the same value as offset 2 and 4
//	 *           V1.4: second byte of value
//	 * Offset 4: V1.3: first byte of value
//	 *           V1.4: constant 65
//	 * Offset 5: V1.3: second byte of value
//	 *           V1.4: constant 72
//	 * Offset 6: V1.3: constant 00
//	 *           V1.4: constant 46
//	 * Offset 7: V1.3: constant 00
//	 *           V1.4: constant 31
//	 */
//	device.conversion_method = 1;
//
//	memmove(answer, (unsigned char[8]){ 0x80, 0x02, 0x1a, 0x90, 0x65, 0x72, 0x46, 0x31 }, 8);
//	tmp = calc_value(answer, 2);
//	debug_print("temp: %.4f / expected: 26.5625\n", tmp);
//
//	exit(EXIT_SUCCESS);
//}

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

