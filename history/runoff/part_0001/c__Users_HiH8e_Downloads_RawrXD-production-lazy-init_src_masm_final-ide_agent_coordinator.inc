; ============================================================================
; AGENT_COORDINATOR.INC - Multi-Agent Orchestration Header
; ============================================================================

EXTERN coordinator_init:PROC
EXTERN coordinator_shutdown:PROC
EXTERN register_agent:PROC
EXTERN unregister_agent:PROC
EXTERN create_task:PROC
EXTERN queue_task:PROC
EXTERN delegate_to_agent:PROC
EXTERN auto_delegate:PROC
EXTERN monitor_execution:PROC
EXTERN collect_results:PROC
EXTERN handle_failure:PROC
EXTERN sync_agents:PROC
EXTERN get_coordinator_stats:PROC
EXTERN cancel_task:PROC
EXTERN requeue_failed_task:PROC
EXTERN set_resource_limits:PROC

TASK STRUCT
    task_id dword
    agent_id dword
    priority dword
    status dword
    request_data qword
    result_data qword
    request_size dword
    result_size dword
    error_message qword
    created_at dword
    started_at dword
    completed_at dword
    auto_retry byte
TASK ENDS

METRICS STRUCT
    total_tasks_processed qword
    total_latency_ms qword
    avg_queue_depth dword
    agents_idle dword
    agents_busy dword
    failure_rate_percent dword
METRICS ENDS
