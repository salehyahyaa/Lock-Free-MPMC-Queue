-- Remove all benchmark measurement rows. Keeps `queue_type` seed rows intact.
-- Run from repo root, e.g.:
--   psql "dbname=queue_benchmark user=YOUR_USER host=localhost port=5432" -f documentation/clear_benchmark_data.sql

TRUNCATE TABLE benchmark_run RESTART IDENTITY CASCADE;
