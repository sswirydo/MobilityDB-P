/**
  DISCLAIMER: The below code is proof of concept and is not intended for production use.
              It is still in developped in the context of a master thesis.
*/

#ifndef __PERIODIC_PARSER_H__
#define __PERIODIC_PARSER_H__


Periodic *periodic_parse(const char **str, meosType temptype);

bool pcontseq_parse(const char **str, meosType temptype, perType pertype, interpType interp, bool end, PSequence **result);
bool pinstant_parse(const char **str, meosType temptype, perType pertype, bool end, PInstant **result);
bool periodic_basetype_parse(const char **str, meosType basetype, Datum *result);
TimestampTz periodic_timestamp_parse(const char **str, perType pertype);
PSequence* normalize_periodic_sequence(PSequence *pseq);


#endif /* __PERIODIC_PARSER_H__ */