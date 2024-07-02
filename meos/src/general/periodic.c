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

#if NPOINT
  #include "npoint/tnpoint_spatialfuncs.h"
#endif




/*****************************************************************************
 *  PMode
*****************************************************************************/

PMode *
pmode_in(const char *str)
{
  return pmode_parse(&str);
}

PMode *
pmode_parse(const char **str)
{
  Interval* frequency = NULL;
  int32 repetitions = 0;
  bool keep_pattern = true;
  int delim = 0;
  char *endptr = NULL;  

  while ((*str)[delim] != ';') delim++;
  char *str1 = palloc(sizeof(char) * (delim + 1));
  strncpy(str1, *str, delim);
  str1[delim] = '\0';
  *str += delim + 1;

  // Frequency
  frequency = pg_interval_in(str1, -1); 

  // Repetitions
  repetitions = strtol(*str, &endptr, 10); // 10 cause base 10
  if (*str == endptr) 
  {
    elog(ERROR, "Could not parse %s value (repetitions)", "periodic mode");
  }
  *str = endptr;

  /* Move str after the semicolon */
  while (**str != ';' && **str != '\0') (*str)++;
  if (**str == ';') (*str)++;

  // Boolean
  if (strncmp(*str, "true", 4) == 0) {
    keep_pattern = true;
    *str += 4;
  } else if (strncmp(*str, "false", 5) == 0) {
    keep_pattern = false;
    *str += 5;
  } else {
    elog(ERROR, "Could not parse %s value (keep_pattern)", "periodic mode");
  }

  /* Move str after the semicolon */
  while (**str != ';' && **str != '\0') (*str)++;
  if (**str == ';') (*str)++;

  // Span - bool span_parse(const char **str, meosType spantype, bool end, Span *span);
  Span *period = NULL;
  span_parse(str, T_TSTZRANGE, true, period);

  ensure_end_input(str, "periodic mode");
  pfree(str1);

  return pmode_make(frequency, repetitions, keep_pattern, period);
}

PMode *
pmode_make(Interval *frequency, int32 repetitions, bool keep_pattern, Span *anchor)
{
  PMode *pmode = palloc(sizeof(PMode));
  pmode->frequency = *frequency;
  pmode->repetitions = repetitions;
  pmode->keep_pattern = keep_pattern;
  pmode->anchor = *anchor;
  return pmode;
}

char *
pmode_out(const PMode *pmode)
{
  const Interval *freq_iv = &(pmode->frequency);
  char *freq_str = pg_interval_out(freq_iv); 
  char *rep_str = int4_out(pmode->repetitions);
  char *result = palloc(sizeof(char)*63 + strlen(freq_str) + strlen(rep_str));
  sprintf(result, "Interval: %s; Repetitions: %s", freq_str, rep_str);
  return result;
}

/*****************************************************************************
 *  In/Out
*****************************************************************************/

Periodic *
periodic_in(const char *str, meosType temptype)
{

  /* Possible inputs

    # DEFAULT FLAG
    [A@2023-01-01 00:00:00, B@2023-01-01 02:00:00] <-- temporal seq: shift to 2000 00:00:00 UTC (note the 00:00:00 start time)
    [A#2000-01-01 00:00:00, B#2000-01-01 02:00:00] <-- periodic seq: don't shift
    [A, B#Interval]
    [A#Interval, B#Interval] <--- not sure about this one

    # INFO/REFERENCE KEEPING FLAGS
    [A#08:00:00, B#10:00:00]                # PER DAY FLAG
    [A#Mon 08:00:00, B#Tue 08:00:00]        # PER WEEK FLAG
    [A#01 08:00:00, B#02 08:00:00]          # PER MONTH FLAG
    [A#Jan 01 08:00:00, B#Feb 01 08:00:00]  # PER YEAR FLAG

  */

  /*
    1) Input and parse as intervals list or other depending on the flag.
    1.Q) How to input the flag ? Automatic ? User specified ?
      Do DAY; []
    2) Store as dates relative to 2000 UTC
  */

  return (Periodic *) periodic_parse(&str, temptype);
  // return (Periodic *) temporal_parse(&str, temptype);
}




