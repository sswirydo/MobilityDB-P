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
 * splits the trips and the speed of these trips by tiles, and computes
 * aggregate values of the count, duration, and distance of the tiles.
 *
 * Please read the assumptions made about the input file `trips.csv` in the
 * file `meos_disassemble_berlinmod.c` in the same directory. Notice that the
 * program assumes that we already have computed the extent of the trips and
 * the speeds. Otherwise, a first scan of the CSV file is needed for computing
 * both extents and then proceed for the second scan as below.
 *
 * The program can be build as follows
 * @code
 * gcc -Wall -g -I/usr/local/include -o 07_meos_tile_berlinmod 07_meos_tile_berlinmod.c -L/usr/local/lib -lmeos
 * @endcode
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <meos.h>

/* Maximum length in characters of a trip in the input data */
#define MAX_LENGTH_TRIP 160000
/* Maximum length in characters of a header record in the input CSV file */
#define MAX_LENGTH_HEADER 1024
/* Maximum length in characters of a date in the input data */
#define MAX_LENGTH_DATE 12
/* Maximum number of buckets in each dimension */
#define MAX_NO_BUCKETS 10

typedef struct
{
  int count;
  Interval duration;
  double distance;
} trip_record;

typedef struct
{
  int count;
  Interval duration;
} speed_record;

