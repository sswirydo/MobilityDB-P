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
#include "general/tnumber_distance.h"
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
pmode_make(Interval *frequency, int32 repetitions, bool keep_pattern, Span *period)
{
  PMode *pmode = palloc(sizeof(PMode));
  pmode->frequency = *frequency;
  pmode->repetitions = repetitions;
  pmode->keep_pattern;
  pmode->period = *period;
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
  char *t = NULL;

  if (ptype == P_DAY)
    t = format_timestamptz(inst->t, "HH24:MI:SS");  // hour:minutes:seconds
  else if (ptype == P_WEEK) {
    // Shifting up by 2 days cause 2000-01-01 is actually a Saturday and not a Monday.
    // But we assume that date as Monday 00:00:00. Shifting only affects FMDay output.
    // TimestampTz temp_t = pg_timestamp_pl_interval(inst->t, pg_interval_in("2 days", -1));
    TimestampTz temp_t = add_timestamptz_interval(inst->t, pg_interval_in("2 days", -1));
    t = format_timestamptz(temp_t, "FMDay HH24:MI:SS"); // day_of_week hour:minutes:seconds
  }
  else if (ptype == P_MONTH)
    t = format_timestamptz(inst->t, "DD HH24:MI:SS");  // day_of_month hour:minutes:seconds
  else if (ptype == P_YEAR) 
    t = format_timestamptz(inst->t, "Mon DD HH24:MI:SS"); // day_of_month month hour:minutes:seconds
  else if (ptype == P_INTERVAL) {
    TimestampTz reference_tstz = pg_timestamptz_in("2000-01-01 00:00:00", -1);
    // Interval *diff = (Interval *) pg_timestamp_mi(inst->t, reference_tstz);
    Interval *diff = (Interval *) minus_timestamptz_timestamptz(inst->t, reference_tstz);
    t = pg_interval_out(diff);
  }
    
  else 
    t = pg_timestamptz_out(inst->t); // default

  meosType basetype = temptype_basetype(inst->temptype);
  char *value = value_out(tinstant_value((TInstant *) inst), basetype, maxdd);
  char *result = palloc(strlen(value) + strlen(t) + 2);
  sprintf(result, "%s#%s", value, t);
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

  // todo shall we set flags for each temporal (insts and seqs) composing the temporal or only the englobing temporal ?
  // todo shall we ensure perType is valid ? (a priori no constraints to change perType ?)
  // ensure_valid_interpolation(temp->temptype, interp); 

  Periodic *result = periodic_copy(per);
  MEOS_FLAGS_SET_PERIODIC(result->flags, ptype);
  return result;
}

char *
periodic_get_pertype(const Periodic *per)
{
  char *result = palloc(sizeof(char) * MEOS_PERTYPE_STR_MAXLEN);
  perType ptype = MEOS_FLAGS_GET_PERIODIC(per->flags);
  if (ptype == P_DAY)
    strcpy(result, "day");
  else if (ptype == P_WEEK)
    strcpy(result, "week");
  else if (ptype == P_MONTH)
    strcpy(result, "month");
  else if (ptype == P_YEAR)
    strcpy(result, "year");
  else
    strcpy(result, "none");
  return result;
}


/*****************************************************************************
 *  Casting
 * 
 *  TODO obviously a simple cast is not eough
 * 
*****************************************************************************/

// Periodic *
// tint_to_pint(Temporal *temp)
// {
//   // todo fixme
//   Periodic *result;
//   assert(temptype_subtype(temp->subtype));
//   if (temp->subtype == TINSTANT)
//     result = (Periodic *) tinstant_copy((TInstant *) temp);
//   else if (temp->subtype == TSEQUENCE)
//     result = (Periodic *) tsequence_copy((TSequence *) temp);
//   else /* temp->subtype == TSEQUENCESET */
//     result = (Periodic *) tsequenceset_copy((TSequenceSet *) temp);
//   return result;
// }

// Temporal *
// pint_to_tint(Periodic *temp) 
// {
//   // todo fixme
//   Temporal *result;
//   assert(temptype_subtype(temp->subtype));
//   if (temp->subtype == TINSTANT)
//     result = (Temporal *) tinstant_copy((TInstant *) temp);
//   else if (temp->subtype == TSEQUENCE)
//     result = (Temporal *) tsequence_copy((TSequence *) temp);
//   else /* temp->subtype == TSEQUENCESET */
//     result = (Temporal *) tsequenceset_copy((TSequenceSet *) temp);
//   return result;
// }



/*****************************************************************************
 *  Operations
*****************************************************************************/