char *
periodic_out(const Periodic *per, int maxdd)
{
  char *result;
  assert(temptype_subtype(per->subtype)); // <-- fixme in what cases could it not be valid ?
  if (per->subtype == TINSTANT)
    result = pinstant_out((PInstant *) per, maxdd);
  else if (per->subtype == TSEQUENCE)
    result = psequence_out((PSequence *) per, maxdd);
  else /* temp->subtype == TSEQUENCESET */
    result = psequenceset_out((PSequenceSet *) per, maxdd);
  return result;
}


char *
pinstant_out(const PInstant *pinst, int maxdd)
{
  perType ptype = MEOS_FLAGS_GET_PERIODIC(pinst->flags);
  return pinstant_to_string(pinst, ptype, maxdd, &basetype_out);
}

char *
psequence_out(const PSequence *pseq, int maxdd)
{
  perType ptype = MEOS_FLAGS_GET_PERIODIC(pseq->flags);
  return psequence_to_string(pseq, ptype, maxdd, false, &basetype_out);
}

char *
psequenceset_out(const PSequenceSet *pss, int maxdd)
{
  perType ptype = MEOS_FLAGS_GET_PERIODIC(pss->flags);
  return psequenceset_to_string(pss, ptype, maxdd, &basetype_out);
}


char *
pinstant_to_string(const PInstant *inst, const perType ptype, int maxdd, outfunc value_out)
{  
  // Reference: https://www.postgresql.org/docs/16/functions-formatting.html
  // FIXME: Currently timestamptz formatting is done manually for BE locale
  //        Update to support for instance 01-12h + A.M/P.M format instead of 00-23h depending
  //        on user settings.

  // FIXME (?): When timestamps are greater than their corresponding perType period,
  //            a counter is added to indicate how many times greater is the timestamp.
  //            e.g. For a weekly output, the second Monday is marked as #Monday 12:00:00 +1W
  //                 as it is 1 week later.
  //            However, for monthly and yearly outputs that number will be inaccurate as 
  //            1) months can have 28/29/30/31 days
  //            2) years can have 365/366 days
  //            Nonetheless, periodic outputs in practice should not go over 1 additional time period,
  //            as those are used to mark simple overflows.
  //            e.g. GTFS timetables can be greater than 24h (to account for overnight transport)
  //                 yet their period should remain <24h.

  const size_t pattern_size = sizeof(char) * 128;

  TimestampTz reference_tstz;
  // reference_tstz = pg_timestamptz_in("2000-01-01 00:00:00", -1); // with locale time zone offset
  reference_tstz = (TimestampTz) (int64) 0; // i.e., 2000-01-01 00:00:00 UTC
  

  // elog(NOTICE, "REFERENCE UTC: %s", pg_timestamptz_out((TimestampTz) (int64) 0)); // 2000-01-01 01:00:00+01
  // elog(NOTICE, "REFERENCE CET: %s", pg_timestamptz_out(reference_tstz));          // 2000-01-01 00:00:00+01
  
  char *t = NULL;
  char *pattern = (char *) palloc(pattern_size); // fixme replace by strlen of int64_to_str
  bool include_us = (inst->t % 1000000) != 0; // checks if value has trailing microseconds
  if (ptype == P_DAY) 
  {
    long int day_ratio = 86400000000 + (long int) reference_tstz; // microseconds in a day + timezone offset
    long int no_days = (long int) (inst->t / day_ratio); 
    if (include_us)
    {
      t = format_timestamptz(inst->t, "HH24:MI:SS.USTZH");  // hour:minutes:seconds.microseconds+timezone_hours
    }
    else
      t = format_timestamptz(inst->t, "HH24:MI:SSTZH");  // hour:minutes:seconds
    if (no_days > 0) 
    {
      snprintf(pattern, pattern_size, "%s+%ldD", t, no_days);
      pfree(t);
      t = pattern;
    }
  }
    
  else if (ptype == P_WEEK)
  {
    long int week_ratio = 604800000000 + (long int) reference_tstz;
    long int no_weeks = (long int) (inst->t / week_ratio); // microseconds in a week
    // Shifting up by 2 days cause 2000-01-01 is actually a Saturday and not a Monday.
    // But we assume that date as Monday 00:00:00. Shifting only affects FMDay output.
    TimestampTz temp_t = add_timestamptz_interval(inst->t, pg_interval_in("2 days", -1));
    if (include_us)
    {
      // t = format_timestamptz(temp_t, "FMDay HH24:MI:SS.USTZH"); // day_of_week hour:minutes:seconds.microseconds+timezone_hours
      t = format_timestamptz(temp_t, "FMDay HH24:MI:SS.US"); // day_of_week hour:minutes:seconds.microseconds+timezone_hours
    }
    else
    {
      // t = format_timestamptz(temp_t, "FMDay HH24:MI:SSTZH"); // day_of_week hour:minutes:seconds+timezone_hours
      t = format_timestamptz(temp_t, "FMDay HH24:MI:SS"); // day_of_week hour:minutes:seconds+timezone_hours
    }
    if (no_weeks > 0) 
    {
      snprintf(pattern, pattern_size, "%s+%ldW", t, no_weeks);
      pfree(t);
      t = pattern;
    }
  }

  // else if (ptype == P_MONTH)
  // {
  //   long int month_ratio = 2678400000000 + (long int) reference_tstz; // microseconds in 31 days (January)
  //   long int no_months = (long int) (inst->t / month_ratio); // (WARNING: imprecision if no_months > 1)
  //   if (include_us)
  //     t = format_timestamptz(inst->t, "DD HH24:MI:SS.USTZH");  // day_of_month hour:minutes:seconds.microseconds+timezone_hours
  //   else
  //     t = format_timestamptz(inst->t, "DD HH24:MI:SSTZH");  // day_of_month hour:minutes:seconds+timezone_hours
    
  //   if (no_months > 0) 
  //   {
  //     snprintf(pattern, pattern_size, "%s+%ldM", t, no_months);
  //     pfree(t);
  //     t = pattern;
  //   }
  // }
    
  // else if (ptype == P_YEAR)
  // {
  //   long int year_ratio = 31622400000000 + (long int) reference_tstz; // microseconds in 366 days (as 2000 is leap year)
  //   long int no_years = (long int) (inst->t / year_ratio);  // (WARNING: possible imprecision)
  //   if (include_us)
  //     t = format_timestamptz(inst->t, "Mon DD HH24:MI:SS.USTZH"); // day_of_month month hour:minutes:seconds.microseconds+timezone_hours
  //   else
  //     t = format_timestamptz(inst->t, "Mon DD HH24:MI:SSTZH"); // day_of_month month hour:minutes:seconds+timezone_hours
    
  //   if (no_years > 0) 
  //   {
  //     snprintf(pattern, pattern_size, "%s+%ldY", t, no_years);
  //     pfree(t);
  //     t = pattern;
  //   }
  // }
    
  else if (ptype == P_INTERVAL)
  {
    Interval *diff = (Interval *) minus_timestamptz_timestamptz(inst->t, reference_tstz);
    t = pg_interval_out(diff);
  }
    
  else 
  {
    //t = pg_timestamptz_out(inst->t); // default
    t = pg_timestamp_out(inst->t); // default
  }
   

  meosType basetype = temptype_basetype(inst->temptype);
  char *value = value_out(tinstant_value((TInstant *) inst), basetype, maxdd);
  char *result = palloc(strlen(value) + strlen(t) + 2);
  snprintf(result, strlen(value) + strlen(t) + 2, "%s#%s", value, t);
  pfree(t);
  pfree(value);
  return result;
}

