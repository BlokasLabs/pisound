/*
 * test.c
 *
 * program to test seconds calculations to verify correctness.
 *
 * Prints out a table of calculated vs expected values.  table will include an "X" in each cell that has an error.
 *
 * If there are errors, print out the calculations that have errors.
 *
 *
 *  Created on: 31 Aug 2021
 *      Author: claude
 */

#include <stdarg.h>
#include <stdio.h>

static int g_full_time;
static int g_offset_time;

char* lbls[] = { "f-o-", "f-o+", "f+o-", "f+o+" };


static int seconds( int ticks ) {

	int result;

	if (g_offset_time)  {
		if (g_full_time) {  // f+o+
			result = (ticks+500)/1000;

		} else {		// f-o+
			int sec = (ticks)/1000;
			result = sec+((sec+1)%2);
		}
	} else {
		if (g_full_time) { // f+o-
			result = ticks/1000;
		} else {		// f-o-
			result = 1+( ((ticks-1000)/2000)* 2);
		}
	}
	return result;

}

static int assert( int x, char* stmt) {
	if (!x) {
		printf( "%s\n", stmt );
		return 1;
	}
	return 0;
}

int main(int argc, char **argv, char **envp)
{

	/* A list of seconds and expected values for each calculation type.
	 * Seconds were chosen based on transition points in the calculations.
	 */
	// 		sec,		f-o-	f-o+	f+o-	f+o+
	int tests[][5] = {
			{400,		1,		1,		0,		0},
			{499,		1,		1,		0,		0},
			{500,		1,		1,		0,		1},
			{999,		1,		1,		0,		1},
			{1000,		1,		1,		1,		1},
			{1499,		1,		1,		1,		1},
			{1500,		1,		1,		1,		2},
			{1999,		1,		1,		1,		2},
			{2000,		1,		3,		2,		2},
			{2499,		1,		3,		2,		2},
			{2500,		1,		3,		2,		3},
			{2999,		1,		3,		2,		3},
			{3000,		3,		3,		3,		3},
			{3499,		3,		3,		3,		3},
			{3500,		3,		3,		3,		4},
			{3999,		3,		3,		3,		4},
			{4000,		3,		5,		4,		4},
			{4499,		3,		5,		4,		4},
			{4500,		3,		5,		4,		5},
			{4999,		3,		5,		4,		5},
			{5000,		5,		5,		5,		5},
			{5499,		5,		5,		5,		5},
			{5500,		5,		5,		5,		6},
			{5999,		5,		5,		5,		6},
			{6000,		5,		7,		6,		6},
			{6499,		5,		7,		6,		6},
			{6500,		5,		7,		6,		7},
			{6999,		5,		7,		6,		7},
			{7000,		7,		7,		7,		7},
			{7499,		7,		7,		7,		7},
			{7500,		7,		7,		7,		8},
			{7999,		7,		7,		7,		8},
			{8000,		7,		9,		8,		8}
	};
	int test_count = sizeof(tests)/sizeof(tests[0]);

	printf( "   t |  %s  |  %s  |  %s  |  %s \n", lbls[0], lbls[1], lbls[2], lbls[3] );
	int err = 0;
	for (int i=0;i<test_count;i++)
	{
		int t = tests[i][0];
		printf( "%4d |", t );
		for (int  l=0;l<4;l++)
		{
			g_full_time = l & 0x2;
			g_offset_time = l & 0x1;
			int result = seconds(t);
			int expected = tests[i][1+l];
			err |= result!=tests[i][1+l];
			printf( " %i %c %i  |", result, result==expected?' ':'X', expected );
		}
		printf( "\n" );
	}


	// If there was an error print out detail.
	if (err) {
		char sout[250];

		for (int i=0;i<test_count;i++)
		{
			for (int  l=0;l<4;l++)
			{
				g_full_time = l & 0x2;
				g_offset_time = l & 0x1;
				int t = tests[i][0];
				int expected = tests[i][1+l];
				int result = seconds(t);
				sprintf( sout, "%s %i yields %i not %i (%i %i)", lbls[l], t, result, expected, g_full_time, g_offset_time );
				assert( result==expected, sout );
			}
		}

	}


}

