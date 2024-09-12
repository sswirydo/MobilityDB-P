/**
  DISCLAIMER: The below code is proof of concept and is not intended for production use.
              It is still in developped in the context of a master thesis.
*/


/**
  NOTE: most of below functions were (temporarily) copied and adapted from Temporal "type_parser.c"
        and should be merged with corresponding temporal parse functions
        if the behaviour is accepted. (check differences)
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

  perType pertype = P_DEFAULT;
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
  else if (pg_strncasecmp(*str, "Periodic=Interval;", 18) == 0)
  {
    *str += 18;
    pertype = P_INTERVAL;
  }
  p_whitespace(str);


  // TODO: Only doing sequences for now. Add seqsets later.
  if (**str == '[' || **str == '(') {
    PSequence *seq;
    if (! pcontseq_parse(str, temptype, pertype, interp, true, &seq))
      return NULL;
    result = (Periodic *) seq;
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

  bool contains_at = strchr(*str, '@') != NULL;
  bool contains_hash = strchr(*str, '#') != NULL;
  if (contains_at && contains_hash) 
  {
    meos_error(ERROR, MEOS_ERR_TEXT_INPUT,
      "Could not parse periodic value: Sequence mixes temporals and periodics. (@ != #)");
      return false;
  }

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

  if (result)
    *result = (PSequence*)tsequence_make_free((TInstant**)instants, count, lower_inc, upper_inc, interp, NORMALIZE); // todo
  
  // If input contains @ instead of # perhaps shift the sequence s.t. it starts at 2000-01-01 00:00:00
  // if (contains_at) {
  //   *result = normalize_periodic_sequence(*result);
  // }

  MEOS_FLAGS_SET_PERIODIC((*result)->flags, pertype);
  
  return true;
}


bool 
pinstant_parse(const char **str, meosType temptype, perType pertype, bool end, PInstant **result)
{
  p_whitespace(str);
  meosType basetype = temptype_basetype(temptype);
  Datum elem;
  if (! periodic_basetype_parse(str, basetype, &elem))
    return false;
  
  TimestampTz t = periodic_timestamp_parse(str, pertype);
  
  if (t == DT_NOEND)
    return false;
  /* Ensure there is no more input */
  if (end && ! ensure_end_input(str, "periodic"))
    return false;
  if (result) {
    *result = (PInstant*) tinstant_make(elem, temptype, t);
    MEOS_FLAGS_SET_PERIODIC((*result)->flags, pertype);
  }
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


  TimestampTz reference_tstz;
  // reference_tstz = pg_timestamptz_in("2000-01-01 00:00:00", -1); // with TZ offset
  reference_tstz = (TimestampTz) (int64) 0; // i.e., 2000-01-01 00:00:00 UTC

  // Prefixing 2000 to certain inputs cause to_timestamp by default creates timestamps at year 0000.
  const char* parsePrefix = "2000 ";
  char *date2parse = palloc(strlen(parsePrefix) + strlen(str1) + 1);
  strcpy(date2parse, parsePrefix);
  strcat(date2parse, str1);

  if (pertype == P_DAY)
    result = pg_to_timestamp(cstring2text(date2parse), cstring2text("YYYY HH24:MI:SS.US")); // hour:minutes:seconds.microseconds
  else if (pertype == P_WEEK)
  {
    // Checking for day_of_week manually as FMDay is ignored in to_timestamp
    Interval *week_shift;
    if (pg_strncasecmp(str1, "Mon", 3) == 0) week_shift = pg_interval_in("0 days", -1); // 2000-01-01 is actually a Sat but we'll juste assume it is a Mon
    else if (pg_strncasecmp(str1, "Tue", 3) == 0) week_shift = pg_interval_in("1 days", -1);
    else if (pg_strncasecmp(str1, "Wed", 3) == 0) week_shift = pg_interval_in("2 days", -1);
    else if (pg_strncasecmp(str1, "Thu", 3) == 0) week_shift = pg_interval_in("3 days", -1);
    else if (pg_strncasecmp(str1, "Fri", 3) == 0) week_shift = pg_interval_in("4 days", -1);
    else if (pg_strncasecmp(str1, "Sat", 3) == 0) week_shift = pg_interval_in("5 days", -1); 
    else week_shift = pg_interval_in("6 days", -1);
    TimestampTz result2shift = pg_to_timestamp(cstring2text(date2parse), cstring2text("YYYY FMDay HH24:MI:SS.US")); // day_of_week hour:minutes:seconds.microseconds
    result = add_timestamptz_interval(result2shift, week_shift);
  }
  else if (pertype == P_INTERVAL) 
  {
    Interval *diff = pg_interval_in(str1, -1);
    result = add_timestamptz_interval(reference_tstz, diff);
  }
  else // P_DEFAULT, P_NONE
  {
    //result = pg_timestamptz_in(str1, -1);
    result = pg_timestamp_in(str1, -1);
  }
  

  /*
   *	Small trick to convert TimestampTz input to Timestamp input
   *	because PostgreSQL pg_to_timestamp() returns a ts WITH time zone.
   *	e.g.,
   *  user input: 
   *    Oct 01 10:00:00
   *  converted to UTC due to Europe/Brussels locale:
   *    Oct 01 10:00:00+02 -- CEST
   *    Oct 01 08:00:00    -- UTC
   *  pg_timestamp_in() of Oct 01 10:00:00+02:
   *    Oct 01 10:00:00    -- UTC
   */
  if (pertype == P_DAY ||pertype == P_WEEK)
    result = pg_timestamp_in(pg_timestamptz_out(result), -1);

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







