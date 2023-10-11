/**
  DISCLAIMER: The below code is proof of concept and is not intended for production use.
              It is still in developped in the context of a master thesis.
*/

#include "general/periodic.h"
#include "general/temporal.h"

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
#include "general/doxygen_libmeos.h"
#include "general/pg_types.h"
#include "general/temporaltypes.h"
#include "general/temporal_boxops.h"
#include "general/tnumber_distance.h"
#include "general/temporal_tile.h"
#include "general/type_parser.h"
#include "general/type_util.h"
#include "general/type_out.h"
#include "point/pgis_call.h"
#include "point/tpoint_spatialfuncs.h"



#if NPOINT
  #include "npoint/tnpoint_spatialfuncs.h"
#endif



/*****************************************************************************
 *  Temporary MEOS / MOBILITYDB defintions
*****************************************************************************/

#if ! MEOS
  extern Datum call_function1(PGFunction func, Datum arg1);
  extern Datum call_function2(PGFunction func, Datum arg1, Datum arg2);
  extern Datum call_function3(PGFunction func, Datum arg1, Datum arg2, Datum arg3);
  extern Datum timestamptz_to_char(PG_FUNCTION_ARGS);
  extern Datum interval_in(PG_FUNCTION_ARGS);
  extern Datum interval_out(PG_FUNCTION_ARGS);
#endif /* ! MEOS */


#if ! MEOS

  text *
  pg_timestamptz_to_char(TimestampTz dt, text *fmt)
  {
    Datum arg1 = TimestampTzGetDatum(dt);
    Datum arg2 = PointerGetDatum(fmt);
    text *result = DatumGetTextP(call_function2(timestamptz_to_char, arg1, arg2));
    return result;
  }

  Interval *
  pg_interval_in(const char *str, int32 typmod)
  {
    Datum arg1 = CStringGetDatum(str);
    Datum arg2 = Int32GetDatum(typmod);
    Interval *result = DatumGetIntervalP(call_function2(interval_in, arg1, arg2));
    return result;
  }

  char *
  pg_interval_out(const Interval *span)
  {
    Datum arg1 = IntervalPGetDatum(span);
    char *result = DatumGetCString(call_function1(interval_out, arg1));
    return result;
  }

#endif /* ! MEOS */




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
  int delim = 0;
  char *endptr = NULL;  

  while (*str[delim] != ';') delim++;
  char *str1 = palloc(sizeof(char) * (delim + 1));
  strncpy(str1, *str, delim);
  str[delim] = '\0';
  *str += delim;

  // Frequency
  // TODO/FIXME
  // frequency = pg_interval_in(str1, -1); 
  
  // Repeitions
  repetitions = strtol(*str, &endptr, 10); // 10 cause base 10
  if (*str == endptr) 
  {
    elog(ERROR, "Could not parse %s value (repetitions)", "periodic mode");
  }
  *str = endptr;

  ensure_end_input(str, true, "periodic mode");
  pfree(str1);

  return pmode_make(frequency, repetitions);
}

PMode *
pmode_make(Interval *frequency, int32 repetitions)
{
  PMode *pmode = palloc(sizeof(PMode));
  pmode->frequency = *frequency;
  pmode->repetitions = repetitions;
  return pmode;
}

char *
pmode_out(const PMode *pmode)
{
  const Interval *freq_iv = &(pmode->frequency);
  
  elog(NOTICE, "PMODE OUT DEBUG: days: %d", freq_iv->day);
  elog(NOTICE, "PMODE OUT DEBUG: repet: %d", pmode->repetitions);

  // TODO / FIXME : add call_func for pg_interval_out
  // char *freq_str = pg_interval_out(freq_iv); 
  // pg_interval_out -> undefined symbol for some reason although pg_interval_in is ok
  // edit: now pg_interval_in doesnt work either D:

  // char *str = psprintf("%s, %d", freq_str, repet);
  // char *str = psprintf("freq: %d, repetitions: %d", freq_iv->day, pmode->repetitions);
  // char *str = psprintf("Interval (month %d, day: %d); Repetitions: %d", freq_iv->month, freq_iv->day, pmode->repetitions);

  char *mon_str = int4_out(freq_iv->month);
  char *dd_str = int4_out(freq_iv->day);
  char *rep_str = int4_out(pmode->repetitions);
  char *result = palloc(sizeof(char)*64 + strlen(mon_str) + strlen(dd_str) + strlen(rep_str));
  sprintf(result, "Interval (month %s, day: %s); Repetitions: %s", mon_str, dd_str, rep_str);
  
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

  // todo add flags

  return (Periodic *) temporal_parse(&str, temptype);
}

// Periodic *
// temporal_parse(const char **str, meosType temptype)
// {
//   p_whitespace(str);
//   Temporal *result = NULL;  /* keep compiler quiet */
//   interpType interp = temptype_continuous(temptype) ? LINEAR : STEP;
//   /* Starts with "Interp=Step;" */
//   if (pg_strncasecmp(*str, "Interp=Step;", 12) == 0)
//   {
//     /* Move str after the semicolon */
//     *str += 12;
//     interp = STEP;
//   }

