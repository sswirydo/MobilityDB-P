/**
  DISCLAIMER: The below code is proof of concept and is not intended for production use.
              It is still in developped in the context of a master thesis.
*/


/* Periodic */
#include "pg_general/periodic.h"
#include "general/periodic.h"
#include "general/periodic_parser.h"

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



// TODO MODIFY WITH OID LOGIC LATER,
// TODO PERHAPS DO SOME SORT OF MATCHING TO MEOSTYPE
// TODO -> PROBABLY JUST ADD TYPE AS FLAG INSTEAD
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




PGDLLEXPORT Datum Ppoint_in(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(Ppoint_in);
Datum
Ppoint_in(PG_FUNCTION_ARGS)
{
  const char *input = PG_GETARG_CSTRING(0);
  meosType subtype = T_TGEOMPOINT; //fixme
  PG_RETURN_TEMPORAL_P(ppoint_parse(&input, subtype));
}


/* Note: there is no difference with Tsequence_constructor */
/* TODO Perhaps add an argument for perType */
PGDLLEXPORT Datum Psequence_constructor(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(Psequence_constructor);
Datum
Psequence_constructor(PG_FUNCTION_ARGS)
{
  ArrayType *array = PG_GETARG_ARRAYTYPE_P(0);
  ensure_not_empty_array(array);
  meosType temptype = oid_type(get_fn_expr_rettype(fcinfo->flinfo));
  interpType interp = temptype_continuous(temptype) ? LINEAR : STEP;
  if (PG_NARGS() > 1 && !PG_ARGISNULL(1))
  {
    text *interp_txt = PG_GETARG_TEXT_P(1);
    char *interp_str = text2cstring(interp_txt);
    interp = interptype_from_string(interp_str);
    pfree(interp_str);
  }
  bool lower_inc = true, upper_inc = true;
  if (PG_NARGS() > 2 && !PG_ARGISNULL(2))
    lower_inc = PG_GETARG_BOOL(2);
  if (PG_NARGS() > 3 && !PG_ARGISNULL(3))
    upper_inc = PG_GETARG_BOOL(3);
  int count;
  PInstant **instants = (PInstant **) temparr_extract(array, &count);
  PSequence *result = (PSequence *) tsequence_make((const TInstant **) instants, count,
    lower_inc, upper_inc, interp, NORMALIZE);
  pfree(instants);
  PG_FREE_IF_COPY(array, 0);
  PG_RETURN_TSEQUENCE_P(result);
}






/*****************************************************************************
  Setters
*****************************************************************************/

static const char *_perType_names[] =
{
  [P_NONE] = "none",
  [P_DEFAULT] = "default",
  [P_DAY] = "day",
  [P_WEEK] = "week",
  [P_MONTH] = "month",
  [P_YEAR] = "year",
  [P_INTERVAL] = "interval"
};

#define PERTYPE_STR_MAX_LEN 5

perType
pertype_from_string(const char *pertype_str)
{
  size_t n = sizeof(_perType_names) / sizeof(char *);
  for (size_t i = 0; i < n; i++)
  {
    if (pg_strncasecmp(pertype_str, _perType_names[i], PERTYPE_STR_MAX_LEN) == 0) {
      return (perType) i;
    }
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
  PG_RETURN_PERIODIC_P(result);
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
  text *result = cstring2text(str);
  pfree(str);
  PG_FREE_IF_COPY(per, 0);
  PG_RETURN_TEXT_P(result);
}


/*****************************************************************************
  Casting
*****************************************************************************/

/* fixme later (merge both) */
PGDLLEXPORT Datum Periodic_to_temporal(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(Periodic_to_temporal);
Datum Periodic_to_temporal(PG_FUNCTION_ARGS)
{
  Periodic *per = PG_GETARG_PERIODIC_P(0);
  Temporal *result = (Temporal*) per;
  // PG_FREE_IF_COPY(per, 0);
  PG_RETURN_TEMPORAL_P(result);
}

PGDLLEXPORT Datum Temporal_to_periodic(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(Temporal_to_periodic);
Datum Temporal_to_periodic(PG_FUNCTION_ARGS)
{
  Temporal *temp = PG_GETARG_TEMPORAL_P(0);
  Periodic *result = (Periodic*) temp;
  // PG_FREE_IF_COPY(temp, 0);
  PG_RETURN_PERIODIC_P(result);
}


/*****************************************************************************
  OTHER
*****************************************************************************/

PGDLLEXPORT Datum Quick_test(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(Quick_test);
Datum Quick_test(PG_FUNCTION_ARGS)
{
  TimestampTz tstz = PG_GETARG_TIMESTAMPTZ(0);
  text *format = PG_GETARG_TEXT_P(1);
  
  char *fmt_str = text2cstring(format);

  char *result_str = format_timestamptz(tstz, fmt_str);
  // char *result_str = "42";
  
  text *result = cstring2text(result_str);
  // pfree(result_str);

  PG_RETURN_TEXT_P(result);
}




/*****************************************************************************
 *  Cyclic
*****************************************************************************/


// PGDLLEXPORT Datum Cyclic_in(PG_FUNCTION_ARGS);
// PG_FUNCTION_INFO_V1(Cyclic_in);
// Datum Cyclic_in(PG_FUNCTION_ARGS)
// {
//   const char *str = PG_GETARG_CSTRING(0);
//   PG_RETURN_CYCLIC_P(cyclic_in(str));
// }

// PGDLLEXPORT Datum Cyclic_out(PG_FUNCTION_ARGS);
// PG_FUNCTION_INFO_V1(Cyclic_out);
// Datum Cyclic_out(PG_FUNCTION_ARGS)
// {
//   Cyclic *cycle = PG_GETARG_CYCLIC_P(0);
//   char *result = cyclic_out(cycle);
//   PG_FREE_IF_COPY_P(cycle, 0);
//   PG_RETURN_CSTRING(result);
// }


// PGDLLEXPORT Datum Cyclic_constructor(PG_FUNCTION_ARGS);
// PG_FUNCTION_INFO_V1(Cyclic_constructor);
// Datum Cyclic_constructor(PG_FUNCTION_ARGS)
// {
//   Periodic *per = PG_GETARG_PERIODIC_P(0);
//   PMode *pmode = PG_GETARG_PMODE_P(1);
//   PG_RETURN_CYCLIC_P(cyclic_make(per, pmode));
// }
