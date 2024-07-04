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

Periodic *ppoint_parse(const char **str, meosType temptype);
bool ppointinst_parse(const char **str, meosType temptype, perType pertype, bool end, int *tpoint_srid, PInstant **result);
bool ppointcontseq_parse(const char **str, meosType temptype, perType pertype, interpType interp, bool end, int *tpoint_srid, PSequence **result);
PSequenceSet *ppointseqset_parse(const char **str, meosType temptype, perType pertype, interpType interp, int *tpoint_srid);
PSequence * ppointdiscseq_parse(const char **str, meosType temptype, perType pertype, int *tpoint_srid);


#endif /* __PERIODIC_PARSER_H__ */