char *
psequence_to_string(const PSequence *pseq, const perType ptype, int maxdd, bool component, outfunc value_out)
{
  char **strings = palloc(sizeof(char *) * pseq->count);
  size_t outlen = 0;
  char prefix[20];
  interpType interp = MEOS_FLAGS_GET_INTERP(pseq->flags);
  if (! component && MEOS_FLAGS_GET_CONTINUOUS(pseq->flags) &&
      interp == STEP)
    sprintf(prefix, "Interp=Step;");
  else
    prefix[0] = '\0';
  for (int i = 0; i < pseq->count; i++)
  {
    const PInstant *inst = (PInstant *) TSEQUENCE_INST_N(pseq, i);
    strings[i] = pinstant_to_string((PInstant *) inst, ptype, maxdd, value_out);
    outlen += strlen(strings[i]) + 1;
  }
  char open, close;
  if (MEOS_FLAGS_DISCRETE_INTERP(pseq->flags))
  {
    open = (char) '{';
    close = (char) '}';
  }
  else
  {
    open = pseq->period.lower_inc ? (char) '[' : (char) '(';
    close = pseq->period.upper_inc ? (char) ']' : (char) ')';
  }
  return stringarr_to_string(strings, pseq->count, outlen, prefix, open, close,
    QUOTES_NO, SPACES);
}