/* Main program */
int main(void)
{
  /* Variables to read the input CSV file */
  char trip_buffer[MAX_LENGTH_TRIP];
  char header_buffer[MAX_LENGTH_HEADER];
  char date_buffer[MAX_LENGTH_DATE];

  /* Number of records */
  int no_records = 0;
  int no_nulls = 0;
  /* Iterator variables */
  int i, j, k;

  /* Initialize MEOS */
  meos_initialize(NULL);

  /* Compute the spatial tiles for trips */
  int no_rows, no_cols, *no_cells;
  STBox *trip_extent =
    stbox_in("SRID=3857;STBOX X(((473212.810151,6578740.528027),(499152.544688,6607165.513683)))");
  GSERIALIZED *sorigin = gserialized_in("Point(0 0)", -1);
  STBox *trip_tiles = stbox_tile_list(trip_extent, 5e3, NULL, sorigin, 0,
    &no_cells);
  /* Compute the (value and time) tiles for speed of trips */
  TBox *speed_extent = tbox_in("TBox XT([0, 35),[2020-06-01, 2020-06-05))");
  Interval *duration = pg_interval_in("1 day", -1);
  TimestampTz torigin = pg_timestamptz_in("2020-06-01", -1);
  TBox *speed_tiles = tbox_tile_list(speed_extent, 10.0, duration, 0.0, torigin,
    &no_rows, &no_cols);
  /* Variables for tiling the trips and their speeds */
  int no_tiles = no_cells[0] * no_cells[1]; /* We only have spatial tiles */
  trip_record *trip_splits = malloc(sizeof(trip_record) * no_tiles);
  memset(trip_splits, 0, sizeof(trip_record) * no_tiles);
  speed_record *speed_splits = malloc(sizeof(speed_record) * no_rows * no_cols);
  memset(speed_splits, 0, sizeof(speed_record) * no_rows * no_cols);

  /* Substitute the full file path in the first argument of fopen */
  FILE *file = fopen("trips.csv", "r");

  if (! file)
  {
    printf("Error opening file\n");
    free(trip_tiles);
    free(trip_splits);
    free(speed_tiles);
    free(speed_splits);
    return 1;
  }

  /* Read the first line of the file with the headers */
  fscanf(file, "%1023s\n", header_buffer);
  printf("Processing records\n");

  /* Continue reading the file */
  do
  {
    int tripId, vehId, seq;
    int read = fscanf(file, "%d,%d,%10[^,],%d,%160000[^\n]\n",
      &tripId, &vehId, date_buffer, &seq, trip_buffer);
    /* Transform the string representing the trip into a temporal value */
    Temporal *trip = temporal_from_hexwkb(trip_buffer);
    Temporal *speed = tpoint_speed(trip);
    Temporal *split;
    Interval *dur1, *dur2;

    if (read == 5)
    {
      no_records++;
      printf("*");
      fflush(stdout);
    }

    if (read != 5 && !feof(file))
    {
      printf("Record with missing values ignored\n");
      no_nulls++;
    }

    if (ferror(file))
    {
      printf("Error reading file\n");
      fclose(file);
      free(trip_tiles);
      free(trip_splits);
      free(speed_tiles);
      free(speed_splits);
      return 1;
    }

    /* Split the trip by the tiles and accumulate aggregate values */
    k = 0;
    for (i = 0; i < no_cells[0] * no_cells[1]; i++)
    {
      split = tpoint_at_stbox(trip, &trip_tiles[k]);
      if (split != NULL)
      {
        trip_splits[k].count++;
        trip_splits[k].distance += tpoint_length(split) / 1e3;
        dur1 = temporal_duration(split);
        dur2 = pg_interval_pl(dur1, &trip_splits[k].duration);
        memcpy(&trip_splits[k].duration, dur2, sizeof(Interval));
        free(split); free(dur1); free(dur2);
      }
      k++;
    }

    /* Split the temporal speed by the tiles and accumulate aggregate values */
    k = 0;
    for (i = 0; i < no_rows * no_cols; i++)
    {
      split = tnumber_at_tbox(speed, &speed_tiles[k]);
      if (split != NULL)
      {
        speed_splits[k].count++;
        dur1 = temporal_duration(split);
        dur2 = pg_interval_pl(dur1, &speed_splits[k].duration);
        memcpy(&speed_splits[k].duration, dur2, sizeof(Interval));
        free(split); free(dur1); free(dur2);
      }
      k++;
    }

    free(trip);
    free(speed);

  } while (!feof(file));

  printf("\n%d records read.\n%d incomplete records ignored.\n",
    no_records, no_nulls);

  /* Print results */
  printf("-------------\n");
  printf(" Trip tiles \n");
  printf("-------------\n");
  k = 0;
  Interval intervalzero;
  memset(&intervalzero, 0, sizeof(Interval));
  for (i = 0; i < no_cells[0]; i++)
  {
    for (j = 0; j < no_cells[1]; j++)
    {
      if (pg_interval_cmp(&trip_splits[k].duration, &intervalzero) != 0)
      {
        char *stbox_str = stbox_out(&trip_tiles[k], 0);
        char *interval_str = pg_interval_out(&trip_splits[k].duration);
        printf("Tile: (%d,%d), Box: %s, Count: %d, Duration: %s, Distance: %f\n",
          i, j, stbox_str, trip_splits[k].count, interval_str,
          trip_splits[k].distance);
        free(stbox_str);
      }
      k++;
    }
  }
  printf("\n");

  /* Print results */
  printf("-------------\n");
  printf(" Speed tiles \n");
  printf("-------------\n");
  k = 0;
  for (i = 0; i < no_rows; i++)
  {
    for (j = 0; j < no_cols; j++)
    {
      char *span_str = span_out(&speed_tiles[k].span, 0);
      char *period_str = span_out(&speed_tiles[k].period, 0);
      char *interval_str = pg_interval_out(&speed_splits[k].duration);
      printf("Tile: (%d,%d), Span: %s, Period: %s, Count: %d, Duration: %s\n",
        i, j, span_str, period_str, speed_splits[k].count, interval_str);
      free(span_str); free(period_str); free(interval_str);
      k++;
    }
  }

  /* Free memory */
  free(trip_tiles);
  free(trip_splits);
  free(speed_tiles);
  free(speed_splits);

  /* Close the file */
  fclose(file);

  /* Finalize MEOS */
  meos_finalize();

  return 0;
}