// Temporal * anchor(Periodic* per, PMode* pmode, TimestampTz start, TimestampTz end, bool upper_inc) 
Temporal * 
anchor(Periodic* per, PMode* pmode) 
{
  /** PARAMETERS:
   * Frequency: after how long should the sequence repeat itself (interval relative to the start of the sequence)
   *  If is empty, repeat directly
   * Repetitions: how many times should the sequence repeat ()
   *  If empty, repeat until end of span range
   * Range: start and end of the created sequence
  */

  /** WARNING:
   * Ensure FREQUENCY is > than the length of sequence
   * Ensure REPETITIONS and END of range are not empty at the same time
   */

 /** TODO:
  * Is there a clean way to specify upper bound inclusion?
  *   Maybe replace (start, end) by tstz span (s.t. we can have low_up_inc) ? 
  *   But what if we don't want an upper bound (empty tstz end) ?
  *   Can be set to PG_INT64_MAX
  */

 /** TBD:
  * Must the whole pattern occurn at the end or can the pattern be trimmed if does not fit ?
  *   Both for upper_bound and interval.  
  * 
  */

  Temporal *result;

  if (!ensure_not_null(per) || !ensure_not_null(pmode))
    return NULL;

  Temporal *temp;
  Temporal *base_temp;
  Temporal *work_temp;
  TimestampTz reference_tstz = pg_timestamptz_in("2000-01-01 00:00:00", -1);
  Interval *work_freq = pg_interval_in("0 days", -1);

  perType ptype = MEOS_FLAGS_GET_PERIODIC(per->flags);
  
  Interval *frequency = &(pmode->frequency);
  int32 repetitions = pmode->repetitions;
  TimestampTz start = pmode->period.lower;
  TimestampTz end = pmode->period.upper;
  bool upper_inc = pmode->period.upper_inc;

  bool lower_inc = pmode->period.lower_inc; // todo
  bool keep_pattern = pmode->keep_pattern; // todo
  uint8 spantype = pmode->period.spantype; // todo?

  // Create basic temporal sequence.
  temp = (Temporal*) periodic_copy(per);
  MEOS_FLAGS_SET_PERIODIC(temp->flags, P_NONE);

  // Shift it s.t. it starts at "start".
  // Interval *diff = pg_timestamp_mi(start, reference_tstz);
  Interval *diff = minus_timestamptz_timestamptz(start, reference_tstz);
  base_temp = (Temporal*) temporal_shift_scale_time(temp, diff, NULL);

  // Repeat until repetition is empty or end is reached.
  for (int32 i = 1; i < repetitions; i++)
  {
    // Incrementing frequency. 
    // work_freq = pg_interval_pl(work_freq, frequency);
    work_freq = add_interval_interval(work_freq, frequency);

    // Shifts new seq accordingly.
    temp = (Temporal*) periodic_copy(per);
    MEOS_FLAGS_SET_PERIODIC(temp->flags, P_NONE);
    work_temp = (Temporal*) temporal_shift_scale_time(temp, diff, NULL); 
    work_temp = (Temporal*) temporal_shift_scale_time(work_temp, work_freq, NULL);
    // (note: keeping diff and work_freq separate to avoid mixing interval days and months etc.)

    // TODO Trim if frequency is month based and overflow into the next month.
    // ...

    // Merge new seq and seq.
    base_temp = temporal_merge(base_temp, work_temp);

    // Stop if reached end.
    if (temporal_end_timestamptz(base_temp) >= end) { 
      break;
    }

  }
  // Trim if necessary
  Span *trim_span = span_make(TimestampGetDatum(start), TimestampGetDatum(end), true, upper_inc, T_TIMESTAMPTZ);
  // base_temp = temporal_restrict_period(base_temp, trim_span, REST_AT);
  base_temp = temporal_restrict_tstzspan(base_temp, trim_span, REST_AT);
  result = base_temp;
  return result;
}





/*****************************************************************************
 *  Other
*****************************************************************************/

char * 
format_timestamptz(TimestampTz tstz, const char *fmt) 
{
    text *fmt_text = cstring2text(fmt);
    text *out_text = pg_timestamptz_to_char(tstz, fmt_text);
    char *result = text2cstring(out_text);
    return result;
}



// RSequence *
// repeat_in(const char *str, meosType temptype) 
// {
//   RSequence *rseq = (RSequence *) temporal_parse(&str, temptype);
//   // todo below does not makes sense as we haven't allocate memory for freq not rep
//   rseq->frequency = *(pg_interval_in("666 days", -1));
//   rseq->repetitions = 1515; 
//   return rseq;
// }

// char *
// repeat_out(const RSequence *per, int maxdd) 
// {
//   char *result;
//   assert(temptype_subtype(per->subtype));
//   if (per->subtype == TINSTANT)
//     result = "SUSHI RINSTANT";
//   else if (per->subtype == TSEQUENCE) {
//     result = psequence_out((PSequence *) per, maxdd);
//     char* temp1 = pg_interval_out(&(per->frequency));
//     char* temp2 = int4_out(per->repetitions);

//     int new_len = strlen(result) + strlen(temp1) + strlen(temp2) + 1;
//     result = (char*) repalloc(result, new_len);
//     strcat(result, temp1);
//     strcat(result, temp2);
//   }
    
//   else /* temp->subtype == TSEQUENCESET */
//     result = "SUSHI RSEQUENCESET";
//   return result;
// }

// RSequence *
// r_distance(const RSequence *per, Datum value) 
// {
//   // return distance_tint_int(per, value);

//   elog(NOTICE, "SUSHI A: %s", temporal_out(per, 0));


//   Temporal *temp = distance_tnumber_number(per, value, T_INT4, T_TINT);

//   elog(NOTICE, "SUSHI A: %s", temporal_out(temp, 0));


//   RSequence* rseq = (RSequence*) palloc(sizeof(RSequence));
//   memcpy(rseq, temp, sizeof(TSequence));
//   rseq->frequency = per->frequency;
//   rseq->repetitions = per->repetitions;


//   elog(NOTICE, "SUSHI A: %s", temporal_out(rseq, 0));

//   return rseq;

//   // return temp;
// }


// RSequence* create_rsequence_from_tsequence(TSequence* tseq, Interval frequency, int32 repetitions) {

//   RSequence* rseq = (RSequence*) palloc(sizeof(RSequence));
//   memcpy(rseq, tseq, sizeof(TSequence)); // + instants sizes etc
//   rseq->frequency = frequency;
//   rseq->repetitions = repetitions;

//   return rseq;
// }
