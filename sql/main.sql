
--DROP SCHEMA if exists gwu18 CASCADE;

CREATE SCHEMA gwu18;

CREATE TABLE gwu18.config
(
  app_class character varying (32) NOT NULL,
  db_public character varying (256) NOT NULL,
  udp_port character varying (32) NOT NULL,
  pid_path character varying (32) NOT NULL,
  udp_buf_size character varying (32) NOT NULL, --size of buffer used in sendto() and recvfrom() functions (508 is safe, if more then packet may be lost while transferring over network). Enlarge it if your rio module returns SRV_BUF_OVERFLOW
  db_data character varying (32) NOT NULL,
  db_log character varying (32) NOT NULL,
  cycle_duration_us character varying(32) NOT NULL,--the more cycle duration is, the less your hardware is loaded
  CONSTRAINT config_pkey PRIMARY KEY (app_class)
)
WITH (
  OIDS=FALSE
);

CREATE TABLE gwu18.device
(
  app_class character varying(32) NOT NULL,
  id integer NOT NULL,--this id will be used for interprocess communication
  pin integer NOT NULL,-- any GPIO pin (use physical address)
  addr character(16) NOT NULL,-- you can get it using 1wgr utility from control.tar.gz
  resolution integer NOT NULL,-- 9 || 10 || 11 || 12
  CONSTRAINT device_pkey PRIMARY KEY (app_class, id),
  CONSTRAINT device_app_class_pin_addr_key UNIQUE (app_class, pin, addr)
)
WITH (
  OIDS=FALSE
);



