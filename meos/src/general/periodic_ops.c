/**
  DISCLAIMER: The below code is proof of concept and is not intended for production use.
              It is still in developped in the context of a master thesis.
*/



#include "general/periodic.h"
#include "general/temporal.h"
#include "general/periodic_parser.h"
#include "general/periodic_pg_types.h"

/* C */
#include <assert.h>
#include <time.h>
#include "utils/date.h"
/* GEOS */
#include <geos_c.h>
/* POSTGRESQL */
#include <postgres.h>
#if POSTGRESQL_VERSION_NUMBER >= 160000
  #include "varatt.h"
#endif
/* POSTGIS */
#include <lwgeodetic.h>
#include <lwgeom_log.h>
#include <lwgeom_geos.h>
/* MEOS */
#include <meos.h>
#include <meos_internal.h>
#include "general/doxygen_meos.h"
#include "general/pg_types.h"
// #include "general/temporaltypes.h"
#include "general/temporal_boxops.h"
// #include "general/tnumber_distance.h"
#include "general/temporal_tile.h"
#include "general/type_parser.h"
#include "general/type_util.h"
#include "general/type_out.h"
// #include "point/pgis_call.h"
#include "point/tpoint_spatialfuncs.h"


/*****************************************************************************
 *  Operations
*****************************************************************************/

/* 
 * TODO 
 * - possible missing edge cases with keep_pattern and sequence/anchor low/up bound inclusion
 */
Temporal *
anchor_pmode(const Periodic *per, PMode *pmode) 
{
  const Temporal *relative = (Temporal *) per;

  Temporal *result = NULL;
  Temporal *temp = NULL;
  Temporal *base_temp = NULL;
  Temporal *work_temp = NULL;

  if (! per && ! pmode)
    return NULL;
  
  if (MEOS_FLAGS_GET_PERIODIC(per->flags) == P_NONE)
  {
    meos_error(ERROR, MEOS_ERR_INVALID_ARG_VALUE,
    "Anchor(): Temporal is not periodic.");
    return NULL;
  }

  const Interval *frequency = &(pmode->frequency);
  int32 repetitions = pmode->repetitions;
  const Span *anchor = &(pmode->anchor);
  const TimestampTz start_tstz = pmode->anchor.lower;
  TimestampTz end_tstz = pmode->anchor.upper;
  const bool keep_pattern = pmode->keep_pattern;
  const uint8 spantype = pmode->anchor.spantype;

  /* Some basic validity checks */

  if (spantype != T_TSTZSPAN)
    return NULL;

  Interval *duration = temporal_duration(relative, true);
  if (pg_interval_cmp(duration, frequency) > 0)
  {
    meos_error(ERROR, MEOS_ERR_INVALID_ARG_VALUE,
      "Anchor(): Frequency (%s) must be greater or equal than the time range of the periodic sequence (%s).",
      pg_interval_out(frequency), pg_interval_out(duration));
    return NULL;
  }

  if (repetitions < 0)
    repetitions = INT32_MAX;
  
  if (end_tstz <= start_tstz)
    end_tstz = INT64_MAX;
  
  if (repetitions == INT32_MAX && end_tstz == INT64_MAX)
  {
    meos_error(ERROR, MEOS_ERR_INVALID_ARG_VALUE,
    "Anchor(): Periodic sequence must have either repetitions or end timestamp defined.");
    return NULL;
  }

  /* Creates first anchored temporal sequence. */
  temp = temporal_copy(relative);
  MEOS_FLAGS_SET_PERIODIC(temp->flags, P_NONE);

  TimestampTz anchor_reference = (TimestampTz) (int64) 0; // 2000 UTC

  Interval *frequency_interval = pg_interval_in("0 days", -1);
  Interval *anchor_interval = minus_timestamptz_timestamptz(start_tstz, anchor_reference);

  base_temp = (Temporal*) temporal_shift_scale_time(temp, anchor_interval, NULL);

  int32 i;
  for (i = 0; i < repetitions; i++)
  {
    /* Incrementing frequency */
    frequency_interval = add_interval_interval(frequency_interval, frequency);
    Interval *shift_interval = add_interval_interval(anchor_interval, frequency_interval);

    /* Shifts new seq accordingly */
    temp = temporal_copy(relative);
    MEOS_FLAGS_SET_PERIODIC(temp->flags, P_NONE);
    temp = (Temporal *) temporal_shift_scale_time(temp, shift_interval, NULL); 

    /* Stop if reached upper anchor bound */
    if (temporal_end_timestamptz(temp) >= end_tstz)
    {
      bool check1 = temporal_end_timestamptz(temp) > end_tstz && keep_pattern;
      bool check2 = temporal_end_timestamptz(temp) == end_tstz && keep_pattern && (! anchor->upper_inc && temporal_upper_inc(temp)); // i.e., when seq upper bound is inclusive but anchor's is not
      /* Do not include last pattern occurrence if it does not fit */
      if (check1 || check2)
      {
        pfree(temp);
        break;
      }
      i = repetitions;
    }

    // Merge new seq and seq.
    work_temp = base_temp;
    base_temp = temporal_merge(work_temp, temp);
    pfree(work_temp);
    pfree(temp);
  }

  /* Trim if the trajectory is longer than anchor span */
  work_temp = base_temp;
  base_temp = temporal_restrict_tstzspan(base_temp, anchor, REST_AT);

  pfree(work_temp);
  result = base_temp;
  return result;
}


