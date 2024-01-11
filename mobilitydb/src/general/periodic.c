/**
  DISCLAIMER: The below code is proof of concept and is not intended for production use.
              It is still in developped in the context of a master thesis.
*/


/* Periodic */
#include "pg_general/periodic.h"
#include "general/periodic.h"

#include "pg_general/temporal.h"

/* C */
#include <assert.h>
/* PostgreSQL */
#if POSTGRESQL_VERSION_NUMBER >= 130000
  #include <access/heaptoast.h>
  #include <access/detoast.h>
#else
  #include <access/tuptoaster.h>
#endif
#include <libpq/pqformat.h>
#include <funcapi.h>
/* MEOS */
#include <meos.h>
#include <meos_internal.h>
#include "general/pg_types.h"
#include "general/temporaltypes.h"
#include "general/temporal_boxops.h"
#include "general/type_out.h"
#include "general/type_util.h"
/* MobilityDB */
#include "pg_general/doxygen_mobilitydb_api.h"
#include "pg_general/meos_catalog.h"
#include "pg_general/tinstant.h"
#include "pg_general/tsequence.h"
#include "pg_general/type_util.h"
#include "pg_point/tpoint_spatialfuncs.h"


/*****************************************************************************
 *  Temporary (copy paste)
*****************************************************************************/
// /**
//  * @brief Ensure that the temporal type of a temporal value corresponds to the
//  * typmod
//  */
// static Temporal *
// temporal_valid_typmod_temp(Temporal *temp, int32_t typmod)
// {
//   /* No typmod (-1) */
//   if (typmod < 0)
//     return temp;
//   uint8 typmod_subtype = TYPMOD_GET_SUBTYPE(typmod);
//   uint8 subtype = temp->subtype;
//   /* Typmod has a preference */
//   if (typmod_subtype != ANYTEMPSUBTYPE && typmod_subtype != subtype)
//     ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
//       errmsg("Temporal type (%s) does not match column type (%s)",
//       tempsubtype_name(subtype), tempsubtype_name(typmod_subtype))));
//   return temp;
// }


/*****************************************************************************
 *  PMode
*****************************************************************************/