char *
psequenceset_to_string(const PSequenceSet *pss, const perType ptype, int maxdd, outfunc value_out)
{
  char **strings = palloc(sizeof(char *) * pss->count);
  size_t outlen = 0;
  char prefix[20];
  if (MEOS_FLAGS_GET_CONTINUOUS(pss->flags) &&
      ! MEOS_FLAGS_LINEAR_INTERP(pss->flags))
    sprintf(prefix, "Interp=Step;");
  else
    prefix[0] = '\0';
  for (int i = 0; i < pss->count; i++)
  {
    const PSequence *pseq = (PSequence *) TSEQUENCESET_SEQ_N(pss, i);
    strings[i] = psequence_to_string(pseq, ptype, maxdd, true, value_out);
    outlen += strlen(strings[i]) + 1;
  }
  return stringarr_to_string(strings, pss->count, outlen, prefix, '{', '}',
    QUOTES_NO, SPACES);
}

/*****************************************************************************
 *  Copy
*****************************************************************************/

Periodic *
periodic_copy(const Periodic *per)
{
  Periodic *result = palloc0(VARSIZE(per));
  memcpy(result, per, VARSIZE(per));
  return result;
}

PInstant *
pinstant_copy(const PInstant *pinst)
{
  PInstant *result = palloc0(VARSIZE(pinst));
  memcpy(result, pinst, VARSIZE(pinst));
  return result;
}

PSequence *
psequence_copy(const PSequence *pseq)
{
  PSequence *result = palloc0(VARSIZE(pseq));
  memcpy(result, pseq, VARSIZE(pseq));
  return result;
}

PSequenceSet *
psequenceset_copy(const PSequenceSet *pss)
{
  PSequenceSet *result = palloc0(VARSIZE(pss));
  memcpy(result, pss, VARSIZE(pss));
  return result;
}

/*****************************************************************************
 *  Periodic type
*****************************************************************************/

Periodic *
periodic_set_pertype(const Periodic *per, perType ptype)
{
  Periodic *result;
  if (per->subtype == TINSTANT)
    result = periodic_copy(per);
  else if (per->subtype == TSEQUENCE) // fixme there is probably a cleaner way of doing this.. (todo: c.f. lifting)
  {
    PSequence *tempSeq = (PSequence*) per;
    // setting flag for each individual instant composing the sequence
    interpType interp = MEOS_FLAGS_GET_INTERP(tempSeq->flags);
    // meosType basetype = temptype_basetype(tempSeq->temptype);
    bool lower_inc = tempSeq->period.lower_inc;
    bool upper_inc = tempSeq->period.upper_inc;
    int16 flags = tempSeq->flags;
    int ninsts = temporal_num_instants((Temporal*) tempSeq);
    PInstant** instants = (PInstant**) temporal_insts((Temporal*) tempSeq, &ninsts);
    for (int i = 0; i < ninsts; i++) 
    {
      MEOS_FLAGS_SET_PERIODIC(instants[i]->flags, ptype);
    }
    result = (Periodic*) tsequence_make((const TInstant **) instants, ninsts, lower_inc, upper_inc, interp, NORMALIZE_NO);
    result->flags = flags;
  }
  else if (per->subtype == TSEQUENCESET)
  {
    meos_error(ERROR, MEOS_ERR_INTERNAL_ERROR, "periodic_set_pertype: TODO");
    result = periodic_copy(per);
  }
  else { // fixme remove later
    result = periodic_copy(per);
    meos_error(ERROR, MEOS_ERR_INTERNAL_ERROR, "Unknown periodic subtype %s", per->subtype);
    result = periodic_copy(per);
  }
  MEOS_FLAGS_SET_PERIODIC(result->flags, ptype);
  return result;
}