Temporal *
anchor(const Temporal *periodic, const Span *ts_anchor, const Interval *frequency, const bool strict_pattern)
{
  Temporal *result = NULL;
  Temporal *temp = NULL;
  Temporal *base_temp = NULL;
  Temporal *work_temp = NULL;

  if (! periodic || ! ts_anchor)
    return NULL;
    
  if (MEOS_FLAGS_GET_PERIODIC(periodic->flags) == P_NONE)
  {
    meos_error(ERROR, MEOS_ERR_INVALID_ARG_VALUE,
    "Anchor(): Temporal is not periodic.");
    return NULL;
  }

  if (ts_anchor->spantype != T_TSTZSPAN) 
  {
    meos_error(ERROR, MEOS_ERR_INVALID_ARG_VALUE,
    "Anchor(): SPAN type must be a TSTZSPAN");
    return NULL;
  }
    
  if (! frequency)
    frequency = temporal_duration(periodic, true);
  
  TimestampTz start_tstz = ts_anchor->lower;
  TimestampTz end_tstz = ts_anchor->upper;

  if (end_tstz <= start_tstz)
    end_tstz = INT64_MAX;

  /* Return if anchor is shorter than the base sequence */
  Interval *anchor_range = minus_timestamptz_timestamptz(end_tstz, start_tstz);
  Interval *duration = temporal_duration(periodic, true);
  if (strict_pattern && pg_interval_cmp(duration, anchor_range) > 0)
    return NULL;
    
  TimestampTz anchor_reference = (TimestampTz) (int64) 0; // 2000 UTC
  Interval *frequency_interval = pg_interval_in("0 days", -1);
  Interval *anchor_interval = minus_timestamptz_timestamptz(start_tstz, anchor_reference);
  Interval *shift_interval = add_interval_interval(anchor_interval, frequency_interval);

  bool finished = false;
  do 
  {
    /* Copy base pattern */
    temp = temporal_copy(periodic);
    MEOS_FLAGS_SET_PERIODIC(temp->flags, P_NONE);
    /* Shift pattern copy */
    temp = (Temporal *) temporal_shift_scale_time(temp, shift_interval, NULL); 

    /* Checking stop condition */
    if (temporal_end_timestamptz(temp) >= end_tstz)
    {
      /* Do not include last pattern occurrence if it does not fit */
      bool check1 = temporal_end_timestamptz(temp) > end_tstz && strict_pattern;
      bool check2 = temporal_end_timestamptz(temp) == end_tstz && strict_pattern && (! ts_anchor->upper_inc && temporal_upper_inc(temp));
      // bool check3 = temporal_start_timestamptz(temp) >= end_tstz;
      if (check1 || check2)
      {
        pfree(temp);
        break;
      }
      finished = true;
    }

    /* Merge copied pattern with the currently built temporal */
    if (base_temp)
    {
      work_temp = base_temp;
      base_temp = temporal_merge(work_temp, temp);
      pfree(work_temp); pfree(temp);
    }
    else // NULL
    {
      base_temp = temp;
    }

    /* Incrementing frequency */
    if (! finished) 
    {
      frequency_interval = add_interval_interval(frequency_interval, frequency);
      shift_interval = add_interval_interval(anchor_interval, frequency_interval);
    }
  } 
  while (! finished);

  /* Trim the trajectory if longer than anchor span */
  work_temp = base_temp;
  base_temp = (Temporal *) temporal_restrict_tstzspan(base_temp, ts_anchor, REST_AT);   
  if (work_temp != base_temp)
    pfree(work_temp);

  result = base_temp;
  return result;
}

// todo todo todo 
/*
 * TODO TODO TODO
 * merge with other anchor functions
 * FIXME duplicate code
 */