PGDLLEXPORT Datum PMode_in(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(PMode_in);
Datum PMode_in(PG_FUNCTION_ARGS)
{
  const char *str = PG_GETARG_CSTRING(0);
  PG_RETURN_PMODE_P(pmode_in(str));
}

PGDLLEXPORT Datum PMode_out(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(PMode_out);
Datum PMode_out(PG_FUNCTION_ARGS)
{
  PMode *pmode = PG_GETARG_PMODE_P(0);
  char *result = pmode_out(pmode);
  PG_FREE_IF_COPY(pmode, 0);
  PG_RETURN_CSTRING(result);
}

// PGDLLEXPORT Datum PMode_send(PG_FUNCTION_ARGS); // todo update later
// PG_FUNCTION_INFO_V1(PMode_send);
// Datum PMode_send(PG_FUNCTION_ARGS)
// {
//   PMode *pmode = PG_GETARG_PMODE_P(0);
//   StringInfoData buffer;
//   pq_begintypsend(&buffer);
//   pq_sendint64(&buffer, (uint64) pmode->repetitions);
//   PG_FREE_IF_COPY(pmode, 0);
//   PG_RETURN_BYTEA_P(pq_endtypsend(&buffer));
// }

// PGDLLEXPORT Datum PMode_recv(PG_FUNCTION_ARGS); // todo update later
// PG_FUNCTION_INFO_V1(PMode_recv);
// Datum PMode_recv(PG_FUNCTION_ARGS)
// {
//   StringInfo buffer = (StringInfo) PG_GETARG_POINTER(0);
//   PMode *pmode = (PMode *) palloc(sizeof(PMode));
//   pmode->repetitions = pq_getmsgint64(buffer);
//   PG_RETURN_PMODE_P(pmode);
// }

PGDLLEXPORT Datum PMode_constructor(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(PMode_constructor);
Datum PMode_constructor(PG_FUNCTION_ARGS)
{
  Interval *frequency = PG_GETARG_INTERVAL_P(0);
  int repetitions = PG_GETARG_INT32(1);
  bool keep_pattern = PG_GETARG_BOOL(2);
  Span *period = PG_GETARG_SPAN_P(3);
  // if ((PG_NARGS() > 2) && ! PG_ARGISNULL(n))
  PG_RETURN_PMODE_P(pmode_make(frequency, repetitions, keep_pattern, period));
}

// PGDLLEXPORT Datum PMode_constructor(PG_FUNCTION_ARGS);
// PG_FUNCTION_INFO_V1(PMode_constructor);
// Datum PMode_constructor(PG_FUNCTION_ARGS)
// {
//   Interval frequency = PG_GETARG_INTERVAL_P(0);
//   int repetitions = PG_GETARG_INT32(1);
//   PG_RETURN_PMODE_P(pmode_make(frequency, repetitions));
// }


/*****************************************************************************
 *  Periodic
*****************************************************************************/

PGDLLEXPORT Datum Anchor(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(Anchor);
Datum Anchor(PG_FUNCTION_ARGS)
{
  Periodic *per = PG_GETARG_PERIODIC_P(0);
  PMode *pmode = PG_GETARG_PMODE_P(1);
  PG_RETURN_POINTER(anchor(per, pmode));
}

// PGDLLEXPORT Datum Anchor_end(PG_FUNCTION_ARGS);
// PG_FUNCTION_INFO_V1(Anchor_end);
// Datum Anchor_end(PG_FUNCTION_ARGS)
// {
//   Periodic *per = PG_GETARG_PERIODIC_P(0);
//   PMode *pmode = PG_GETARG_PMODE_P(1);
//   TimestampTz start_tstz = PG_GETARG_TIMESTAMPTZ(2);
//   TimestampTz end_tstz = PG_GETARG_TIMESTAMPTZ(3);
//   PG_RETURN_POINTER(anchor(per, pmode, start_tstz, end_tstz, false));
// }

// PGDLLEXPORT Datum Anchor_end_inc(PG_FUNCTION_ARGS);
// PG_FUNCTION_INFO_V1(Anchor_end_inc);
// Datum Anchor_end_inc(PG_FUNCTION_ARGS)
// {
//   Periodic *per = PG_GETARG_PERIODIC_P(0);
//   PMode *pmode = PG_GETARG_PMODE_P(1);
//   TimestampTz start_tstz = PG_GETARG_TIMESTAMPTZ(2);
//   TimestampTz end_tstz = PG_GETARG_TIMESTAMPTZ(3);
//   bool upper_inc = PG_GETARG_TIMESTAMPTZ(4);
//   PG_RETURN_POINTER(anchor(per, pmode, start_tstz, end_tstz, upper_inc));
// }




// TODO MODIFY WITH OID LOGIC LATER,
// TODO PERHAPS DO SOME SORT OF MATCHING TO MEOSTYPE 
PGDLLEXPORT Datum Periodic_int_in(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(Periodic_int_in);
Datum Periodic_int_in(PG_FUNCTION_ARGS)
{
  const char *input = PG_GETARG_CSTRING(0);
  meosType subtype = T_TINT;
  Periodic *result = periodic_in(input, subtype);
  PG_RETURN_POINTER(result);
}


// PGDLLEXPORT Datum Periodic_in(PG_FUNCTION_ARGS);
// PG_FUNCTION_INFO_V1(Periodic_in);
// Datum Periodic_in(PG_FUNCTION_ARGS)
// {
//   const char *input = PG_GETARG_CSTRING(0);
//   Oid temptypid = PG_GETARG_OID(1); 
//   Temporal *result = temporal_in(input, oid_type(temptypid));
//   int32 temp_typmod = -1;
//   if (PG_NARGS() > 2 && !PG_ARGISNULL(2))
//     temp_typmod = PG_GETARG_INT32(2);
//   if (temp_typmod >= 0)
//     result = temporal_valid_typmod_temp(result, temp_typmod);
//   PG_RETURN_POINTER(result);
// }


PGDLLEXPORT Datum Periodic_out(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(Periodic_out);
Datum Periodic_out(PG_FUNCTION_ARGS)
{
  Periodic *per = PG_GETARG_PERIODIC_P(0);
  // char *result = temporal_out(temp, OUT_DEFAULT_DECIMAL_DIGITS);
  char *result = periodic_out(per, OUT_DEFAULT_DECIMAL_DIGITS);
  PG_FREE_IF_COPY(per, 0);
  PG_RETURN_CSTRING(result);
}


/*****************************************************************************
  Setters
*****************************************************************************/

char * _perType_names[] =
{
  [P_NONE] = "none",
  [P_DAY] = "day",
  [P_WEEK] = "week",
  [P_MONTH] = "month",
  [P_YEAR] = "year"
};

#define PERTYPE_STR_MAX_LEN 5

perType
pertype_from_string(const char *pertype_str)
{
  int n = sizeof(_perType_names) / sizeof(char *);
  for (int i = 0; i < n; i++)
  {
    if (pg_strncasecmp(pertype_str, _perType_names[i], PERTYPE_STR_MAX_LEN) == 0)
      return i;
  }
  /* Error */
  elog(ERROR, "Unknown periodic type: %s", pertype_str);
  return P_NONE; /* make compiler quiet */
}

PGDLLEXPORT Datum Periodic_set_type(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(Periodic_set_type);
Datum Periodic_set_type(PG_FUNCTION_ARGS)
{
  Periodic *per = PG_GETARG_PERIODIC_P(0);
  text *ptype_txt = PG_GETARG_TEXT_P(1);
  char *ptype_str = text2cstring(ptype_txt);
  perType ptype =  pertype_from_string(ptype_str);
  pfree(ptype_str);
  Periodic *result = periodic_set_pertype(per, ptype);
  PG_FREE_IF_COPY(per, 0);
  PG_RETURN_POINTER(result);
}


/*****************************************************************************
  Getters
*****************************************************************************/

PGDLLEXPORT Datum Periodic_get_type(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(Periodic_get_type);
Datum Periodic_get_type(PG_FUNCTION_ARGS)
{
  Periodic *per = PG_GETARG_PERIODIC_P(0);
  char *str = periodic_get_pertype(per);
  text *result = cstring_to_text(str);
  pfree(str);
  PG_FREE_IF_COPY(per, 0);
  PG_RETURN_TEXT_P(result);
}


/*****************************************************************************
  Casting
*****************************************************************************/


// PGDLLEXPORT Datum Tint_to_pint(PG_FUNCTION_ARGS);
// PG_FUNCTION_INFO_V1(Tint_to_pint);
// Datum Tint_to_pint(PG_FUNCTION_ARGS)
// {
//   Temporal *temp = PG_GETARG_TEMPORAL_P(0);
//   Periodic *result = tint_to_pint(temp);
//   PG_FREE_IF_COPY(temp, 0);
//   PG_RETURN_CSTRING(result);
// }

// PGDLLEXPORT Datum Pint_to_tint(PG_FUNCTION_ARGS);
// PG_FUNCTION_INFO_V1(Pint_to_tint);
// Datum Pint_to_tint(PG_FUNCTION_ARGS)
// {
//   Periodic *temp = PG_GETARG_PERIODIC_P(0);
//   Temporal *result = pint_to_tint(temp);
//   PG_FREE_IF_COPY(temp, 0);
//   PG_RETURN_POINTER(result);
// }

/*****************************************************************************
  OTHER
*****************************************************************************/

PGDLLEXPORT Datum Quick_test(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(Quick_test);
Datum Quick_test(PG_FUNCTION_ARGS)
{
  TimestampTz tstz = PG_GETARG_TIMESTAMPTZ(0);
  text *format = PG_GETARG_TEXT_P(1);
  
  char *fmt_str = text_to_cstring(format);

  char *result_str = format_timestamptz(tstz, fmt_str);
  // char *result_str = "42";
  
  text *result = cstring_to_text(result_str);
  // pfree(result_str);

  PG_RETURN_TEXT_P(result);
}






/*****************************************************************************
 *  Composition Test (FIXME TEMPORARY)
*****************************************************************************/

// PGDLLEXPORT Datum R_int_in(PG_FUNCTION_ARGS);
// PG_FUNCTION_INFO_V1(R_int_in);
// Datum R_int_in(PG_FUNCTION_ARGS)
// {
//   const char *input = PG_GETARG_CSTRING(0);
//   meosType subtype = T_TINT;
//   RSequence *result = repeat_in(input, subtype);
//   PG_RETURN_POINTER(result);   
// }

// PGDLLEXPORT Datum R_int_out(PG_FUNCTION_ARGS);
// PG_FUNCTION_INFO_V1(R_int_out);
// Datum R_int_out(PG_FUNCTION_ARGS)
// {
//   RSequence *per = PG_GETARG_PERIODIC_P(0);
//   char *result = repeat_out(per, OUT_DEFAULT_DECIMAL_DIGITS);
//   PG_FREE_IF_COPY(per, 0);
//   PG_RETURN_CSTRING(result);
// }

// PGDLLEXPORT Datum Distance_rnumber_number(PG_FUNCTION_ARGS);
// PG_FUNCTION_INFO_V1(Distance_rnumber_number);
// Datum Distance_rnumber_number(PG_FUNCTION_ARGS)
// {
//   RSequence *temp = PG_GETARG_RSEQUENCE_P(0);
//   Datum value = PG_GETARG_DATUM(1);
//   RSequence *result = r_distance(temp, value);
//   PG_FREE_IF_COPY(temp, 0);
//   PG_RETURN_POINTER(result);
// }