/* ------------------------------------------------------- */


/**
 * @brief Parse a temporal point value from the buffer
 * @param[in] str Input string
 * @param[in] temptype Temporal type
 */
Periodic *
ppoint_parse(const char **str, meosType temptype)
{
  int tpoint_srid = 0;
  const char *bak = *str;
  p_whitespace(str);

  /* Starts with "SRID=". The SRID specification must be gobbled for all
   * types excepted TInstant. We cannot use the atoi() function
   * because this requires a string terminated by '\0' and we cannot
   * modify the string in case it must be passed to the #tpointinst_parse
   * function. */
  if (pg_strncasecmp(*str, "SRID=", 5) == 0)
  {
    /* Move str to the start of the number part */
    *str += 5;
    int delim = 0;
    tpoint_srid = 0;
    /* Delimiter will be either ',' or ';' depending on whether interpolation
       is given after */
    while ((*str)[delim] != ',' && (*str)[delim] != ';' && (*str)[delim] != '\0')
    {
      tpoint_srid = tpoint_srid * 10 + (*str)[delim] - '0';
      delim++;
    }
    /* Set str to the start of the temporal point */
    *str += delim + 1;
  }

  /* We cannot ensure that the SRID is geodetic for geography since
   * the srid_is_latlong function is not exported by PostGIS */
  // if (temptype == T_TGEOGPOINT)
    // srid_is_latlong(fcinfo, tpoint_srid);

  interpType interp = temptype_continuous(temptype) ? LINEAR : STEP;
  /* Starts with "Interp=Step" */
  if (pg_strncasecmp(*str, "Interp=Step;", 12) == 0)
  {
    /* Move str after the semicolon */
    *str += 12;
    interp = STEP;
  }

  /* Allow spaces after the SRID and/or Interpolation */
  p_whitespace(str);



  perType pertype = P_DEFAULT;
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
  // else if (pg_strncasecmp(*str, "Periodic=Month;", 15) == 0)
  // {
  //   *str += 15;
  //   pertype = P_MONTH;
  // }
  // else if (pg_strncasecmp(*str, "Periodic=Year;", 14) == 0)
  // {
  //   *str += 14;
  //   pertype = P_YEAR;
  // }
  else if (pg_strncasecmp(*str, "Periodic=Interval;", 18) == 0)
  {
    *str += 18;
    pertype = P_INTERVAL;
  }
  p_whitespace(str);


  Periodic *result = NULL; /* keep compiler quiet */
  /* Determine the type of the temporal point */
  if (**str != '{' && **str != '[' && **str != '(')
  {
    /* Pass the SRID specification */
    *str = bak;
    PInstant *inst;
    if (! ppointinst_parse(str, temptype, pertype, true, &tpoint_srid, &inst))
      return NULL;
    result = (Periodic *) inst;
  }
  else if (**str == '[' || **str == '(')
  {
    PSequence *seq;
    if (! ppointcontseq_parse(str, temptype, pertype, interp, true, &tpoint_srid, &seq))
      return NULL;
    result = (Periodic *) seq;
  }
  else if (**str == '{')
  {
    bak = *str;
    p_obrace(str);
    p_whitespace(str);
    if (**str == '[' || **str == '(')
    {
      *str = bak;
      result = (Periodic *) ppointseqset_parse(str, temptype, pertype, interp,
        &tpoint_srid);
    }
    else
    {
      *str = bak;
      result = (Periodic *) ppointdiscseq_parse(str, temptype, pertype, &tpoint_srid);
    }
  }
  MEOS_FLAGS_SET_PERIODIC((result)->flags, pertype);
  return result;
}



