#include "sdkconfig.h"

#include "FurbleControl.h"
#include "FurbleRemote.h"

#if CONFIG_ESP_CONSOLE_SECONDARY_USB_SERIAL_JTAG

#include <fcntl.h>
#include <unistd.h>

#include <cstring>
#include <string>

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace Furble {

static const char *REMOTE_TAG = "remote";

/* USB Serial/JTAG is registered as /dev/secondary when
 * CONFIG_ESP_CONSOLE_SECONDARY_USB_SERIAL_JTAG is enabled. The console only
 * writes to it; console_read() uses the primary (UART0) port, so the RX FIFO
 * is exclusively available for our command channel. */
static constexpr const char *REMOTE_PORT = "/dev/secondary";
static constexpr size_t REMOTE_LINE_MAX = 128;
static constexpr uint32_t REMOTE_TASK_STACK = 4096;
static constexpr uint8_t REMOTE_TASK_PRIORITY = 3;
static constexpr TickType_t REMOTE_POLL_MS = 10;

/* How long the AF-ON button is held down for a 'FOCUS' tap.
 * Needs to be long enough for the camera to acquire focus, a short press is
 * simply ignored by the camera. */
static constexpr uint32_t REMOTE_FOCUS_HOLD_MS = 400;

/* How long a SHUTTER tap holds the shutter press before releasing. */
static constexpr TickType_t REMOTE_SHUTTER_HOLD_MS = pdMS_TO_TICKS(100);

static int s_RemoteFd = -1;

void Remote::reply(const char *line) {
  if (s_RemoteFd < 0 || line == NULL) {
    return;
  }
  char buf[REMOTE_LINE_MAX + 4];
  int len = snprintf(buf, sizeof(buf), ">%s\r\n", line);
  if (len > 0) {
    (void)write(s_RemoteFd, buf, static_cast<size_t>(len));
  }
}

void Remote::handleLine(char *line) {
  /* Translate a submit result into an honest response. A command is only
   * acknowledged when it was actually queued for dispatch (F-1). */
  auto replySubmit = [](Control::submit_result_t result) {
    switch (result) {
      case Control::SUBMIT_OK:
        reply("OK");
        break;
      case Control::SUBMIT_NOT_CONNECTED:
        reply("ERR not-connected");
        break;
      case Control::SUBMIT_BUSY:
        reply("ERR busy");
        break;
    }
  };

  /* Press-then-release with a hold between. Both halves must be accepted
   * before an OK is sent, otherwise the script is told a lie (F-1). */
  auto tap = [&replySubmit](Control &control, Control::cmd_t press,
                            Control::cmd_t release, TickType_t hold) {
    Control::submit_result_t result = control.submitIfActive(press);
    if (result != Control::SUBMIT_OK) {
      replySubmit(result);
      return;
    }
    vTaskDelay(hold);
    replySubmit(control.submitIfActive(release));
  };

  /* Trim CR/LF and leading spaces. */
  while (*line == ' ' || *line == '\r' || *line == '\n') {
    line++;
  }
  size_t len = strlen(line);
  while (len > 0 && (line[len - 1] == ' ' || line[len - 1] == '\r' || line[len - 1] == '\n')) {
    line[--len] = '\0';
  }
  if (len == 0) {
    return;
  }

  auto &control = Control::getInstance();

  if (strcmp(line, "PING") == 0) {
    reply("PONG");
  } else if (strcmp(line, "SHUTTER") == 0) {
    tap(control, Control::CMD_SHUTTER_PRESS, Control::CMD_SHUTTER_RELEASE,
        REMOTE_SHUTTER_HOLD_MS);
  } else if (strcmp(line, "SHUTTER_PRESS") == 0) {
    replySubmit(control.submitIfActive(Control::CMD_SHUTTER_PRESS));
  } else if (strcmp(line, "SHUTTER_RELEASE") == 0) {
    replySubmit(control.submitIfActive(Control::CMD_SHUTTER_RELEASE));
  } else if (strcmp(line, "SHUTTER_LOCK") == 0) {
    /* Same as the UI's shutterLock(): hold the shutter open. */
    replySubmit(control.submitIfActive(Control::CMD_SHUTTER_PRESS));
  } else if (strcmp(line, "SHUTTER_UNLOCK") == 0) {
    /* Same as the UI's shutterUnlock(): release the held shutter. */
    replySubmit(control.submitIfActive(Control::CMD_SHUTTER_RELEASE));
  } else if (strcmp(line, "FOCUS") == 0) {
    tap(control, Control::CMD_FOCUS_PRESS, Control::CMD_FOCUS_RELEASE,
        pdMS_TO_TICKS(REMOTE_FOCUS_HOLD_MS));
  } else if (strcmp(line, "FOCUS_PRESS") == 0) {
    replySubmit(control.submitIfActive(Control::CMD_FOCUS_PRESS));
  } else if (strcmp(line, "FOCUS_RELEASE") == 0) {
    replySubmit(control.submitIfActive(Control::CMD_FOCUS_RELEASE));
  } else if (strcmp(line, "STATUS") == 0) {
    /* Iterate a lock-safe copy: disconnect() clears m_Targets under m_Mutex,
     * so touching the live vector here used to risk iterator invalidation /
     * use-after-free (F-2). */
    Control::Snapshot snap = control.snapshot();
    char buf[REMOTE_LINE_MAX];
    snprintf(buf, sizeof(buf), "STATE %d TARGETS %u",
             static_cast<int>(snap.state),
             static_cast<unsigned int>(snap.targets.size()));
    reply(buf);
    for (const auto &target : snap.targets) {
      char cbuf[REMOTE_LINE_MAX];
      snprintf(cbuf, sizeof(cbuf), "CAM %s %s", target.name.c_str(),
               target.connected ? "connected" : "disconnected");
      reply(cbuf);
    }
  } else if (strcmp(line, "HELP") == 0) {
    reply("CMDS PING SHUTTER SHUTTER_PRESS SHUTTER_RELEASE SHUTTER_LOCK SHUTTER_UNLOCK "
          "FOCUS FOCUS_PRESS FOCUS_RELEASE STATUS HELP");
  } else {
    reply("ERR unknown");
  }
}

void Remote::task(void *param) {
  (void)param;

  s_RemoteFd = open(REMOTE_PORT, O_RDWR);
  if (s_RemoteFd < 0) {
    ESP_LOGE(REMOTE_TAG, "failed to open %s", REMOTE_PORT);
    vTaskDelete(NULL);
    return;
  }

  ESP_LOGI(REMOTE_TAG, "remote control ready on %s", REMOTE_PORT);

  char line[REMOTE_LINE_MAX];
  size_t used = 0;
  bool dropping = false;
  char chunk[REMOTE_LINE_MAX];

  while (true) {
    int rx = read(s_RemoteFd, chunk, sizeof(chunk) - 1);
    if (rx > 0) {
      for (int i = 0; i < rx; i++) {
        char c = chunk[i];
        if (c == '\n') {
          /* An overflowed line is discarded in full; only a clean, complete
           * line may ever reach the parser (F-4). */
          if (!dropping) {
            line[used] = '\0';
            handleLine(line);
          }
          used = 0;
          dropping = false;
        } else if (c != '\r') {
          if (dropping) {
            continue;
          }
          if (used < sizeof(line) - 1) {
            line[used++] = c;
          } else {
            dropping = true;
            used = 0;
          }
        }
      }
    } else {
      vTaskDelay(pdMS_TO_TICKS(REMOTE_POLL_MS));
    }
  }
}

bool Remote::start(void) {
  BaseType_t ret = xTaskCreate(task, "remote", REMOTE_TASK_STACK, NULL,
                               REMOTE_TASK_PRIORITY, NULL);
  if (ret != pdPASS) {
    ESP_LOGE(REMOTE_TAG, "failed to create task");
    return false;
  }
  return true;
}

}  // namespace Furble

#else  // !CONFIG_ESP_CONSOLE_SECONDARY_USB_SERIAL_JTAG

/* On boards without the USB Serial/JTAG secondary console there is no
 * /dev/secondary to open. Keep the public API linkable but inert, so those
 * builds do not emit a spurious ERROR at every boot (F-3). */
namespace Furble {

bool Remote::start(void) {
  return false;
}

void Remote::reply(const char *line) {
  (void)line;
}

}  // namespace Furble

#endif  // CONFIG_ESP_CONSOLE_SECONDARY_USB_SERIAL_JTAG
