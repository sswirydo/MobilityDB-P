/**
  DISCLAIMER: The below code is proof of concept and is not intended for production use.
              It is still in developped in the context of a master thesis.
*/

#ifndef __PERIODIC_OPS_H__
#define __PERIODIC_OPS_H__


/*****************************************************************************
 *  Operations
*****************************************************************************/

// Temporal *anchor_pmode(const Periodic *per, PMode *pmode);
Temporal *anchor(
  const Temporal *periodic, 
  const Span *ts_anchor, 
  const Interval *period, 
  const bool strict_pattern);
Temporal *anchor_array(
  const Temporal *periodic, 
  const Span *ts_anchor, 
  const Interval *period, 
  const bool strict_pattern, 
  const Datum *service_array, 
  const int array_shift,
  const int array_count);

Periodic *periodic_align(const Periodic *per, const Timestamp ts);

bool periodic_value_at_timestamptz(
  const Periodic *per, 
  const Span *anchor_ts, 
  const Interval *period,
  TimestampTz tstz, 
  bool strict, 
  Datum *result);

Timestamp periodic_timestamptz_to_relative(
  const Timestamp reference_ts, 
  const Interval *period, 
  const TimestampTz tstz);



#endif /* __PERIODIC_OPS_H__ */