/*
 * Copyright 2013 Marco Lizza (marco.lizza@gmail.com)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License. 
 */

///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2006-2011 OpenNETCF Consulting
//
// Permission is hereby granted, free of charge, to any person obtaining a copy 
// of this software and associated documentation files (the "Software"), to 
// deal in the Software without restriction, including without limitation the 
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or 
// sell copies of the Software, and to permit persons to whom the Software is 
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in 
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
// DEALINGS IN THE SOFTWARE. 
//
// Version 0.01 - March 22, 2007
//                Initial Release
//
// Version 0.02 - July 5, 2007
//                Bug fixes.  UTC offset not properly accounted for unless SetTz had been previously called.
//                            UTC offset used for functions like localtime_ce using old, rather than current data
//
// For the latest source, visit http://time.codeplex.com
///////////////////////////////////////////////////////////////////////////////

// Many but not all of the functions were created from the following source:
//
// Copyright (C) 2002 Michael Ringgaard. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 
// 1. Redistributions of source code must retain the above copyright 
//    notice, this list of conditions and the following disclaimer.  
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.  
// 3. Neither the name of the project nor the names of its contributors
//    may be used to endorse or promote products derived from this software
//    without specific prior written permission. 
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
// OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
// OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
// SUCH DAMAGE.
/////////////////////////////////////////////////////////////////////////////// 

#include <windows.h>
//#include <time.h>
#include "sys/timeb.h"
#include <stdio.h>
#include <winsock2.h>
#include <errno.h>
#include "internal.h"
//#include <stdlib.h>		// time_t defined there



///////////////////////////////////////////////////////////////////////////////
// Macros
///////////////////////////////////////////////////////////////////////////////
#define TIME_FAILURE	0xFFFFFFFF
#define ASC_BUFF_SIZE	26  // Ascii buffer size is 26 bytes, (24 chars and CR+LF)
#define SEC_IN_HOUR     3600L
#define SECS_IN_MIN		60L
#define DAYSPERWEEK		7
#define YEAR0           1900
#define EPOCH_YR        1970
#define SECS_DAY        (24L * 60L * 60L)
#define LEAPYEAR(year)  (!((year) % 4) && (((year) % 100) || !((year) % 400)))
#define TIME_MAX        2147483647L


static const FILETIME	ftJan1970 = {3577643008,27111902};



static struct tm tmbuf;

///////////////////////////////////////////////////////////////////////////////
// Local Variables
///////////////////////////////////////////////////////////////////////////////

// Number of seconds between local time and UTC time, includes DST bias
//
LONG _localtime;

// Is the local time in daylight savings time
//
DWORD _isdst;

// Bias for daylight savings time
//
int _dstBias;

// Contains the time zone string
//
char tz_name[2][32];

// Contains the 1/1/1970 reference date/time
//
const SYSTEMTIME st1970 = {1970, 1,	4, 1, 0, 0, 0, 0};