Temporal *
anchor_array(const Temporal *periodic, const Span *ts_anchor, const Interval *frequency, const bool strict_pattern, const Datum *service_array, const int array_count)
{
  Temporal *result = NULL;
  Temporal *temp = NULL;
  Temporal *base_temp = NULL;
  Temporal *work_temp = NULL;

  if (! periodic || ! ts_anchor)
    return NULL;
    
  if (MEOS_FLAGS_GET_PERIODIC(periodic->flags) == P_NONE)
  {
    meos_error(ERROR, MEOS_ERR_INVALID_ARG_VALUE,
    "Anchor(): Temporal is not periodic.");
    return NULL;
  }

  if (ts_anchor->spantype != T_TSTZSPAN) 
  {
    meos_error(ERROR, MEOS_ERR_INVALID_ARG_VALUE,
    "Anchor(): SPAN type must be a TSTZSPAN");
    return NULL;
  }
    
  if (! frequency)
    frequency = temporal_duration(periodic, true);
  
  TimestampTz start_tstz = ts_anchor->lower;
  TimestampTz end_tstz = ts_anchor->upper;

  if (end_tstz <= start_tstz)
    end_tstz = INT64_MAX;

  /* Return if anchor is shorter than the base sequence */
  Interval *anchor_range = minus_timestamptz_timestamptz(end_tstz, start_tstz);
  Interval *duration = temporal_duration(periodic, true);
  if (strict_pattern && pg_interval_cmp(duration, anchor_range) > 0)
    return NULL;
    
  TimestampTz anchor_reference = (TimestampTz) (int64) 0; // 2000 UTC
  Interval *frequency_interval = pg_interval_in("0 days", -1);
  Interval *anchor_interval = minus_timestamptz_timestamptz(start_tstz, anchor_reference);
  Interval *shift_interval = add_interval_interval(anchor_interval, frequency_interval);

  int service_i = 0;
  bool finished = false;
  while (! finished) 
  {
    /* Copy base pattern */
    temp = temporal_copy(periodic);
    MEOS_FLAGS_SET_PERIODIC(temp->flags, P_NONE);
    /* Shift pattern copy */
    temp = (Temporal *) temporal_shift_scale_time(temp, shift_interval, NULL); 

    /* Checking stop condition */
    if (temporal_end_timestamptz(temp) >= end_tstz)
    {
      /* Do not include last pattern occurrence if it does not fit */
      bool check1 = temporal_end_timestamptz(temp) > end_tstz && strict_pattern;
      bool check2 = temporal_end_timestamptz(temp) == end_tstz && strict_pattern && (! ts_anchor->upper_inc && temporal_upper_inc(temp));
      // bool check3 = temporal_start_timestamptz(temp) >= end_tstz;
      if (check1 || check2)
      {
        pfree(temp);
        break;
      }
      finished = true;
    }

    /* Merge copied pattern with the currently built temporal */
    if (DatumGetInt32(service_array[service_i % array_count]))
    {
      if (base_temp)
      {
        work_temp = base_temp;
        base_temp = temporal_merge(work_temp, temp);
        pfree(work_temp); pfree(temp);
      }
      else // NULL
      {
        base_temp = temp;
      }
    }
    service_i += 1;

    /* Incrementing frequency */
    if (! finished) 
    {
      frequency_interval = add_interval_interval(frequency_interval, frequency);
      shift_interval = add_interval_interval(anchor_interval, frequency_interval);
    }
  }

  if (!base_temp) 
    return NULL;

  /* Trim the trajectory if longer than anchor span */
  work_temp = base_temp;
  base_temp = (Temporal *) temporal_restrict_tstzspan(base_temp, ts_anchor, REST_AT);   
  if (work_temp != base_temp)
    pfree(work_temp);

  result = base_temp;
  return result;
  
  return NULL;
}



/**
 * @brief Shifts temporal such that starts at the given timestamp.
 * By default should be shifted to 2000-01-01 00:00:00 UTC i.e.,  Timestamp 0
 */
Periodic *
periodic_align(const Periodic *per, const Timestamp ts)
{
  if (!per) return NULL;
  // TimestampTz reference_tstz = (TimestampTz) (int64) 0;
  TimestampTz start_tstz = temporal_start_timestamptz((Temporal*) per);
  Interval *diff = (Interval*) minus_timestamptz_timestamptz(ts, start_tstz);
  Periodic* result = (Periodic*) temporal_shift_scale_time((Temporal*) per, diff, NULL);
  return result;
}

/* TODO
 * - add strict pattern check
 */
bool
periodic_value_at_timestamptz(
  const Periodic *per, 
  const Span *anchor_ts, 
  const Interval *frequency,
  TimestampTz tstz, 
  bool strict, 
  Datum *result)
{
  assert(per); assert(anchor_ts); assert(frequency); 
  assert(result);
  assert(temptype_subtype(temp->subtype));

  TimestampTz limit_ts = anchor_ts->upper;
  TimestampTz search_ts = anchor_ts->lower;
  TimestampTz previous_ts;

  if (tstz < search_ts || tstz > limit_ts) 
    return false;

  /* Idea
   * If in range, tstz should be between previous_ts and search_ts
   * which delimit the duration of the periodic sequence.
   * tstz - previous_ts gives the corresponding relative instant
   */
  do 
  {
    previous_ts = search_ts;
    search_ts = add_timestamptz_interval(search_ts, frequency);
  } while (search_ts < tstz);
  TimestampTz goal_ts = tstz - previous_ts;

  /* Call temporal value_at_timestamptz() */
  return temporal_value_at_timestamptz((Temporal*) per, goal_ts, strict, result);
}