char *
periodic_get_pertype(const Periodic *per)
{
  char *result = palloc(sizeof(char) * MEOS_PERTYPE_STR_MAXLEN);
  perType ptype = MEOS_FLAGS_GET_PERIODIC(per->flags);
  switch (ptype) {
    case P_DAY:
      strcpy(result, "day");
      break;
    case P_WEEK:
      strcpy(result, "week");
      break;
    case P_MONTH:
      strcpy(result, "month");
      break;
    case P_YEAR:
      strcpy(result, "year");
      break;
    case P_INTERVAL:
      strcpy(result, "interval");
      break;
    case P_DEFAULT:
    case P_NONE:
    default:
      strcpy(result, "none");
      break;
  }
  return result;
}



/*****************************************************************************
 *  Operations
*****************************************************************************/

/*
 * TODOS
 * - support anchor low/up inclusion 
 */
Temporal *
anchor(Periodic *per, PMode *pmode) 
{
  

  Temporal *result;
  Temporal *temp;
  Temporal *base_temp;
  Temporal *work_temp;

  if (!ensure_not_null(per) || !ensure_not_null(pmode))
    return NULL;

  Interval *frequency = &(pmode->frequency);
  int32 repetitions = pmode->repetitions;
  TimestampTz start_tstz = pmode->anchor.lower;
  TimestampTz end_tstz = pmode->anchor.upper;
  bool lower_inc = pmode->anchor.upper_inc;
  bool upper_inc = pmode->anchor.upper_inc;
  bool keep_pattern = pmode->keep_pattern;
  uint8 spantype = pmode->anchor.spantype;

  Interval *duration = temporal_duration((Temporal*) per, true);

  /* Some basic validity checks */
  if (spantype != T_DATESPAN && spantype != T_TSTZSPAN)
    return NULL;
  
  
  if (pg_interval_cmp(duration, frequency) > 0) 
  {
    meos_error(ERROR, MEOS_ERR_INVALID_ARG_VALUE,
      "Anchor(): Frequency (%s) must be greater or equal than the time range of the periodic sequence (%s).",
      pg_interval_out(frequency), pg_interval_out(duration));
    return NULL;
  }

  if (repetitions == -1)
    repetitions = INT32_MAX;
  
  if (end_tstz < start_tstz)
    end_tstz = INT64_MAX;
  
  if (repetitions == INT32_MAX && end_tstz == INT64_MAX)
  {
    meos_error(ERROR, MEOS_ERR_INVALID_ARG_VALUE,
    "Anchor(): Periodic sequence must have either repetitions or end timestamp defined.");
    return NULL;
  }

  /* Creates first anchored temporal sequence. */
  temp = (Temporal *) periodic_copy(per);
  MEOS_FLAGS_SET_PERIODIC(temp->flags, P_NONE);

  TimestampTz anchor_reference = (TimestampTz) (int64) 0; // 2000 UTC
  TimestampTz frequency_reference = temporal_start_timestamptz(temp);

  Interval *frequency_interval = pg_interval_in("0 days", -1);

  /* Shift the copy s.t. it begins at start_tstz */
  Interval *anchor_interval = minus_timestamptz_timestamptz(start_tstz, anchor_reference);

  base_temp = (Temporal*) temporal_shift_scale_time(temp, anchor_interval, NULL);

  for (int32 i = 1; i < repetitions; i++)
  {
    /* Incrementing frequency */
    frequency_interval = add_interval_interval(frequency_interval, frequency);
    Interval *shift_interval = add_interval_interval(anchor_interval, frequency_interval);

    /* Shifts new seq accordingly */
    temp = (Temporal*) periodic_copy(per);
    MEOS_FLAGS_SET_PERIODIC(temp->flags, P_NONE);
    temp = (Temporal*) temporal_shift_scale_time(temp, shift_interval, NULL); 

    /* Stop if reached upper anchor bound */
    if (temporal_end_timestamptz(temp) >= end_tstz) {
      pfree(temp);
      break;
    }

    // Merge new seq and seq.
    work_temp = base_temp;
    base_temp = temporal_merge(work_temp, temp);
    pfree(work_temp);
    pfree(temp);
  }

  /* Do not include last pattern occurrence if it does not fit */
  if (! keep_pattern || temporal_end_timestamptz(temp) == end_tstz)
    base_temp = temporal_merge(base_temp, temp);

  /* Trim if the trajectory is longer than anchor span */
  if (temporal_end_timestamptz(temp) > end_tstz) 
  {
    Span *trim_span = span_make(TimestampGetDatum(start_tstz), TimestampGetDatum(end_tstz), true, upper_inc, T_TIMESTAMPTZ);
    base_temp = temporal_restrict_tstzspan(base_temp, trim_span, REST_AT);
  }
  
  result = base_temp;
  return result;
}