//   /* Allow spaces after the Interpolation */
//   p_whitespace(str);

//   if (**str != '{' && **str != '[' && **str != '(')
//     result = (Temporal *) tinstant_parse(str, temptype, true, true);
//   else if (**str == '[' || **str == '(')
//     result = (Temporal *) tcontseq_parse(str, temptype, interp, true, true);
//   else if (**str == '{')
//   {
//     const char *bak = *str;
//     p_obrace(str);
//     p_whitespace(str);
//     if (**str == '[' || **str == '(')
//     {
//       *str = bak;
//       result = (Temporal *) tsequenceset_parse(str, temptype, interp);
//     }
//     else
//     {
//       *str = bak;
//       result = (Temporal *) tdiscseq_parse(str, temptype);
//     }
//   }
//   return result;
// }

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
  // todo also add interval output
  // todo check ensure setting flag on seq also sets it to its instants (?)
  
  char *t = NULL;

  if (ptype == P_DAY)
    t = format_timestamptz(inst->t, "HH24:MI:SS");  // hour:minutes:seconds
  else if (ptype == P_WEEK) 
    t =format_timestamptz(inst->t, "FMDay HH24:MI:SS"); // day_of_week hour:minutes:seconds
  else if (ptype == P_MONTH)
    t = format_timestamptz(inst->t, "DD HH24:MI:SS");  // day_of_month hour:minutes:seconds
  else if (ptype == P_YEAR) 
    t = format_timestamptz(inst->t, "DD Mon HH24:MI:SS"); // day_of_month month hour:minutes:seconds
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
  if (MEOS_FLAGS_GET_DISCRETE(pseq->flags))
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
      ! MEOS_FLAGS_GET_LINEAR(pss->flags))
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
*****************************************************************************/

Periodic *
tint_to_pint(Temporal *temp)
{
  // todo fixme
  Periodic *result;
  assert(temptype_subtype(temp->subtype));
  if (temp->subtype == TINSTANT)
    result = (Periodic *) tinstant_copy((TInstant *) temp);
  else if (temp->subtype == TSEQUENCE)
    result = (Periodic *) tsequence_copy((TSequence *) temp);
  else /* temp->subtype == TSEQUENCESET */
    result = (Periodic *) tsequenceset_copy((TSequenceSet *) temp);
  return result;
}

Temporal *
pint_to_tint(Periodic *temp) 
{
  // todo fixme
  Temporal *result;
  assert(temptype_subtype(temp->subtype));
  if (temp->subtype == TINSTANT)
    result = (Temporal *) tinstant_copy((TInstant *) temp);
  else if (temp->subtype == TSEQUENCE)
    result = (Temporal *) tsequence_copy((TSequence *) temp);
  else /* temp->subtype == TSEQUENCESET */
    result = (Temporal *) tsequenceset_copy((TSequenceSet *) temp);
  return result;
}



/*****************************************************************************
 *  Operations
*****************************************************************************/

// Temporal* periodic_generate(Periodic* temp, PMode* pmode) 
// {
//   // Generate a TSequenceSet from TSequence
//   // if repetitions is specified -> repeat at max 'repetitions' times
//   // if end_time is specified -> generated until that date
//   // if both repetitions and end_time -> max 'repetitions' times but not further than end_time

//   // ...

// }


Periodic* temporal_make_periodic(Temporal* temp, PMode* pmode) 
{
  // todo currently shifts 1st to 00:00:00, 
  // but we might want to keep some info like time or part of date

  // TInstant *start = temporal_start_instant(temp);
  // TimestampDifferenceMilliseconds(start_tstz, )

  // 1) find shift from 1st element to 2000 UTC  
  char* reference_str = "2000-01-01 00:00:00 UTC";
  TimestampTz reference_tstz = pg_timestamptz_in(reference_str, -1);
  // elog(NOTICE, "debug tstz reference: %s", pg_timestamptz_out(reference_tstz));

  //TimestampTz result = DatumGetTimestampTz(call_function3(timestamptz_in, arg1,(Datum) 0, arg3));
  // Datum arg1 = CStringGetDatum(str);
  // Datum reference_str_datum = CStringGetDatum(reference_str);
  // elog(NOTICE, "MAKE RELATIVE TEST: %s", DatumGetCString(call_function1(pg_timestamptz_out, reference_str_datum)));

  TimestampTz start_tstz = temporal_start_timestamp(temp);
  // elog(NOTICE, "debug tstz start: %s", pg_timestamptz_out(start_tstz));

  Interval *diff = pg_timestamp_mi(reference_tstz, start_tstz);

  // 2) shift the rest of the sequence accordingly
  Interval *shift_to_rel = diff;
  Temporal *shifted_temp = temporal_shift_tscale(temp, shift_to_rel, NULL);

  Periodic* result = (Periodic *) shifted_temp;

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