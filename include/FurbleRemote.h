#ifndef FURBLE_REMOTE_H
#define FURBLE_REMOTE_H

#include <stdint.h>

namespace Furble {

/**
 * Remote control channel over the USB Serial/JTAG secondary console port.
 *
 * The primary console is UART0, so the USB Serial/JTAG port (registered as
 * /dev/secondary) is free for bidirectional use. The console driver only
 * writes to it (console_read() reads from the primary port), which leaves the
 * receive direction completely idle for command input.
 *
 * The protocol is line based, ASCII, terminated by '\n':
 *
 *   PING              -> ">PONG"
 *   SHUTTER           -> ">OK"          (press + release)
 *   SHUTTER_PRESS     -> ">OK"
 *   SHUTTER_RELEASE   -> ">OK"
 *   FOCUS             -> ">OK"          (press + hold + release)
 *   FOCUS_PRESS       -> ">OK"
 *   FOCUS_RELEASE     -> ">OK"
 *   STATUS            -> ">STATE <n> <camera-name>"
 *   HELP              -> ">CMDS <space separated list>"
 *   anything else     -> ">ERR unknown"
 *
 * 'FOCUS' holds the AF-ON button down long enough for the camera to acquire
 * focus. With the camera set to AF-S this locks the focus point, so a
 * following 'SHUTTER' captures the subject that was focused, not wherever the
 * camera happens to be pointing at that moment.
 *
 * Responses are prefixed with '>' so they can be distinguished from the
 * regular ESP_LOG output that shares the same port.
 */
class Remote {
 public:
  /**
   * Start the remote control task.
   *
   * @return true if the task was created.
   */
  static bool start(void);

  /**
   * Write a line to the remote channel.
   *
   * @param[in] line Text to send, a newline is appended automatically.
   */
  static void reply(const char *line);

 private:
  static void task(void *param);
  static void handleLine(char *line);
};

}  // namespace Furble

#endif
