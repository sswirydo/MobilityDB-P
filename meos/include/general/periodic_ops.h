/**
  DISCLAIMER: The below code is proof of concept and is not intended for production use.
              It is still in developped in the context of a master thesis.
*/

#ifndef __PERIODIC_OPS_H__
#define __PERIODIC_OPS_H__


/*****************************************************************************
 *  Operations
*****************************************************************************/

Temporal *anchor_pmode(const Periodic *per, PMode *pmode);
Temporal *anchor(const Temporal *periodic, const Span *ts_anchor, const Interval *frequency, const bool strict_pattern);
Temporal *anchor_array(const Temporal *periodic, const Span *ts_anchor, const Interval *frequency, const bool strict_pattern, const Datum *service_array, const int array_count);

Periodic *periodic_align(const Periodic *per, const Timestamp ts);
bool periodic_value_at_timestamptz(
  const Periodic *per, 
  const Span *anchor_ts, 
  const Interval *frequency,
  TimestampTz tstz, 
  bool strict, 
  Datum *result);

// bool periodic_value_at_timestamptz(const Periodic *per, PMode *pmode, TimestampTz tstz, bool strict, Datum *result);
// extern bool temporal_value_at_timestamptz(const Temporal *temp, TimestampTz t, bool strict, Datum *result);



#endif /* __PERIODIC_OPS_H__ */