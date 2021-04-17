/*
 * temperlib contains all functions for tempersensor that
 * can be tested automatically.
 * Additional infos (including a license notice) are at the end of this file.
 */

/*
 * setup based on input of:
 * https://stackoverflow.com/questions/65820/unit-testing-c-code
 * https://libcheck.github.io/check/doc/check_html/check_3.html
 */

#include <check.h>
#include <stdlib.h>
#include "../src/temperlib.h"

START_TEST(extend_errormessage_someerror)
{
    char *fullerr = NULL;
    char testerr[10];

    strcpy(testerr, "Testerror");
    fullerr = extend_errormessage(testerr, 1);

    ck_assert_str_eq(testerr, "Testerror");
    ck_assert_str_eq(fullerr, "Testerror: Operation not permitted");
    free(fullerr);
}
END_TEST

START_TEST(extend_errormessage_noerror)
{
    char *fullerr = NULL;
    char testerr[10];

    strcpy(testerr, "Testerror");
    fullerr = extend_errormessage(testerr, 0);

    ck_assert_str_eq(testerr, "Testerror");
    ck_assert_str_eq(fullerr, "Testerror: Success");
    free(fullerr);
}
END_TEST

Suite *temperlib_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("temperlib");

    tc_core = tcase_create("temperlib");

    tcase_add_test(tc_core, extend_errormessage_noerror);
    tcase_add_test(tc_core, extend_errormessage_someerror);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = temperlib_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

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