bool 
ppointinst_parse(const char **str, meosType temptype, perType pertype, bool end, 
  int *tpoint_srid, PInstant **result)
{
  p_whitespace(str);
  meosType basetype = temptype_basetype(temptype);
  /* The next instruction will throw an exception if it fails */
  Datum geo;
  if (! periodic_basetype_parse(str, basetype, &geo))
    return false;
  GSERIALIZED *gs = DatumGetGserializedP(geo);
  if (! ensure_point_type(gs) || ! ensure_not_empty(gs) ||
      ! ensure_has_not_M_gs(gs))
  {
    pfree(gs);
    return false;
  }
  /* If one of the SRID of the temporal point and of the geometry
   * is SRID_UNKNOWN and the other not, copy the SRID */
  int geo_srid = gserialized_get_srid(gs);
  if (*tpoint_srid == SRID_UNKNOWN && geo_srid != SRID_UNKNOWN)
    *tpoint_srid = geo_srid;
  else if (*tpoint_srid != SRID_UNKNOWN &&
    ( geo_srid == SRID_UNKNOWN || geo_srid == SRID_DEFAULT ))
    gserialized_set_srid(gs, *tpoint_srid);
  /* If the SRID of the temporal point and of the geometry do not match */
  else if (*tpoint_srid != SRID_UNKNOWN && geo_srid != SRID_UNKNOWN &&
    *tpoint_srid != geo_srid)
  {
    meos_error(ERROR, MEOS_ERR_TEXT_INPUT,
      "Geometry SRID (%d) does not match temporal type SRID (%d)",
      geo_srid, *tpoint_srid);
    pfree(gs);
    return false;
  }
  TimestampTz t = periodic_timestamp_parse(str, pertype);
  if (t == DT_NOEND)
    return false;
  if (end && ! ensure_end_input(str, "temporal point"))
  {
    pfree(gs);
    return false;
  }
  if (result) {
    *result = (PInstant *) tinstant_make_free(PointerGetDatum(gs), temptype, t);
    MEOS_FLAGS_SET_PERIODIC((*result)->flags, pertype);
  }
    
  return true;
}





