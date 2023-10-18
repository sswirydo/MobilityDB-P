/**
  DISCLAIMER: The below code is proof of concept and is not intended for production use.
              It is still in developped in the context of a master thesis.
*/


/**
  NOTE: most of below functions were (temporarily) copied and adapted from Temporal "type_parser.c"
*/

#include "general/periodic.h"
#include "general/temporal.h"
#include "general/periodic_parser.h"

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


#if ! MEOS
  extern Datum call_function1(PGFunction func, Datum arg1);
  extern Datum call_function2(PGFunction func, Datum arg1, Datum arg2);
  extern Datum call_function3(PGFunction func, Datum arg1, Datum arg2, Datum arg3);
  extern Datum to_timestamp(PG_FUNCTION_ARGS);
#endif /* ! MEOS */


#if ! MEOS

  TimestampTz 
  pg_to_timestamp(text *date_txt, text *fmt) // pg_to_timestamp(text *date_txt, text *fmt)
  {
    Datum arg1 = PointerGetDatum(date_txt);
    Datum arg2 = PointerGetDatum(fmt);
    TimestampTz result = DatumGetTimestampTz(call_function2(to_timestamp, arg1, arg2));
    return result;
  }

#endif /* ! MEOS */




Periodic *
periodic_parse(const char **str, meosType temptype) 
{
  p_whitespace(str);
  Periodic *result = NULL;
  interpType interp = temptype_continuous(temptype) ? LINEAR : STEP;
  /* Starts with "Interp=Step;" */
  if (pg_strncasecmp(*str, "Interp=Step;", 12) == 0)
  {
    /* Move str after the semicolon */
    *str += 12;
    interp = STEP;
  }
  /* Allow spaces after the Interpolation */
  p_whitespace(str);

  perType pertype = P_NONE;
  if (pg_strncasecmp(*str, "Periodic=Day;", 13) == 0)
  {
    *str += 13;
    pertype = P_DAY;
  }
   else if (pg_strncasecmp(*str, "Periodic=Week;", 14) == 0)
  {
    *str += 14;
    pertype = P_WEEK;
  }
  else if (pg_strncasecmp(*str, "Periodic=Month;", 15) == 0)
  {
    *str += 15;
    pertype = P_MONTH;
  }
  else if (pg_strncasecmp(*str, "Periodic=Year;", 14) == 0)
  {
    *str += 14;
    pertype = P_YEAR;
  }
  else if (pg_strncasecmp(*str, "Periodic=Interval;", 18) == 0)
  {
    *str += 18;
    pertype = P_INTERVAL;
  }
  p_whitespace(str);


  // TODO: Only doing sequences for now. Instants are (probably) not important. Add seqsets later.
  if (**str == '[' || **str == '(') {
    TSequence *seq;
    if (! pcontseq_parse(str, temptype, pertype, interp, true, &seq))
      return NULL;
    result = (Temporal *) seq;
  }
    
  return result;
}


bool 
pcontseq_parse(const char **str, meosType temptype, perType pertype, interpType interp,
  bool end, PSequence **result)
{
  p_whitespace(str);
  bool lower_inc = false, upper_inc = false;
  /* We are sure to find an opening bracket or parenthesis because that was the
   * condition to call this function in the dispatch function temporal_parse */
  if (p_obracket(str))
    lower_inc = true;
  else if (p_oparen(str))
    lower_inc = false;

  /* First parsing */
  const char *bak = *str;
  if (! pinstant_parse(str, temptype, pertype, false, NULL))
    return false;
  int count = 1;
  while (p_comma(str))
  {
    count++;
    if (! pinstant_parse(str, temptype, pertype, false, NULL))
      return false;
  }
  if (p_cbracket(str))
    upper_inc = true;
  else if (p_cparen(str))
    upper_inc = false;
  else
  {
    meos_error(ERROR, MEOS_ERR_TEXT_INPUT,
      "Could not parse periodic value: Missing closing bracket/parenthesis");
      return false;
  }
  /* Ensure there is no more input */
  if (end && ! ensure_end_input(str, "periodic"))
    return NULL;

  /* Second parsing */
  *str = bak;
  PInstant **instants = palloc(sizeof(PInstant *) * count);
  for (int i = 0; i < count; i++)
  {
    p_comma(str);
    pinstant_parse(str, temptype, pertype, false, &instants[i]);
  }
  p_cbracket(str);
  p_cparen(str);

  /* TODO

  Somewhere around here (or before)
  make sure instants are correct with all those different formats
  and make sure sequence is shifted to 2000 if contains @
  and ensure sequence is correct if contains #
  (note: does it neceserrily need to start at 2000 01 01 00:00:00,
  or can we accept the first one is omitted ?)
  
  */

  if (result)
    *result = tsequence_make_free(instants, count, lower_inc, upper_inc, interp, NORMALIZE); // todo
  

  // if ((*result)->subtype == TSEQUENCE) {
  //   elog(NOTICE, "SUSHI A");
  // }
  // else {
  //   elog(NOTICE, "SUSHI B");
  // }

  *result = normalize_periodic_sequence(*result, pertype);
  // MEOS_FLAGS_SET_PERIODIC((*result)->flags, pertype);
  
  return true;
}


