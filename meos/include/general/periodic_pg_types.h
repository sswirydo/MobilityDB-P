/**
  DISCLAIMER: The below code is proof of concept and is not intended for production use.
              It is still in developped in the context of a master thesis.
*/

#ifndef __PERIODIC_PG_TYPES_H__
#define __PERIODIC_PG_TYPES_H__

/* PostgreSQL */
#include <postgres.h>
#include <utils/timestamp.h>

text *pg_timestamptz_to_char(TimestampTz dt, text *fmt);
Interval *pg_interval_in(const char *str, int32 typmod);
TimestampTz pg_to_timestamp(text *date_txt, text *fmt);


#endif /* __PERIODIC_PG_TYPES_H__ */