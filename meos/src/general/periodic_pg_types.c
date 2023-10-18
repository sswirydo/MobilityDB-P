
/**
  DISCLAIMER: The below code is proof of concept and is not intended for production use.
              It is still in developped in the context of a master thesis.
*/

#include "general/periodic_pg_types.h"
#include "general/temporal.h"


/* C */
#include <float.h>
#include <math.h>
#include <limits.h>
/* PostgreSQL */
#include <postgres.h>
#include <common/int128.h>
#include <utils/datetime.h>
#include <utils/float.h>
#if POSTGRESQL_VERSION_NUMBER >= 130000
  #include <common/hashfn.h>
#else
  #include <access/hash.h>
#endif
#if POSTGRESQL_VERSION_NUMBER >= 160000
  #include "varatt.h"
#endif
/* PostGIS */
#include <liblwgeom_internal.h> /* for OUT_DOUBLE_BUFFER_SIZE */

/* MEOS */
#include <meos.h>
#include <meos_internal.h>
#include "general/type_util.h"




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
  extern Datum to_timestamp(PG_FUNCTION_ARGS);

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
    Datum arg2 = Int32GetDatum(DEFAULT_COLLATION_OID); // Not used but required for PG_FUNCTION_ARGS.
    Datum arg3 = Int32GetDatum(typmod);
    Interval *result = DatumGetIntervalP(call_function3(interval_in, arg1, arg2, arg3));
    return result;
  }

  TimestampTz 
  pg_to_timestamp(text *date_txt, text *fmt)
  {
    Datum arg1 = PointerGetDatum(date_txt);
    Datum arg2 = PointerGetDatum(fmt);
    TimestampTz result = DatumGetTimestampTz(call_function2(to_timestamp, arg1, arg2));
    return result;
  }

#endif /* ! MEOS */






