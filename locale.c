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

#include <windows.h>
#include <locale.h>

#include "crt.h"

#if !defined (PLC_WINPAC_9000)
static struct lconv _localeconv={0};
#endif

/* convertLocaleInfo(): helper function
 *
 * given an LCTYPE constant, get the respective locale value, convert it
 * to UTF8 and store it into dest.
 * The char buf for the destination is allocated dynamically.
 * If the destination char *pointer is not NULL, it is free()d previously
 *
 * Implementation note: this is done out of laziness since i didn't want to find
 * out the individual string size of a specific locale field
 *
 * If an error occurs, NULL is stored to the destination pointer
 */
void convertLocaleInfo(int lctype,char **dest)
{
	int lenW, len;
	wchar_t *t;

	if(!*dest) {
		free(*dest);
		*dest=NULL;
	}

	lenW = GetLocaleInfoW(LOCALE_USER_DEFAULT, lctype, NULL, 0);

	if(!lenW)
        return;

	if(!(t=(wchar_t *)calloc(lenW, sizeof(wchar_t))))
        return;

	if (!GetLocaleInfoW(LOCALE_USER_DEFAULT, lctype, t, lenW))
		goto exit_fail;

	len = WideCharToMultiByte(CP_UTF8, 0, t, lenW, NULL, 0, NULL, NULL);

    if(!(*dest=(char *)calloc(len, sizeof(wchar_t))))
		goto exit_fail;

	WideCharToMultiByte(CP_UTF8, 0, t, lenW, *dest, len, NULL, NULL);

exit_fail:
	free(t);
}

/*
see: http://www.chemie.fu-berlin.de/chemnet/use/info/libc/libc_19.html
and: http://msdn.microsoft.com/en-us/library/aa912934.aspx
*/

struct lconv *localeconv( void )
{
	char *t;
	convertLocaleInfo(LOCALE_SCURRENCY,&_localeconv.currency_symbol);
	convertLocaleInfo(LOCALE_SDECIMAL,&_localeconv.decimal_point);
	convertLocaleInfo(LOCALE_ICURRDIGITS,&t);
	_localeconv.frac_digits=atoi(t);
	convertLocaleInfo(LOCALE_SGROUPING,&_localeconv.grouping);
	convertLocaleInfo(LOCALE_SINTLSYMBOL,&_localeconv.int_curr_symbol);
	convertLocaleInfo(LOCALE_IINTLCURRDIGITS,&t);
	_localeconv.int_frac_digits=atoi(t);
	convertLocaleInfo(LOCALE_SMONDECIMALSEP,&_localeconv.mon_decimal_point);
	convertLocaleInfo(LOCALE_SMONGROUPING,&_localeconv.mon_grouping);
	convertLocaleInfo(LOCALE_SMONTHOUSANDSEP,&_localeconv.mon_thousands_sep);
	convertLocaleInfo(LOCALE_ICURRENCY,&t);
	switch(*t) {
	case '0':
		_localeconv.p_cs_precedes=1;
		_localeconv.p_sep_by_space=0;
		break;
	case '1':
		_localeconv.p_cs_precedes=0;
		_localeconv.p_sep_by_space=0;
		break;
	case '2':
		_localeconv.p_cs_precedes=1;
		_localeconv.p_sep_by_space=1;
		break;
	case '3':
		_localeconv.p_cs_precedes=0;
		_localeconv.p_sep_by_space=1;
	}
	convertLocaleInfo(LOCALE_INEGCURR,&t);
	switch(atoi(t)) {
	case 0: case 1: case 2: case 3:
		_localeconv.n_cs_precedes=1;
 		_localeconv.n_sep_by_space=0;
		break;
	case 4: case 5: case 6: case 7:
		_localeconv.n_cs_precedes=0;
 		_localeconv.n_sep_by_space=0;
		break;
	case 8: case 10: case 13: case 15:
		_localeconv.n_cs_precedes=0;
 		_localeconv.n_sep_by_space=1;
		break;
	case 9: case 11: case 12: case 14:
		_localeconv.n_cs_precedes=1;
 		_localeconv.n_sep_by_space=1;
		break;
	}
	convertLocaleInfo(LOCALE_INEGSIGNPOSN,&t);
	_localeconv.n_sign_posn=atoi(t);
	convertLocaleInfo(LOCALE_SNEGATIVESIGN,&_localeconv.negative_sign);
	convertLocaleInfo(LOCALE_IPOSSIGNPOSN,&t);
	_localeconv.p_sign_posn=atoi(t);
	convertLocaleInfo(LOCALE_SPOSITIVESIGN,&_localeconv.positive_sign);
	convertLocaleInfo(LOCALE_STHOUSAND,&_localeconv.thousands_sep);
	return &_localeconv;
}

/* convert a locale identifier into the respective
 * locale string.
 * Workaround: just work for a small amount of locales
 *
 * TODO: could be implemented with GetLocaleInfo()
 *
 */
char *LCIDTOSTRING(LCID lcid)
{
	switch(lcid) {
		case 0: return "C";
		case 0x0407: return "German";
		case 0x0409: return "English";
		default: return NULL;
	}
}
/* convert a locale string to the locale identifier used by
 * Windows functions.
 * Workaround: just work for a small amount of locales
 *
 * TODO: find out how to do this seriously
 */
LCID STRINGTOLCID(const char *locale)
{
	if(!strcmp("C",locale)) return 0;
	if(!strcmp("POSIX",locale)) return 0;
	if(!strcmp("german",locale)) return 0x407;
	if(!strcmp("english",locale)) return 0x409;
	return 0;
}
/* setlocale():
 * Windows does not support the concept of several categories, so
 * we only allow LC_ALL.  Individual locale settings could be modified
 * though with (several calls to) SetLocaleInfo(LCID,LCTYPE,data).
 *
 * posix only requires "C" == "POSIX" => , 
 */
char *setlocale(int category, const char *locale)
{

	if(category!=LC_ALL) return NULL;

	if(!locale) /* read locale */
		return(LCIDTOSTRING(GetUserDefaultLCID()));

	if(!*locale) { /* init: setlocale(LC_ALL,"") */
		if(!SetUserDefaultLCID(0x0400)) return NULL;
		else return (char *)locale;
	}

	if(!SetUserDefaultLCID(STRINGTOLCID(locale))) return NULL;

	return (char *)locale;
}