// bool
// periodic_value_at_timestamptz(const Periodic *per, PMode *pmode, TimestampTz tstz, bool strict, Datum *result)
// {
//   assert(per); assert(pmode); assert(result);
//   assert(temptype_subtype(temp->subtype));
//   switch (temp->subtype)
//   {
//     case TINSTANT:
//       return tinstant_value_at_timestamptz((TInstant *) temp, t, result);
//     case TSEQUENCE:
//       return tsequence_value_at_timestamptz((TSequence *) temp, t, strict, result);
//       // return MEOS_FLAGS_DISCRETE_INTERP(temp->flags) ?
//       //   tdiscseq_value_at_timestamptz((TSequence *) temp, t, result) :
//       //   tsequence_value_at_timestamptz((TSequence *) temp, t, strict, result);
//     default: /* TSEQUENCESET */
//       return false;
//       // return tsequenceset_value_at_timestamptz((TSequenceSet *) temp, t,
//       //   strict, result);
//   }
// }




/*****************************************************************************
 *  Other
*****************************************************************************/

char * 
format_timestamptz(TimestampTz tstz, const char *fmt) 
{
    /*
     *	Small trick to convert TimestampTz output to Timestamp output
     *  as pg_timestamptz_to_char() assumes input timestamp has time zone
     *  (although we want to format using UTC only, without tz)
     *  the idea is to remove the equivalent tz_offset beforehand
     *	e.g.,
     *  initial timestamptz: 
     *    Oct 01 10:00:00 -- UTC
     *    Oct 01 12:00:00+02 -- CEST (output)
     *  pg_timestamp_out('Oct 01 12:00:00+02'):
     *    Oct 01 10:00:00
     *  pg_timestamptz_in('Oct 01 10:00:00'):
     *    Oct 01 10:00:00+02 -- CEST
     *    Oct 01 08:00:00 -- UTC
     *  pg_timestamptz_to_char('Oct 01 08:00:00')
     *    Oct 01 10:00:00 -- expected final output
     */
    // Timestamp ts_without_tz = pg_timestamp_in(pg_timestamp_out(pg_timestamptz_in(pg_timestamp_out((Timestamp) tstz), -1)), -1);
    text *fmt_text = cstring2text(fmt);
    // text *out_text = pg_timestamptz_to_char(ts_without_tz, fmt_text);
    text *out_text = pg_timestamp_to_char((Timestamp) tstz, fmt_text);
    char *result = text2cstring(out_text);
    return result;
}
