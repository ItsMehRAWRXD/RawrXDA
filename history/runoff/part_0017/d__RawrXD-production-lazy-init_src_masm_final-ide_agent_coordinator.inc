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
    task_id DWORD ?
    agent_id DWORD ?
    priority DWORD ?
    status DWORD ?
    request_data QWORD ?
    result_data QWORD ?
    request_size DWORD ?
    result_size DWORD ?
    error_message QWORD ?
    created_at DWORD ?
    started_at DWORD ?
    completed_at DWORD ?
    auto_retry BYTE ?
TASK ENDS

METRICS STRUCT
    total_tasks_processed QWORD ?
    total_latency_ms QWORD ?
    avg_queue_depth DWORD ?
    agents_idle DWORD ?
    agents_busy DWORD ?
    failure_rate_percent DWORD ?
METRICS ENDS
