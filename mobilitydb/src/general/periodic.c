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

PGDLLEXPORT Datum PMode_send(PG_FUNCTION_ARGS); // todo update later
PG_FUNCTION_INFO_V1(PMode_send);
Datum PMode_send(PG_FUNCTION_ARGS)
{
  PMode *pmode = PG_GETARG_PMODE_P(0);
  StringInfoData buffer;
  pq_begintypsend(&buffer);
  pq_sendint64(&buffer, (uint64) pmode->repetitions);
  PG_FREE_IF_COPY(pmode, 0);
  PG_RETURN_BYTEA_P(pq_endtypsend(&buffer));
}

PGDLLEXPORT Datum PMode_recv(PG_FUNCTION_ARGS); // todo update later
PG_FUNCTION_INFO_V1(PMode_recv);
Datum PMode_recv(PG_FUNCTION_ARGS)
{
  StringInfo buffer = (StringInfo) PG_GETARG_POINTER(0);
  PMode *pmode = (PMode *) palloc(sizeof(PMode));
  pmode->repetitions = pq_getmsgint64(buffer);
  PG_RETURN_PMODE_P(pmode);
}

PGDLLEXPORT Datum PMode_constructor(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(PMode_constructor);
Datum PMode_constructor(PG_FUNCTION_ARGS)
{
  Interval* frequency = PG_GETARG_INTERVAL_P(0);
  int repetitions = PG_GETARG_INT32(1);
  PG_RETURN_PMODE_P(pmode_make(frequency, repetitions));
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

// PGDLLEXPORT Datum Periodic_generate(PG_FUNCTION_ARGS);
// PG_FUNCTION_INFO_V1(Periodic_generate);
// Datum Periodic_generate(PG_FUNCTION_ARGS)
// {
//   Periodic *per = PG_GETARG_PERIODIC_P(0);
//   PMode *pmode = PG_GETARG_PMODE_P(1);
//   PG_RETURN_POINTER(periodic_generate(per, pmode));
// }


PGDLLEXPORT Datum Temporal_make_periodic(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(Temporal_make_periodic);
Datum Temporal_make_periodic(PG_FUNCTION_ARGS)
{
  Temporal *temp = PG_GETARG_TEMPORAL_P(0);
  PMode *pmode = PG_GETARG_PMODE_P(1);
  PG_RETURN_POINTER(temporal_make_periodic(temp, pmode));
}


// TODO MODIFY WITH OID LOGIC LATER,
// TODO PERHAPS DO SOME SORT OF MATCHING TO MEOSTYPE 
PGDLLEXPORT Datum Periodic_int_in(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(Periodic_int_in);
Datum Periodic_int_in(PG_FUNCTION_ARGS)
{
  const char *input = PG_GETARG_CSTRING(0);
  meosType subtype = T_TINT;
  Periodic *result = periodic_in(input, subtype); // fixme keeping temporal input for now
  
  // FIXME TYPMOD
  // int32 temp_typmod = -1;
  // if (PG_NARGS() > 2 && !PG_ARGISNULL(2))
  //   temp_typmod = PG_GETARG_INT32(2);
  // if (temp_typmod >= 0)
  //   result = temporal_valid_typmod_temp(result, temp_typmod);

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


PGDLLEXPORT Datum Tint_to_pint(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(Tint_to_pint);
Datum Tint_to_pint(PG_FUNCTION_ARGS)
{
  Temporal *temp = PG_GETARG_TEMPORAL_P(0);
  Periodic *result = tint_to_pint(temp);
  PG_FREE_IF_COPY(temp, 0);
  PG_RETURN_CSTRING(result);
}

PGDLLEXPORT Datum Pint_to_tint(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(Pint_to_tint);
Datum Pint_to_tint(PG_FUNCTION_ARGS)
{
  Periodic *temp = PG_GETARG_PERIODIC_P(0);
  Temporal *result = pint_to_tint(temp);
  PG_FREE_IF_COPY(temp, 0);
  PG_RETURN_POINTER(result);
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
  
  char *fmt_str = text_to_cstring(format);

  char *result_str = format_timestamptz(tstz, P_YEAR);
  
  text *result = cstring_to_text(result_str);
  pfree(result_str);

  PG_RETURN_TEXT_P(result);
}





/*

  TEMPORARY
  COPIES FOR REFERNECE

*/


// typedef enum
// {
//   T_UNKNOWN        = 0,   /**< unknown type */
//   T_BOOL           = 1,   /**< boolean type */
//   T_DOUBLE2        = 2,   /**< double2 type */
//   T_DOUBLE3        = 3,   /**< double3 type */
//   T_DOUBLE4        = 4,   /**< double4 type */
//   T_FLOAT8         = 5,   /**< float8 type */
//   T_FLOATSET       = 6,   /**< float8 set type */
//   T_FLOATSPAN      = 7,   /**< float8 span type */
//   T_FLOATSPANSET   = 8,   /**< float8 span set type */
//   T_INT4           = 9,   /**< int4 type */
//   T_INT4RANGE      = 10,  /**< PostgreSQL int4 range type */
//   T_INT4MULTIRANGE = 11,  /**< PostgreSQL int4 multirange type */
//   T_INTSET         = 12,  /**< int4 set type */
//   T_INTSPAN        = 13,  /**< int4 span type */
//   T_INTSPANSET     = 14,  /**< int4 span set type */
//   T_INT8           = 15,  /**< int8 type */
//   T_BIGINTSET      = 16,  /**< int8 set type */
//   T_BIGINTSPAN     = 17,  /**< int8 span type */
//   T_BIGINTSPANSET  = 18,  /**< int8 span set type */
//   T_STBOX          = 19,  /**< spatiotemporal box type */
//   T_TBOOL          = 20,  /**< temporal boolean type */
//   T_TBOX           = 21,  /**< temporal box type */
//   T_TDOUBLE2       = 22,  /**< temporal double2 type */
//   T_TDOUBLE3       = 23,  /**< temporal double3 type */
//   T_TDOUBLE4       = 24,  /**< temporal double4 type */
//   T_TEXT           = 25,  /**< text type */
//   T_TEXTSET        = 26,  /**< text type */
//   T_TFLOAT         = 27,  /**< temporal float type */
//   T_TIMESTAMPTZ    = 28,  /**< timestamp with time zone type */
//   T_TINT           = 29,  /**< temporal integer type */
//   T_TSTZMULTIRANGE = 30,  /**< PostgreSQL timestamp with time zone multirange type */
//   T_TSTZRANGE      = 31,  /**< PostgreSQL timestamp with time zone range type */
//   T_TSTZSET        = 32,  /**< timestamptz set type */
//   T_TSTZSPAN       = 33,  /**< timestamptz span type */
//   T_TSTZSPANSET    = 34,  /**< timestamptz span set type */
//   T_TTEXT          = 35,  /**< temporal text type */
//   T_GEOMETRY       = 36,  /**< geometry type */
//   T_GEOMSET        = 37,  /**< geometry set type */
//   T_GEOGRAPHY      = 38,  /**< geography type */
//   T_GEOGSET        = 39,  /**< geography set type */
//   T_TGEOMPOINT     = 40,  /**< temporal geometry point type */
//   T_TGEOGPOINT     = 41,  /**< temporal geography point type */
//   T_NPOINT         = 42,  /**< network point type */
//   T_NPOINTSET      = 43,  /**< network point set type */
//   T_NSEGMENT       = 44,  /**< network segment type */
//   T_TNPOINT        = 45,  /**< temporal network point type */
// } meosType_COPY;

// const char *_meosType_names_COPY[] =
// {
//   [T_UNKNOWN] = "",
//   [T_BOOL] = "bool",
//   [T_DOUBLE2] = "double2",
//   [T_DOUBLE3] = "double3",
//   [T_DOUBLE4] = "double4",
//   [T_FLOAT8] = "float8",
//   [T_FLOATSET] = "floatset",
//   [T_FLOATSPAN] = "floatspan",
//   [T_FLOATSPANSET] = "floatspanset",
//   [T_INT4] = "int4",
//   [T_INT4RANGE] = "int4range",
//   [T_INT4MULTIRANGE] = "int4multirange",
//   [T_INTSET] = "intset",
//   [T_INTSPAN] = "intspan",
//   [T_INTSPANSET] = "intspanset",
//   [T_INT8] = "int8",
//   [T_BIGINTSET] = "bigintset",
//   [T_BIGINTSPAN] = "bigintspan",
//   [T_BIGINTSPANSET] = "bigintspanset",
//   [T_STBOX] = "stbox",
//   [T_TBOOL] = "tbool",
//   [T_TBOX] = "tbox",
//   [T_TDOUBLE2] = "tdouble2",
//   [T_TDOUBLE3] = "tdouble3",
//   [T_TDOUBLE4] = "tdouble4",
//   [T_TEXT] = "text",
//   [T_TEXTSET] = "textset",
//   [T_TFLOAT] = "tfloat",
//   [T_TIMESTAMPTZ] = "timestamptz",
//   [T_TINT] = "tint",
//   [T_TSTZMULTIRANGE] = "tstzmultirange",
//   [T_TSTZRANGE] = "tstzrange",
//   [T_TSTZSET] = "tstzset",
//   [T_TSTZSPAN] = "tstzspan",
//   [T_TSTZSPANSET] = "tstzspanset",
//   [T_TTEXT] = "ttext",
//   [T_GEOMETRY] = "geometry",
//   [T_GEOMSET] = "geomset",
//   [T_GEOGRAPHY] = "geography",
//   [T_GEOGSET] = "geogset",
//   [T_TGEOMPOINT] = "tgeompoint",
//   [T_TGEOGPOINT] = "tgeogpoint",
//   [T_NPOINT] = "npoint",
//   [T_NPOINTSET] = "npointset",
//   [T_NSEGMENT] = "nsegment",
//   [T_TNPOINT] = "tnpoint",
// };

// meosType
// oid_type_COPY(Oid typid)
// {
//   if (!_oid_cache_ready)
//     populate_operoid_cache();
//   int n = sizeof(_meosType_names) / sizeof(char *);
//   for (int i = 0; i < n; i++)
//   {
//     if (_type_oids[i] == typid)
//       return i;
//   }
//   return T_UNKNOWN;
// }