// Contains the number of days per month for 
// non leap and leap years
//
const int _ytab[2][12] = 
{
  {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
  {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}
};

// Contains the days of the week abreviation
//
static char *aday[] = {
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

// Contains the days of the week full name
//
static char *day[] = {
    "Sunday", "Monday", "Tuesday", "Wednesday",
    "Thursday", "Friday", "Saturday"
};

// Contains the months of the year abreviation
//
static char *amonth[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

// Contains the months of the year full name
//
static char *month[] = {
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
};

///////////////////////////////////////////////////////////////////////////////
// Forward declaration of internal functions
///////////////////////////////////////////////////////////////////////////////

// Convert system time into seconds since 1970
//
LONGLONG SystemTimeToSecondsSince1970(SYSTEMTIME * st);

// Convert seconds since 1970 into a system time
//
void SecondsSince1970ToSystemTime(const time_t * timer, SYSTEMTIME * st, 
								  BOOLEAN local);

// Initialize the time zone information needed for the time_ce methods
//
void SetTz(SYSTEMTIME *_st);

// Copy system time structure to tm structure
//
void SystemTimeToTm(SYSTEMTIME *st, struct tm * tmbuffer);
void TmToSystemTime(struct tm * tmbuffer, SYSTEMTIME *st);

// Get Julian Days from the begining of the year
//
DWORD JulianDays(SYSTEMTIME * st);

// Method parses and formats strings
//
static void strfmt(char *str, const char *fmt, ...);


// Reentrant version of gmttime
//
struct tm *gmtime_r_ce(const time_t *timer, struct tm *tmbuf, BOOLEAN local);

// Reentrant version of localtime
//
struct tm *localtime_r_ce(const time_t *timer, struct tm *tmbuf);


struct tm *localtime(const time_t *timer);

size_t strftime(char *s, size_t maxs, const char *f, const struct tm *t);


static void shl64(ULARGE_INTEGER* value, int shift)
{
	ULARGE_INTEGER	result;

	if (value == NULL || shift <= 0)
		return;

	result.HighPart = (value->HighPart << shift) | (value->LowPart >> (32-shift));
	result.LowPart = (value->LowPart << shift);

	value->HighPart = result.HighPart;
	value->LowPart = result.LowPart;
}


static void shr64(ULARGE_INTEGER* value, int shift)
{
	ULARGE_INTEGER	result;

	if (value == NULL || shift <= 0)
		return;

	result.HighPart = (value->HighPart >> shift);
	result.LowPart = (value->LowPart >> shift) | (value->HighPart << (32-shift));

	value->HighPart = result.HighPart;
	value->LowPart = result.LowPart;
}


// Subtract valueToSubtract from value (doesn't handle underflow)
static void sub64(ULARGE_INTEGER* value, ULARGE_INTEGER* valueToSubtract)
{
	if (value == NULL || valueToSubtract == NULL)
		return;

	if (value->LowPart < valueToSubtract->LowPart)
		value->HighPart--;	// borrow from HighPart
	value->HighPart -= valueToSubtract->HighPart;
	value->LowPart -= valueToSubtract->LowPart;
}



// Returns position of top non-zero bit,
// eg. -1 = none set, 0 = first bit, 31 = top bit
static int topBit(DWORD value)
{
	if (value == 0)
		return -1;
	else if (value & 0xffff0000)
	{	// bit in 0xffff0000 is set
		if (value & 0xff000000)
		{	// bit in 0xff000000 is set
			if (value & 0xf0000000)
			{	// bit in 0xf0000000 is set
				if (value & 0xc0000000)
				{	// bit in 0xc0000000 is set
					if (value & 0x80000000)
						return 31;
					else
						return 30;
				}
				else
				{	// bit in 0x30000000 is set
					if (value & 0x20000000)
						return 29;
					else
						return 28;
				}
			}
			else
			{	// bit in 0x0f000000 is set
				if (value & 0x0c000000)
				{	// bit in 0x0c000000 is set
					if (value & 0x08000000)
						return 27;
					else
						return 26;
				}
				else
				{	// bit in 0x03000000 is set
					if (value & 0x02000000)
						return 25;
					else
						return 24;
				}
			}
		}
		else
		{	// bit in 0x00ff0000 is set
			if (value & 0x00f00000)
			{	// bit in 0x00f00000 is set
				if (value & 0x00c00000)
				{	// bit in 0x00c00000 is set
					if (value & 0x00800000)
						return 23;
					else
						return 22;
				}
				else
				{	// bit in 0x00300000 is set
					if (value & 0x00200000)
						return 21;
					else
						return 20;
				}
			}
			else
			{	// bit in 0x000f0000 is set
				if (value & 0x000c0000)
				{	// bit in 0x000c0000 is set
					if (value & 0x00080000)
						return 19;
					else
						return 18;
				}
				else
				{	// bit in 0x00030000 is set
					if (value & 0x00020000)
						return 17;
					else
						return 16;
				}
			}
		}
	}
	else
	{	// bit in 0x0000ffff is set
		if (value & 0x0000ff00)
		{	// bit in 0x0000ff00 is set
			if (value & 0x0000f000)
			{	// bit in 0x0000f000 is set
				if (value & 0x0000c000)
				{	// bit in 0x0000c000 is set
					if (value & 0x00008000)
						return 15;
					else
						return 14;
				}
				else
				{	// bit in 0x00003000 is set
					if (value & 0x00002000)
						return 13;
					else
						return 12;
				}
			}
			else
			{	// bit in 0x00000f00 is set
				if (value & 0x00000c00)
				{	// bit in 0x00000c00 is set
					if (value & 0x00000800)
						return 11;
					else
						return 10;
				}
				else
				{	// bit in 0x00000300 is set
					if (value & 0x00000200)
						return 9;
					else
						return 8;
				}
			}
		}
		else
		{	// bit in 0x000000ff is set
			if (value & 0x000000f0)
			{	// bit in 0x000000f0 is set
				if (value & 0x000000c0)
				{	// bit in 0x000000c0 is set
					if (value & 0x00000080)
						return 7;
					else
						return 6;
				}
				else
				{	// bit in 0x00000030 is set
					if (value & 0x00000020)
						return 5;
					else
						return 4;
				}
			}
			else
			{	// bit in 0x0000000f is set
				if (value & 0x0000000c)
				{	// bit in 0x0000000c is set
					if (value & 0x00000008)
						return 3;
					else
						return 2;
				}
				else
				{	// bit in 0x00000003 is set
					if (value & 0x00000002)
						return 1;
					else
						return 0;
				}
			}
		}
	}
}


// Returns position of top non-zero bit,
// eg. -1 = none set, 0 = first bit, 63 = top bit
static int topBit64(ULARGE_INTEGER* value)
{
	int		result;

	if (value == NULL)
		return 0;

	result = topBit(value->HighPart);
	if (result != -1)
		return result+32;
	else
		return topBit(value->LowPart);
}


// Compare two 64-bit values.
// Returns:
//   <0 if a is less than b
//   0  if a is equal to b
//   >0 if a is greater than b
static int cmp64(const ULARGE_INTEGER* a, const ULARGE_INTEGER* b)
{
	if (a == NULL || b == NULL)
		return -1;	// error but no error return value

	if (a->HighPart < b->HighPart)
		return -1;
	else if (a->HighPart == b->HighPart)
	{
		if (a->LowPart < b->LowPart)
			return -1;
		else if (a->LowPart == b->LowPart)
			return 0;
		else	// if (a->LowPart > b->LowPart)
			return 1;
	}
	else	// if (a->HighPart < b->HighPart)
		return 1;
}


// Add valueToAdd to value (doesn't handle overflow)
static void add64(ULARGE_INTEGER* value, ULARGE_INTEGER* valueToAdd)
{
	if (value == NULL || valueToAdd == NULL)
		return;

	value->LowPart += valueToAdd->LowPart;
	if (value->LowPart < valueToAdd->LowPart)
		value->HighPart++;	// carry to HighPart
	value->HighPart += valueToAdd->HighPart;
}


static void div64(ULARGE_INTEGER* value, DWORD divisor)
{
	ULARGE_INTEGER	result = { 0, 0 };
	ULARGE_INTEGER	shiftedDivisor;	// divisor shifted to left
	ULARGE_INTEGER	shiftedOne;		// '1' shifted to left by same number of bits as divisor
	int				shift;

	if (value == NULL)
		return;
	if (divisor == 0)
	{
		value->LowPart = 0;
		value->HighPart = 0;
		return;
	}
	if (value->HighPart == 0)
	{
		if (value->LowPart != 0)
			value->LowPart /= divisor;
		return;
	}

	// shift divisor up (into shifted) as far as it can go before it is greater than value
	shift = topBit64(value) - topBit(divisor);
	shiftedDivisor.LowPart = divisor;
	shiftedDivisor.HighPart = 0;
	shiftedOne.LowPart = 1;
	shiftedOne.HighPart = 0;
	shl64(&shiftedDivisor, shift);
	shl64(&shiftedOne, shift);
	while (shift >= 0)
	{
		if (cmp64(&shiftedDivisor, value) <= 0)
		{
			add64(&result, &shiftedOne);
			sub64(value, &shiftedDivisor);
		}
		shr64(&shiftedDivisor, 1);
		shr64(&shiftedOne, 1);
		shift--;
	}

	value->HighPart = result.HighPart;
	value->LowPart = result.LowPart;
}




///////////////////////////////////////////////////////////////////////////////
// Methods - The meat
///////////////////////////////////////////////////////////////////////////////

// Convert tm to a string in the format "Www Mmm dd hh:mm:ss yyyy", 
// where Www is the weekday, Mmm the month in letters, dd the day 
// of the month, hh:mm:ss the time, and yyyy the year. The string 
// is followed by a newline and a terminating null character, 
// conforming a total of 26 characters. 
//
char *asctime(const struct tm* tmptr)
{
	static char ascbuf[ASC_BUFF_SIZE];
    strftime(ascbuf, ASC_BUFF_SIZE, "%c\n", tmptr);
    return ascbuf;
}

#ifndef _CLOCK_T_DEFINED
typedef long clock_t;
#define _CLOCK_T_DEFINED
#endif



// Return number of clock ticks since process start.
// NOTE: This differs from standard clock since GetTickCount is the
//       number of milliseconds since system startup not process start.
//       This will also rollover after 49.7 days of continuous system
//       runtime.
//
clock_t clock(void)
{
	return GetTickCount();
}

// Convert time_t value to string in the same format as asctime.
//
char* ctime(const time_t* timer)
{
	return asctime(localtime(timer));
}

// Returns the difference in seconds between the two times.
//
double difftime(time_t timer2, time_t timer1)
{
	time_t timediff;

	if (timer2 < timer1)
	{
		timediff = (timer1 - timer2);
	}
	else
	{
		timediff = (timer2 - timer1);
	}
	return (double)timediff;
}

// Reentrant version of gmttime_ce
//
struct tm *gmtime_r(const time_t *timer, struct tm *tmbuf, BOOLEAN local)
{
  SYSTEMTIME	st;

  SecondsSince1970ToSystemTime(timer, &st, local);
  SetTz(&st);
  if(_isdst) {
    SecondsSince1970ToSystemTime(timer, &st, local);
  }

  // copy SYSTEMTIME data to tm structure
  //
  SystemTimeToTm(&st, tmbuf);

  return tmbuf;

}

// Reentrant version of localtime_ce
//
struct tm *localtime_r(const time_t *timer, struct tm *tmbuf)
{
	return gmtime_r(timer, tmbuf, TRUE);
}

// Convert a time_t value to a tm structure as UTC time. 
//
struct tm *gmtime(const time_t *timer)
{
	return gmtime_r(timer, &tmbuf, FALSE);
}

// Convert a time_t value to a tm structure as local time. 
//
struct tm *localtime(const time_t *timer)
{
	return localtime_r(timer, &tmbuf);
}

// time_t represents seconds since midnight January 1, 1970 UTC 
// (coordinated universal time) in 32-bits Win32 FILETIME structure is 64-bit,
// which represents the number of 100-nanosecond (hns) intervals since 
// January 1, 1601 UTC (coordinate universal time) the time difference
// between midnight January 1, 1970 and midnight January 1, 1601 is 11644473600 seconds
//
time_t mktime(struct tm *tptr)
{
  SYSTEMTIME st;
	int day = 0;
	int year = 0;
	int seconds = 0;
	int overflow;
	int tm_year;
	int yday, month;

  TmToSystemTime(tptr, &st);
  SetTz(&st);


	// see if seconds are < 0
	while(tptr->tm_sec < 0)
	{
		// steal 60 seconds from the minutes
		tptr->tm_sec += 60;
		tptr->tm_min--;
	}
	// roll any seconds > 60 into the minutes
	tptr->tm_min += tptr->tm_sec / 60;
	// then crop them off
	tptr->tm_sec %= 60;

	// see if minutes are < 0
	while(tptr->tm_min < 0)
	{
		// steal 60 minutes from the hours
		tptr->tm_min += 60;
		tptr->tm_hour--;
	}
	// roll any minutes > 60 into the hours
	tptr->tm_hour += tptr->tm_min / 60;
	// then crop them off
	tptr->tm_min %= 60;

	// see if hours are < 0
	while(tptr->tm_hour < 0)
	{
		// steal 24 hours from the days
		tptr->tm_hour += 24;
		day--;
	}
	// keep any "excess" days (tm doesn't have a convenient place for this)
	day += tptr->tm_hour / 24;
	// crop
	tptr->tm_hour %= 24;

	// roll any months > 12 into the years
	tptr->tm_year += tptr->tm_mon / 12;
	// then crop the off
	tptr->tm_mon %= 12;

	// see if months are < 0
	if (tptr->tm_mon < 0) 
	{
		// steal 12 months from the years
		tptr->tm_mon += 12;
		tptr->tm_year--;
	}

	// add number of days into the month to total day
	day += (tptr->tm_mday - 1);

	// if days are < 0 then calculate the number of days
	// checking to see if the month is a leap year month
	while (day < 0) 
	{
		// If months are < 0 then steal 12 months from number of years
		// for the day calculation
		if(--tptr->tm_mon < 0) 
		{
			tptr->tm_year--;
			tptr->tm_mon = 11;
		}
		day += _ytab[LEAPYEAR(YEAR0 + tptr->tm_year)][tptr->tm_mon];
	}
	// if day is greater then the number of days in the month
	// subtract the number of days in the month and adjust the 
	// month
	while (day >= _ytab[LEAPYEAR(YEAR0 + tptr->tm_year)][tptr->tm_mon]) 
	{
		day -= _ytab[LEAPYEAR(YEAR0 + tptr->tm_year)][tptr->tm_mon];
		if (++(tptr->tm_mon) == 12) 
		{
			tptr->tm_mon = 0;
			tptr->tm_year++;
		}
	}
	tptr->tm_mday = day + 1;
	year = EPOCH_YR;

	// if year is less then 1970 then return error
	if (tptr->tm_year < year - YEAR0) return (time_t) -1;

	seconds = 0;
	day = 0;                      // Means days since day 0 now
	overflow = 0;

	// Assume that when day becomes negative, there will certainly
	// be overflow on seconds.
	// The check for overflow needs not to be done for leapyears
	// divisible by 400.
	// The code only works when year (1970) is not a leapyear.
	tm_year = tptr->tm_year + YEAR0;

	// make sure we are not past the max year for 32-bit number
	if (TIME_MAX / 365 < tm_year - year) overflow++;

	// calculate number of days since EPOCH
	day = (tm_year - year) * 365;

	if (TIME_MAX - day < (tm_year - year) / 4 + 1) overflow++;

	day += (tm_year - year) / 4 + ((tm_year % 4) && tm_year % 4 < year % 4);
	day -= (tm_year - year) / 100 + ((tm_year % 100) && tm_year % 100 < year % 100);
	day += (tm_year - year) / 400 + ((tm_year % 400) && tm_year % 400 < year % 400);

	// setup for calculation of the yday or Julian day since Jan 1
	yday = month = 0;

	// add up the number of days for the preceding months
	while (month < tptr->tm_mon)
	{
		yday += _ytab[LEAPYEAR(tm_year)][month];
		month++;
	}
	// add the number of days in the current month
	yday += (tptr->tm_mday - 1);

	// make sure the didn't overflow
	if (day + yday < 0) overflow++;

	day += yday;

	// set the year day in the structure
	tptr->tm_yday = yday;

	// calculate the weekday
	tptr->tm_wday = (day + 4) % 7;               // Day 0 was thursday (4)

	// start the seconds calculation by totaling the hours, min, seconds
	seconds = ((tptr->tm_hour * 60L) + tptr->tm_min) * 60L + tptr->tm_sec;

	// make sure we are not going to overflow
	if ((TIME_MAX - seconds) / SECS_DAY < day) overflow++;

	// calculate the number of seconds for the number of days
	seconds += day * SECS_DAY;

	// Now adjust according to timezone and daylight saving time
	if (((_localtime > 0) && (TIME_MAX - _localtime < seconds))
	  || ((_localtime < 0) && (seconds < -_localtime)))
		  overflow++;

	// Adjust for local time zone
  seconds += _localtime;

	// return error if we are going to blow the max values
	if (overflow) return (time_t) -1;

	if ((time_t) seconds != seconds) return (time_t) -1;

	// return the number of seconds since EPOCH
	return (time_t) seconds;
}



// Convert Win32 FILETIME into time_t
time_t w32_filetime_to_time_t(FILETIME* ft)
{
	// make sure ft is at least ftJan1970
	if (cmp64((ULARGE_INTEGER*)ft, (ULARGE_INTEGER*)&ftJan1970) < 0)
	{
//		errno = -1;
		return -1;
	}

	// subtract ftJan1970 from ft
	sub64((ULARGE_INTEGER*)ft, (ULARGE_INTEGER*)&ftJan1970);

	// divide ft by 10,000,000 to convert from 100-nanosecond units to seconds
	div64((ULARGE_INTEGER*)ft, 10000000);

	// bound check result
	if (ft->dwHighDateTime != 0 || ft->dwLowDateTime >= 2147483648)
	{
//		errno = -1;
		return -1;		// value is too big to return in time_t
	}

	return (time_t)ft->dwLowDateTime;
}




// Get the current system time and convert to seconds since
// 1/1/1970.  Store the seconds value in tloc if not a NULL pointer then
// return the seconds value.
//
time_t time(time_t *tloc)
{
	SYSTEMTIME	st;
	time_t	secs = 0;
	
	// Get current system time
	GetSystemTime(&st);

  // Set time zone information
	//
	SetTz(&st);

	// convert system time to number of seconds since 1970
	//
	/* leg: note: time_t is sometimes unsigned long, and other
	 * times long long, so we need to cast */
	secs = (time_t)SystemTimeToSecondsSince1970(&st);

	// check for failure
	//
	if(secs == TIME_FAILURE)
	{
		return TIME_FAILURE;
	}

	// If tloc is not NULL, the return value is also stored in the location to which tloc points
	//
	if(tloc != NULL)
	{
#if 0
		if(IsBadWritePtr(tloc, sizeof(time_t)))
		{
			return TIME_FAILURE;
		}
#endif
		memcpy(tloc, &secs, sizeof(time_t));
	}
	
	return secs;
}

// The strftime function is a modified version created by the following:
// written 6 september 1989 by jim nutt
// released into the public domain by jim nutt
//
// modified 21-Oct-89 by Rob Duff
//
//
// size_t strftime(char *str,
//                 size_t maxs,
//                 const char *fmt,
//                 const struct tm *t)
// 
//      this functions acts much like a sprintf for time/date output.
//      given a pointer to an output buffer, a format string and a
//      time, it copies the time to the output buffer formatted in
//      accordance with the format string.  the parameters are used
//      as follows:
// 
//          str is a pointer to the output buffer, there should
//          be at least maxs characters available at the address
//          pointed to by str.
// 
//          maxs is the maximum number of characters to be copied
//          into the output buffer, included the '\0' terminator
// 
//          fmt is the format string.  a percent sign (%) is used
//          to indicate that the following character is a special
//          format character.  the following are valid format
//          characters:
// 
//              %A      full weekday name (Monday)
//              %a      abbreviated weekday name (Mon)
//              %B      full month name (January)
//              %b      abbreviated month name (Jan)
//              %c      standard date and time representation
//              %d      day-of-month (01-31)
//              %H      hour (24 hour clock) (00-23)
//              %I      hour (12 hour clock) (01-12)
//              %j      day-of-year (001-366)
//              %M      minute (00-59)
//              %m      month (01-12)
//              %p      local equivalent of AM or PM
//              %S      second (00-59)
//              %U      week-of-year, first day sunday (00-53)
//              %W      week-of-year, first day monday (00-53)
//              %w      weekday (0-6, sunday is 0)
//              %X      standard time representation
//              %x      standard date representation
//              %Y      year with century
//              %y      year without century (00-99)
//              %Z      timezone name
//              %%      percent sign
//
//      the standard date string is equivalent to:
//
//          %a %b %d %Y
//
//      the standard time string is equivalent to:
//
//          %H:%M:%S
//
//      the standard date and time string is equivalent to:
//
//          %a %b %d %H:%M:%S %Y
//
//      strftime returns the number of characters placed in the
//      buffer, not including the terminating \0, or zero if more
//      than maxs characters were produced.
//
size_t strftime(char *s, size_t maxs, const char *f, const struct tm *t)
{
	int			w;
	char		*p, *q, *r;
	static char	buf[26];

	p = s;
	q = s + maxs - 1;
	while ((*f != '\0'))
	{
		if (*f++ == '%')
		{
			r = buf;
			switch (*f++)
			{
				case '%' :
					r = "%";
					break;

				case 'a' :
					r = aday[t->tm_wday];
					break;

				case 'A' :
					r = day[t->tm_wday];
					break;

				case 'b' :
					r = amonth[t->tm_mon];
					break;

				case 'B' :
					r = month[t->tm_mon];
					break;

				case 'c' :
					strfmt(r, "%0 %0 %2 %2:%2:%2 %4",
						  aday[t->tm_wday], amonth[t->tm_mon],
						  t->tm_mday,t->tm_hour, t->tm_min,
						  t->tm_sec, t->tm_year+1900);
					break;

				case 'd' :
					strfmt(r,"%2",t->tm_mday);
					break;

				case 'H' :
					strfmt(r,"%2",t->tm_hour);
					break;

				case 'I' :
					strfmt(r,"%2",(t->tm_hour%12)?t->tm_hour%12:12);
					break;

				case 'j' :
					strfmt(r,"%3",t->tm_yday+1);
					break;

				case 'm' :
					strfmt(r,"%2",t->tm_mon+1);
					break;

				case 'M' :
					strfmt(r,"%2",t->tm_min);
					break;

				case 'p' :
					r = (t->tm_hour>11)?"PM":"AM";
					break;

				case 'S' :
					strfmt(r,"%2",t->tm_sec);
					break;

				case 'U' :
					w = t->tm_yday/7;
					if (t->tm_yday%7 > t->tm_wday)
						w++;
					strfmt(r, "%2", w);
					break;

				case 'W' :
					w = (t->tm_yday + DAYSPERWEEK -
						(t->tm_wday ?
						(t->tm_wday - 1) :
						(DAYSPERWEEK - 1))) / DAYSPERWEEK;
					strfmt(r, "%2", w);
					break;

				case 'w' :
					strfmt(r,"%1",t->tm_wday);
					break;

				case 'x' :
					strfmt(r, "%2/%2/%2", t->tm_mon + 1,
						   t->tm_mday, t->tm_year+1900);
					break;

				case 'X' :
					strfmt(r, "%2:%2:%2", t->tm_hour,
						   t->tm_min, t->tm_sec);
					break;

				case 'y' :
					strfmt(r,"%2",t->tm_year%100);
					break;

				case 'Y' :
					strfmt(r,"%4",t->tm_year+1900);
					break;

				case 'Z' :
					r = (t->tm_isdst && tz_name[1][0])?tz_name[1]:tz_name[0];
					break;

				default:
					buf[0] = '%';		// reconstruct the format
					buf[1] = f[-1];
					buf[2] = '\0';
					if (buf[1] == 0)
						f--;			// back up if at end of string
			}
			while (*r)
			{
				if (p == q)
				{
					*q = '\0';
					return 0;
				}
				*p++ = *r++;
			}
		}
		else
		{
			if (p == q)
			{
				*q = '\0';
				return 0;
			}
			*p++ = f[-1];
		}
	}
	*p = '\0';
	return p - s;
}

//
// static void strfmt(char *str, char *fmt);
// 
// simple sprintf for strftime
// 
// each format descriptor is of the form %n
// where n goes from zero to four
// 
// 0    -- string %s
// 1..4 -- int %?.?d
// 
static void strfmt(char *str, const char *fmt, ...)
{
	int ival, ilen;
	char *sval;
	static int pow[5] = { 1, 10, 100, 1000, 10000 };
	va_list vp;

	va_start(vp, fmt);
	while (*fmt)
	{
		if (*fmt++ == '%')
		{
			ilen = *fmt++ - '0';
			if (ilen == 0)                // zero means string arg
			{
				sval = va_arg(vp, char*);
				while (*sval)
					*str++ = *sval++;
			}
			else                          // always leading zeros
			{
				ival = va_arg(vp, int);
				while (ilen)
				{
					ival %= pow[ilen--];
					*str++ = (char)('0' + ival / pow[ilen]);
				}
			}
		}
		else  *str++ = fmt[-1];
	}
	*str = '\0';
	va_end(vp);
}

// internal functions
//-----------------------------------------------------------------------------
// Convert 100ns units since 1601 to seconds since 1970
//
LONGLONG SystemTimeToSecondsSince1970(SYSTEMTIME *st)
{
	ULARGE_INTEGER	uli;
	FILETIME		ft;
	ULARGE_INTEGER	uli1970;
	FILETIME		ft1970;

	// convert to a FILETIME
	// Gives number of 100-nanosecond intervals since January 1, 1601 (UTC)
	//
	if(!SystemTimeToFileTime(st, &ft))
	{
		return TIME_FAILURE;
	}

	// convert to a FILETIME
	// Gives number of 100-nanosecond intervals since January 1, 1970 (UTC)
	//
	if(!SystemTimeToFileTime(&st1970, &ft1970))
	{
		return TIME_FAILURE;
	}

	// Copy file time structures into ularge integer so we can do
	// the math more easily
	//
	memcpy(&uli, &ft, sizeof(uli));
	memcpy(&uli1970, &ft1970, sizeof(uli1970));

	// Subtract the 1970 number of 100 ns value from the 1601 100 ns value
	// so we can get the number of 100 ns value between 1970 and now
	// then devide be 10,000,000 to get the number of seconds since 1970
	//
	uli.QuadPart = ((uli.QuadPart - uli1970.QuadPart) / 10000000);

	return (LONGLONG)uli.QuadPart;
}

// Convert seconds since 1970 to a file time in 100ns units since 1601
// then to a system time.
//
void SecondsSince1970ToSystemTime(const time_t * timer, SYSTEMTIME * st, 
								  BOOLEAN local)
{
	ULARGE_INTEGER	uli;
	FILETIME		ft;
	ULARGE_INTEGER	uli1970;
	FILETIME		ft1970;

	// Convert system time to file time
	//
	if(!SystemTimeToFileTime(&st1970, &ft1970))
	{
		st = NULL;
		return;
	}

	// convert hundreds of nanosecs to secs: 1 sec = 1e7 100ns
	// Gives number of seconds since 1/1/1601 (UTC)
	//
  memcpy(&uli, timer, sizeof(uli));
  memcpy(&uli1970, &ft1970, sizeof(uli1970));

	// If we want local time then subtract the number of seconds between
	// UTC and current local time
	//
	if (local)
	{
		// Calculate 100ns since 1/1/1601 local time
		//
    uli.QuadPart = (((uli.QuadPart - _localtime)*10000000) + uli1970.QuadPart);
	}
	else
	{
		// Calculate 100ns since 1/1/1601 UTC
		//
		uli.QuadPart = ((uli.QuadPart)*10000000 + uli1970.QuadPart);
	}

	// copy data back into the ft
	//
	memcpy(&ft, &uli, sizeof(uli));

	// convert to a SYSTEMTIME
	//
	if(!FileTimeToSystemTime(&ft, st))
	{
		st = NULL;
		return;
	}

	return;
}

// Set the time zone information needed for the rest of the methods
//
void SetTz(SYSTEMTIME *_st)
{
	TIME_ZONE_INFORMATION	tz;
	SYSTEMTIME				st = *_st;
	FILETIME				ftDT; // Daylight savings file time
	FILETIME				ftST; // Standard time file time
	FILETIME				ft;   // file time to compare
	int						i;

	GetTimeZoneInformation(&tz);

	// Convert current system time, daylight savings changover time
	// and standard time to file time to see if current time is between
	// the two dates.  If so then we are in daylight savings otherwise we
	// are not.
	//
	SystemTimeToFileTime(&st, &ft);
	tz.DaylightDate.wYear = st.wYear;
	tz.StandardDate.wYear = st.wYear;
  SystemTimeToFileTime(&tz.DaylightDate, &ftDT);
	SystemTimeToFileTime(&tz.StandardDate, &ftST);

  // -1 First file time is earlier than second file time. 
  //  0 First file time is equal to second file time. 
  //  1 First file time is later than second file time. 
	//
	if ((CompareFileTime(&ft,&ftDT) >= 0) && (CompareFileTime(&ft,&ftST) <= 0) )
	{
		_isdst = TRUE;
	}
	else
	{
		_isdst = FALSE;
	}

	// Set localtime difference in seconds from UTC
	//
	if (_isdst)
	{
		// Adjust for Daylight Savings Time and convert to seconds
		//
		_localtime = (tz.Bias + tz.DaylightBias) * SECS_IN_MIN;
	}
	else
	{
		// Convert to seconds
		//
		_localtime = tz.Bias * SECS_IN_MIN;
	}

	_dstBias = tz.DaylightBias;

	// Set the standard and daylight strings
	//
	for (i=0;i<32;i++)
	{
		tz_name[0][i] = (char)tz.StandardName[i];
		tz_name[1][i] = (char)tz.DaylightName[i];
	}
}

// Copy system time structure to tm structure
//
void SystemTimeToTm(SYSTEMTIME *st, struct tm * tmbuffer)
{
	tmbuffer->tm_hour = st->wHour;
    tmbuffer->tm_mday = st->wDay;
    tmbuffer->tm_min = st->wMinute;
    tmbuffer->tm_mon = st->wMonth - 1;
    tmbuffer->tm_sec = st->wSecond;
    tmbuffer->tm_wday = st->wDayOfWeek;
    tmbuffer->tm_yday = JulianDays(st);		// Julian days, numer of days since Jan 1
	tmbuffer->tm_year = st->wYear - 1900;
	tmbuffer->tm_isdst = _isdst;			// Is Daylight Savings Time
}

void TmToSystemTime(struct tm * tmbuffer, SYSTEMTIME *st)
{
  st->wHour = tmbuffer->tm_hour;
  st->wDay = tmbuffer->tm_mday;
  st->wMinute = tmbuffer->tm_min;
  st->wMonth = tmbuffer->tm_mon + 1;
  st->wSecond = tmbuffer->tm_sec;
  st->wDayOfWeek = tmbuffer->tm_wday;
  st->wYear = tmbuffer->tm_year + 1900;
  st->wMilliseconds = 0;
}

// Get the JulianDay from a Gregorian Date for number of days into the current
// year
// Algorithm from: http://www.vsg.cape.com/~pbaum/date/jdalg2.htm
//
DWORD JulianDays(SYSTEMTIME * st)
{
	int m = 0;
	int y = 0;
	double jdd = 0;
	double jddYearStart = 0;

	// Calculate the Julian day for the beginning of the year
	//
    m = 13;
	y = st->wYear - 1;
    jddYearStart = 1 + (153 * m - 457) / 5 + 365 * y + (y / 4) - (y / 100) + (y / 400) + 1721118.5;

	// Calculate Julian Day for Current Date
	//
	if (st->wMonth >= 3)
	{
		m = st->wMonth;
		y = st->wYear;
	}
    jdd = st->wDay + (153 * m - 457) / 5 + 365 * y + (y / 4) - (y / 100) + (y / 400) + 1721118.5;

	// Subract the year start Julian date from the Current Julian date to get
	// the number of Julian days from the year start
	//
	return (DWORD)(jdd - jddYearStart);
}
