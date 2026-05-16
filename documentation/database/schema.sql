-- table: queue_benchmark

CREATE TABLE queue_type (                                                       -- queue_type
    id SERIAL PRIMARY KEY,
    name VARCHAR(50) UNIQUE NOT NULL,                                           -- data strcuture names
    description TEXT
);

CREATE TABLE benchmark_run (                                                    -- Each benchmark test session (one config of thread count/op type/env)
    id SERIAL PRIMARY KEY,
    queue_type_id INTEGER REFERENCES queue_type(id),
    operation VARCHAR(50) NOT NULL,           -- e.g., 'push', 'pop', 'mixed'
    thread_count INTEGER NOT NULL,
    run_timestamp TIMESTAMP DEFAULT NOW(),
    notes TEXT
);

-- Table: benchmark_result
CREATE TABLE benchmark_result (                                                 -- Results collected for each run; you may want to aggregate by op count etc.
    id SERIAL PRIMARY KEY,
    run_id INTEGER REFERENCES benchmark_run(id) ON DELETE CASCADE,
    operations_performed BIGINT NOT NULL,
    throughput_ops_per_sec DOUBLE PRECISION NOT NULL,
    avg_latency_usec DOUBLE PRECISION,
    min_latency_usec DOUBLE PRECISION,
    max_latency_usec DOUBLE PRECISION,
    measurement_interval_sec DOUBLE PRECISION -- How long this sample covered
);


CREATE TABLE operation_times (                                                  --Stores per-operation latency measurements
    id SERIAL PRIMARY KEY,
    result_id INTEGER REFERENCES benchmark_result(id) ON DELETE CASCADE,
    operation_index BIGINT,
    latency_usec DOUBLE PRECISION
);

-- Insert sample queue types
INSERT INTO queue_type(name, description) VALUES 
('lock-free', 'Lock-free multi-producer multi-consumer queue'),
('lock-based', 'Lock-based multi-producer multi-consumer queue');