bool
ppointcontseq_parse(const char **str, meosType temptype, perType pertype, interpType interp,
  bool end, int *tpoint_srid, PSequence **result)
{
  p_whitespace(str);
  bool lower_inc = false, upper_inc = false;
  /* We are sure to find an opening bracket or parenthesis because that was the
   * condition to call this function in the dispatch function tpoint_parse */
  if (p_obracket(str))
    lower_inc = true;
  else if (p_oparen(str))
    lower_inc = false;

  /* First parsing */
  const char *bak = *str;
  if (! ppointinst_parse(str, temptype, pertype, false, tpoint_srid, NULL))
    return false;
  int count = 1;
  while (p_comma(str))
  {
    count++;
    if (! ppointinst_parse(str, temptype, pertype, false, tpoint_srid, NULL))
      return false;
  }
  if (p_cbracket(str))
    upper_inc = true;
  else if (p_cparen(str))
    upper_inc = false;
  else
  {
    meos_error(ERROR, MEOS_ERR_TEXT_INPUT,
      "Could not parse temporal point value: Missing closing bracket/parenthesis");
    return false;
  }
  /* Ensure there is no more input */
  if (end && ! ensure_end_input(str, "temporal point"))
    return false;

  /* Second parsing */
  *str = bak;
  PInstant **instants = palloc(sizeof(PInstant *) * count);
  for (int i = 0; i < count; i++)
  {
    p_comma(str);
    ppointinst_parse(str, temptype, pertype, false, tpoint_srid, &instants[i]);
  }
  p_cbracket(str);
  p_cparen(str);
  if (result) {
    *result = (PSequence *) tsequence_make_free((TInstant**)instants, count, lower_inc, upper_inc, interp, NORMALIZE);
    MEOS_FLAGS_SET_PERIODIC((*result)->flags, pertype);
  }
  return true;
}



PSequenceSet *
ppointseqset_parse(const char **str, meosType temptype, perType pertype, interpType interp,
  int *tpoint_srid)
{
  const char *type_str = "temporal point";
  p_whitespace(str);
  /* We are sure to find an opening brace because that was the condition
   * to call this function in the dispatch function tpoint_parse */
  p_obrace(str);

  /* First parsing */
  const char *bak = *str;
  if (! ppointcontseq_parse(str, temptype, pertype, interp, false, tpoint_srid, NULL))
    return NULL;
  int count = 1;
  while (p_comma(str))
  {
    count++;
    if (! ppointcontseq_parse(str, temptype, pertype, interp, false, tpoint_srid, NULL))
      return NULL;
  }
  if (! ensure_cbrace(str, type_str) || ! ensure_end_input(str, type_str))
    return NULL;

  /* Second parsing */
  *str = bak;
  PSequence **sequences = palloc(sizeof(PSequence *) * count);
  for (int i = 0; i < count; i++)
  {
    p_comma(str);
    ppointcontseq_parse(str, temptype, pertype, interp, false, tpoint_srid,
      &sequences[i]);
  }
  p_cbrace(str);
  return (PSequenceSet *) tsequenceset_make_free((TSequence**)sequences, count, NORMALIZE);
}


PSequence *
ppointdiscseq_parse(const char **str, meosType temptype, perType pertype, int *tpoint_srid)
{
  const char *type_str = "temporal point";
  p_whitespace(str);
  /* We are sure to find an opening brace because that was the condition
   * to call this function in the dispatch function #tpoint_parse */
  p_obrace(str);

  /* First parsing */
  const char *bak = *str;
  if (! ppointinst_parse(str, temptype, pertype, false, tpoint_srid, NULL))
    return NULL;
  int count = 1;
  while (p_comma(str))
  {
    count++;
    if (! ppointinst_parse(str, temptype, pertype, false, tpoint_srid, NULL))
      return NULL;
  }
  if (! ensure_cbrace(str, type_str) || ! ensure_end_input(str, type_str))
    return NULL;

  /* Second parsing */
  *str = bak;
  PInstant **instants = palloc(sizeof(PInstant *) * count);
  for (int i = 0; i < count; i++)
  {
    p_comma(str);
    ppointinst_parse(str, temptype, pertype, false, tpoint_srid, &instants[i]);
  }
  p_cbrace(str);
  return (PSequence*) tsequence_make_free((TInstant**)instants, count, true, true, DISCRETE,
    NORMALIZE_NO);
}