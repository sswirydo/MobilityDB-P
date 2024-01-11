/**
  DISCLAIMER: The below code is proof of concept and is not intended for production use.
              It is still in developped in the context of a master thesis.
*/

/**
  DISCLAIMER: The below code is proof of concept and is not intended for production use.
              It is still in developped in the context of a master thesis.
*/

CREATE TYPE pmode;

/*****************************************************************************
 *  PMode
*****************************************************************************/

CREATE FUNCTION pmode_in(cstring)
  RETURNS pmode
  AS 'MODULE_PATHNAME', 'PMode_in'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION pmode_out(pmode)
  RETURNS cstring
  AS 'MODULE_PATHNAME', 'PMode_out'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

-- CREATE FUNCTION pmode_send(pmode)
--   RETURNS bytea
--   AS 'MODULE_PATHNAME', 'PMode_send'
--   LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

-- CREATE FUNCTION pmode_recv(internal)
--   RETURNS pmode
--   AS 'MODULE_PATHNAME', 'PMode_recv'
--   LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE TYPE pmode (
    internallength  = 64, --variable, --FIXME
    input           = pmode_in,
    output          = pmode_out,
    -- send            = pmode_send,
    -- receive         = pmode_recv,
    alignment       = double
);
    -- typmod_in = temporal_typmod_in,
    -- typmod_out = temporal_typmod_out,
    -- storage = extended,
    -- analyze = temporal_analyze
-- );


-- CREATE FUNCTION pmode(interval, integer) -- frequency, repetitions
--   RETURNS pmode
--   AS 'MODULE_PATHNAME', 'PMode_constructor'
--   LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

-- -- frequency, repetitions, start_date, end_date, upper_inc, keep_pattern
-- CREATE FUNCTION pmode(interval, integer, timestamptz, timestamptz, boolean, boolean) 
--   RETURNS pmode
--   AS 'MODULE_PATHNAME', 'PMode_constructor'
--   LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION pmode(interval, integer, boolean, tstzspan) 
  RETURNS pmode
  AS 'MODULE_PATHNAME', 'PMode_constructor'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;



/*****************************************************************************
 *  Periodic
*****************************************************************************/



CREATE TYPE pint;

-- CREATE FUNCTION pint_in(cstring, oid, integer)
CREATE FUNCTION pint_in(cstring)
  RETURNS pint
  AS 'MODULE_PATHNAME', 'Periodic_int_in'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION periodic_out(pint)
  RETURNS cstring
  AS 'MODULE_PATHNAME', 'Periodic_out'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

-- CREATE FUNCTION periodic_send(pint)
--   RETURNS bytea
--   AS 'MODULE_PATHNAME', 'Periodic_send'
--   LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

-- CREATE FUNCTION pint_recv(internal, oid, integer)
--   RETURNS pint
--   AS 'MODULE_PATHNAME', 'Periodic_recv'
--   LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE TYPE pint (
    internallength  = variable,
    input           = pint_in,
    output          = periodic_out,
    -- send            = periodic_send,
    -- receive         = pint_recv,
    storage         = extended,
    alignment       = double
);



-- CREATE FUNCTION pint(tint)
--   RETURNS pint
--   AS 'MODULE_PATHNAME', 'Tint_to_pint'
--   LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;
-- CREATE FUNCTION tint(pint)
--   RETURNS tint
--   AS 'MODULE_PATHNAME', 'Pint_to_tint'
--   LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

-- -- https://www.postgresql.org/docs/current/sql-createcast.html
-- CREATE CAST (pint as tint) WITH FUNCTION tint(pint);
-- CREATE CAST (tint as pint) WITH FUNCTION pint(tint);


/*****************************************************************************
 * Periodic type / flags
*****************************************************************************/

CREATE FUNCTION periodicType(pint)
  RETURNS text
  AS 'MODULE_PATHNAME', 'Periodic_get_type'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

CREATE FUNCTION setPeriodicType(pint, text)
  RETURNS pint
  AS 'MODULE_PATHNAME', 'Periodic_set_type'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;



/*****************************************************************************
 *  Periodicity -- todo add for other basetypes once base works
*****************************************************************************/


CREATE FUNCTION anchor(pint, pmode)
  RETURNS tint
  AS 'MODULE_PATHNAME', 'Anchor'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;


-- CREATE FUNCTION anchor(pint, pmode, timestamptz)
--   RETURNS tint
--   AS 'MODULE_PATHNAME', 'Anchor'
--   LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

-- CREATE FUNCTION anchor(pint, pmode, timestamptz, timestamptz)
--   RETURNS tint
--   AS 'MODULE_PATHNAME', 'Anchor_end'
--   LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

-- CREATE FUNCTION anchor(pint, pmode, timestamptz, timestamptz, boolean)
--   RETURNS tint
--   AS 'MODULE_PATHNAME', 'Anchor_end_inc'
--   LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;




/*****************************************************************************
 *  Other
*****************************************************************************/

CREATE FUNCTION quick_test(timestamptz, text)
  RETURNS text
  AS 'MODULE_PATHNAME', 'Quick_test'
  LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;




/*****************************************************************************
 *  Composition Test (FIXME TEMPORARY)
*****************************************************************************/

-- CREATE TYPE rint;

-- CREATE FUNCTION rint_in(cstring)
--   RETURNS rint
--   AS 'MODULE_PATHNAME', 'R_int_in'
--   LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

-- CREATE FUNCTION rint_out(rint)
--   RETURNS cstring
--   AS 'MODULE_PATHNAME', 'R_int_out'
--   LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

-- CREATE TYPE rint (
--     internallength  = variable,
--     input           = rint_in,
--     output          = rint_out,
--     storage         = extended,
--     alignment       = double
-- );

-- -- CREATE FUNCTION r_distance(rint)
-- --   RETURNS cstring
-- --   AS 'MODULE_PATHNAME', 'Distance_tnumber_number'
-- --   LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;

-- CREATE FUNCTION rnumber_distance(rint, integer)
--   RETURNS rint
--   AS 'MODULE_PATHNAME', 'Distance_rnumber_number'
--   LANGUAGE C IMMUTABLE STRICT PARALLEL SAFE;