bool 
pinstant_parse(const char **str, meosType temptype, perType pertype, bool end, PInstant **result)
{
  p_whitespace(str);
  meosType basetype = temptype_basetype(temptype); // todo keep this now but care for later oid updates
  /* The next two instructions will throw an exception if they fail */
  Datum elem;
  if (! periodic_basetype_parse(str, basetype, &elem))
    return false;
  
  TimestampTz t = periodic_timestamp_parse(str, pertype);
  
  if (t == DT_NOEND)
    return false;
  /* Ensure there is no more input */
  if (end && ! ensure_end_input(str, "periodic"))
    return false;
  if (result)
    *result = tinstant_make(elem, temptype, t);
  return true;
}

TimestampTz
periodic_timestamp_parse(const char **str, perType pertype) 
{
  p_whitespace(str);
  int delim = 0;
  while ((*str)[delim] != ',' && (*str)[delim] != ']' && (*str)[delim] != ')' &&
    (*str)[delim] != '}' && (*str)[delim] != '\0')
    delim++;
  char *str1 = palloc(sizeof(char) * (delim + 1));
  strncpy(str1, *str, delim);
  str1[delim] = '\0';
  /* The last argument is for an unused typmod */

  TimestampTz result;

  // Prefixing 2000 to certain inputs cause to_timestamp by default creates timestamps at year 0000.
  const char* parsePrefix = "2000 ";
  char *date2parse = palloc(strlen(parsePrefix) + strlen(str1) + 1);
  strcpy(date2parse, parsePrefix);
  strcat(date2parse, str1);

  if (pertype == P_DAY)
    result = pg_to_timestamp(cstring2text(date2parse), cstring2text("YYYY HH24:MI:SS")); // hour:minutes:seconds
  else if (pertype == P_WEEK) // todo fixme FMDay is ignored/insufficient for to_timestamp
    result = pg_to_timestamp(cstring2text(date2parse), cstring2text("YYYY FMDay HH24:MI:SS")); // day_of_week hour:minutes:seconds
  else if (pertype == P_MONTH) 
    result = pg_to_timestamp(cstring2text(date2parse), cstring2text("YYYY DD HH24:MI:SS")); // day_of_month hour:minutes:seconds
  else if (pertype == P_YEAR) 
    result = pg_to_timestamp(cstring2text(date2parse), cstring2text("YYYY Mon DD HH24:MI:SS")); // month day_of_month hour:minutes:seconds
  else if (pertype == P_INTERVAL) 
    result = pg_timestamptz_in(cstring2text(str1), -1); // todo todo todo 
  else // P_NONE
    result = pg_timestamptz_in(str1, -1);


  pfree(date2parse);
  pfree(str1);
  *str += delim;

  return result;

}




/**
 * @brief Parse a base value from the buffer
 */
bool
periodic_basetype_parse(const char **str, meosType basetype, Datum *result)
{
  p_whitespace(str);
  int delim = 0;
  /* Save the original string for error handling */
  char *origstr = (char *) *str;

  /* ttext values must be enclosed between double quotes */
  if (**str == '"')
  {
    /* Consume the double quote */
    *str += 1;
    while ( ( (*str)[delim] != '"' || (*str)[delim - 1] == '\\' )  &&
      (*str)[delim] != '\0' )
      delim++;
  }
  else
  {
    while ((*str)[delim] != '@' && (*str)[delim] != '#' && (*str)[delim] != '\0')
      delim++;
  }
  if ((*str)[delim] == '\0')
  {
    meos_error(ERROR, MEOS_ERR_TEXT_INPUT, 
      "Could not parse periodic value: %s", origstr);
    return false;
  }
  char *str1 = palloc(sizeof(char) * (delim + 1));
  strncpy(str1, *str, delim);
  str1[delim] = '\0';
  bool success = basetype_in(str1, basetype, false, result);
  pfree(str1);
  if (! success)
    return false;
  /* since there's an @ here, let's take it with us */
  *str += delim + 1;
  return true;
}

/**
 * @brief Ensures the first PSequence element starts at 2000 UTC and shifts the sequence accordingly.
 */
PSequence*
normalize_periodic_sequence(PSequence *pseq, perType pertype) 
{
  PSequence* result;

  if (pertype == P_NONE) // P_NONE
  {
    char *reference_2000_str = "2000-01-01 00:00:00";
    TimestampTz reference_2000_tstz = pg_timestamptz_in(reference_2000_str, -1);
    TimestampTz start_tstz = temporal_start_timestamp(pseq);
    Interval *diff2000 = pg_timestamp_mi(reference_2000_tstz, start_tstz);
    result = (PSequence *) temporal_shift_scale_time(pseq, diff2000, NULL);
  }
  else { // P_DAY, P_WEEK, ...
    result = pseq;
  }

  return result;
}

