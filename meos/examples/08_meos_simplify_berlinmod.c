/*****************************************************************************
 *
 * This MobilityDB code is provided under The PostgreSQL License.
 * Copyright (c) 2016-2023, Université libre de Bruxelles and MobilityDB
 * contributors
 *
 * MobilityDB includes portions of PostGIS version 3 source code released
 * under the GNU General Public License (GPLv2 or later).
 * Copyright (c) 2001-2023, PostGIS contributors
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose, without fee, and without a written
 * agreement is hereby granted, provided that the above copyright notice and
 * this paragraph and the following two paragraphs appear in all copies.
 *
 * IN NO EVENT SHALL UNIVERSITE LIBRE DE BRUXELLES BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING
 * LOST PROFITS, ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION,
 * EVEN IF UNIVERSITE LIBRE DE BRUXELLES HAS BEEN ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * UNIVERSITE LIBRE DE BRUXELLES SPECIFICALLY DISCLAIMS ANY WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE. THE SOFTWARE PROVIDED HEREUNDER IS ON
 * AN "AS IS" BASIS, AND UNIVERSITE LIBRE DE BRUXELLES HAS NO OBLIGATIONS TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS. 
 *
 *****************************************************************************/

/**
 * @brief A simple program that reads from a CSV file synthetic trip data in
 * Brussels generated by the MobilityDB-BerlinMOD generator
 * https://github.com/MobilityDB/MobilityDB-BerlinMOD
 * simplifies the trips using both Douglas-Peucker (DP) and Synchronized
 * Euclidean Distance (SED, also known as Top-Down Time Ratio simplification),
 * and outputs for each trip the initial number of instants and the number of
 * instants of the two simplified trips.
 *
 * Please read the assumptions made about the input file `trips.csv` in the
 * file `meos_disassemble_berlinmod.c` in the same directory.
 *
 * The program can be build as follows
 * @code
 * gcc -Wall -g -I/usr/local/include -o 08_meos_simplify_berlinmod 08_meos_simplify_berlinmod.c -L/usr/local/lib -lmeos
 * @endcode
 */

#include <stdio.h>
#include <stdlib.h>
#include <meos.h>

/* Maximum length in characters of a trip in the input data */
#define MAX_LENGTH_TRIP 160000
/* Maximum length in characters of a header record in the input CSV file */
#define MAX_LENGTH_HEADER 1024
/* Maximum length in characters of a date in the input data */
#define MAX_LENGTH_DATE 12
/* Epsilon distance used for the simplification */
#define DELTA_DISTANCE 2
/* Maximum number of trips */
#define MAX_NO_TRIPS 64

typedef struct
{
  int tripId;
  int vehId;
  DateADT day;
  int seq;
  Temporal *trip;
} trip_record;

/* Main program */
int main(void)
{
  /* Variables to read the input CSV file */
  char trip_buffer[MAX_LENGTH_TRIP];
  char header_buffer[MAX_LENGTH_HEADER];
  char date_buffer[MAX_LENGTH_DATE];
  /* Allocate space for the trips and their simplified versions */
  trip_record trips[MAX_NO_TRIPS];
  Temporal *trips_dp[MAX_NO_TRIPS];
  Temporal *trips_sed[MAX_NO_TRIPS];
  /* Number of records and records with null values */
  int no_records = 0;
  int nulls = 0;
  /* Iterator variables */
  int i = 0, j;

  /* Initialize MEOS */
  meos_initialize(NULL);

  /* Substitute the full file path in the first argument of fopen */
  FILE *file = fopen("trips.csv", "r");

  if (! file)
  {
    printf("Error opening file\n");
    return 1;
  }

  /* Read the first line of the file with the headers */
  fscanf(file, "%1023s\n", header_buffer);

  /* Continue reading the file */
  i = 0;
  do
  {
    int read = fscanf(file, "%d,%d,%10[^,],%d,%160000[^\n]\n",
      &trips[i].tripId, &trips[i].vehId, date_buffer, &trips[i].seq,
        trip_buffer);
    /* Transform the string representing the date into a date value */
    trips[i].day = pg_date_in(date_buffer);
    /* Transform the string representing the trip into a temporal value */
    trips[i].trip = temporal_from_hexwkb(trip_buffer);

    if (read == 5)
      i++;

    if (read != 5 && !feof(file))
    {
      printf("Record with missing values ignored\n");
      nulls++;
    }

    if (ferror(file))
    {
      printf("Error reading file\n");
      fclose(file);
      /* Free memory */
      for (j = 0; j < i; j++)
        free(trips[j].trip);
      return 1;
    }
  } while (!feof(file));

  no_records = i;
  printf("\n%d records read.\n%d incomplete records ignored.\n",
    no_records, nulls);

  /* Simplify the trips */
  for (i = 0; i < no_records; i++)
  {
    trips_dp[i] = temporal_simplify(trips[i].trip, DELTA_DISTANCE, false);
    trips_sed[i] = temporal_simplify(trips[i].trip, DELTA_DISTANCE, true);
    char *day_str = pg_date_out(trips[i].day);
    printf("Vehicle: %d, Date: %s, Seq: %d, No. of instants: %d, "
      "No. of instants DP: %d, No. of instants SED: %d\n",
      trips[i].vehId, day_str, trips[i].seq,
      temporal_num_instants(trips[i].trip),
      temporal_num_instants(trips_dp[i]),
      temporal_num_instants(trips_sed[i]));
    free(day_str);
  }

  /* Free memory */
  for (i = 0; i < no_records; i++)
  {
    free(trips[i].trip);
    free(trips_dp[i]);
    free(trips_sed[i]);
  }

  /* Close the file */
  fclose(file);

  /* Finalize MEOS */
  meos_finalize();

  return 0;
}
