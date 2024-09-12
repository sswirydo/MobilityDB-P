/**
  DISCLAIMER: The below code is proof of concept and is not intended for production use.
              It is still in developped in the context of a master thesis.
*/



#include "general/periodic.h"
#include "general/temporal.h"
#include "general/periodic_parser.h"
#include "general/periodic_pg_types.h"
#include "general/periodic_ops.h"

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


Temporal *
anchor(const Temporal *periodic, const Span *ts_anchor, const Interval *period, const bool strict_pattern)
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
    
  if (! period)
    period = temporal_duration(periodic, true);
  
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
  Interval *period_interval = pg_interval_in("0 days", -1);
  Interval *anchor_interval = minus_timestamptz_timestamptz(start_tstz, anchor_reference);
  Interval *shift_interval = add_interval_interval(anchor_interval, period_interval);

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

    /* Incrementing period */
    if (! finished) 
    {
      period_interval = add_interval_interval(period_interval, period);
      shift_interval = add_interval_interval(anchor_interval, period_interval);
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


/*
 * FIXME duplicate code
 * TODO possibly merge with other anchor function (?)
 * or rather just split these into smaller reusable functions
 */
Temporal *
anchor_array(const Temporal *periodic, const Span *ts_anchor, const Interval *period, const bool strict_pattern, const Datum *service_array, const int array_shift, const int array_count)
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
    
  if (! period)
    period = temporal_duration(periodic, true);
  
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
  Interval *period_interval = pg_interval_in("0 days", -1);
  Interval *anchor_interval = minus_timestamptz_timestamptz(start_tstz, anchor_reference);
  Interval *shift_interval = add_interval_interval(anchor_interval, period_interval);

  int service_i = array_shift;
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

    /* Incrementing period */
    if (! finished) 
    {
      period_interval = add_interval_interval(period_interval, period);
      shift_interval = add_interval_interval(anchor_interval, period_interval);
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


bool
periodic_value_at_timestamptz(
  const Periodic *per, 
  const Span *anchor_ts, 
  const Interval *period,
  TimestampTz tstz, 
  bool strict, 
  Datum *result)
{
  assert(per); assert(anchor_ts); assert(period); 
  assert(result);

  TimestampTz goal_ts;

  TimestampTz low_bound = anchor_ts->lower;
  TimestampTz up_bound = anchor_ts->upper;

  if (tstz < low_bound || tstz > up_bound) 
    return false;

  TimestampTz start_tstz = temporal_start_timestamptz((Temporal*) per);
  goal_ts = start_tstz + periodic_timestamptz_to_relative(low_bound + start_tstz, period, tstz);
  
  /* Call temporal value_at_timestamptz() */
  return temporal_value_at_timestamptz((Temporal*) per, goal_ts, strict, result);
}



Timestamp periodic_timestamptz_to_relative(const Timestamp reference_ts, const Interval *period, const TimestampTz tstz) 
{
  int64 ts_freq = (int64) add_timestamptz_interval(reference_ts, period);
  int64 tstz_norm = tstz - reference_ts;
  int64 freq_norm = ts_freq - reference_ts;
  return (Timestamp) (tstz_norm % freq_norm);
}

